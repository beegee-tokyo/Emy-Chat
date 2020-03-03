#ifdef USE_RFM95
#include "main.h"

int PIN_LORA_CS = 16;
int PIN_LORA_RST = -1;
int PIN_LORA_DIO0 = 26;
int PIN_LORA_DIO1 = 33;
int PIN_LORA_DIO2 = 32;

/** Lora class  */
SX1276 lora = new Module(PIN_LORA_CS, PIN_LORA_DIO0, PIN_LORA_RST, PIN_LORA_DIO1);

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

#define BROKEN_NET

/** Task to handle mesh */
TaskHandle_t meshTaskHandle = NULL;

/** Queue to handle cloud send requests */
volatile xQueueHandle meshMsgQueue;

/** Counter for CAD retry */
uint8_t channelFreeRetryNum = 0;

/** The Mesh node ID, created from ID of the nRF52 */
uint32_t deviceID;
/** The Mesh broadcast ID, created from node ID */
uint32_t broadcastID;

/** Map message buffer */
mapMsg syncMsg;

/** Max number of messages in the queue */
#define SEND_QUEUE_SIZE 2
/** Send buffer for SEND_QUEUE_SIZE messages */
dataMsg sendMsg[SEND_QUEUE_SIZE];
/** Message size buffer for SEND_QUEUE_SIZE messages */
uint8_t sendMsgSize[SEND_QUEUE_SIZE];
/** Queue to handle send requests */
volatile xQueueHandle sendQueue;
#ifdef ESP32
/** Mux used to enter critical code part (clear up queue content) */
portMUX_TYPE accessMsgQueue = portMUX_INITIALIZER_UNLOCKED;
#endif
/** Mux used to enter critical code part (access to node list) */
SemaphoreHandle_t accessNodeList;

/** LoRa TX package */
uint8_t txPckg[256];
/** Size of data package */
uint16_t txLen = 0;
/** LoRa RX buffer */
uint8_t rxBuffer[256];

/** Sync time for routing at start */
#define INIT_SYNCTIME 30000
/** Sync time for routing after mesh has settled */
#define DEFAULT_SYNCTIME 60000
/** Time to switch from INIT_SYNCTIME to DEFAULT_SYNCTIME */
#define SWITCH_SYNCTIME 300000
/** Sync time */
time_t syncTime = INIT_SYNCTIME;

typedef enum
{
	MESH_IDLE = 0, //!< The radio is idle
	MESH_RX,	   //!< The radio is in reception state
	MESH_TX,	   //!< The radio is in transmission state
	MESH_NOTIF	 //!< The radio is doing mesh notification
} meshRadioState_t;

/** Lora statemachine status */
meshRadioState_t loraState = MESH_IDLE;

/** Mesh callback variable */
static MeshEvents_t *_MeshEvents;

/** Lora OnReceive callback */
void OnReceive(void);

/** Read data from LoRa module */
void receiveData(void);

/** Number of nodes in the map */
int _numOfNodes = 0;

/** Flag if the nodes map has changed */
boolean nodesChanged = false;

/** Flag if RFM95 module has data */
boolean hasData = false;

/**
 * Initialize the Mesh network
 * @param events
 * 		Structure of event callbacks
 * @param numOfNodes
 * 		Number of nodes that the Mesh network can accept.
 */
void initMesh(MeshEvents_t *events, int numOfNodes)
{
	_MeshEvents = events;

	_numOfNodes = numOfNodes;

	// Prepare empty nodes map
	nodesMap = (nodesList *)malloc(_numOfNodes * sizeof(nodesList));

	if (nodesMap == NULL)
	{
		myLog_e("Could not allocate memory for nodes map");
	}
	else
	{
		myLog_d("Memory for nodes map is allocated");
	}
	memset(nodesMap, 0, _numOfNodes * sizeof(nodesList));

	// Prepare empty names map
	namesMap = (namesList *)malloc(_numOfNodes * sizeof(namesList));

	if (namesMap == NULL)
	{
		myLog_e("Could not allocate memory for names map");
	}
	else
	{
		myLog_d("Memory for names map is allocated");
	}
	memset(namesMap, 0, _numOfNodes * sizeof(namesList));

	// Create queue
	sendQueue = xQueueCreate(SEND_QUEUE_SIZE, sizeof(uint8_t));
	if (sendQueue == NULL)
	{
		myLog_e("Could not create send queue!");
	}
	else
	{
		myLog_d("Send queue created!");
	}
	// Create blocking semaphore for nodes list access
	accessNodeList = xSemaphoreCreateBinary();
	xSemaphoreGive(accessNodeList);

	// Create broadcast ID
	broadcastID = deviceID & 0xFFFFFF00;
	myLog_d("Broadcast ID is %08X", broadcastID);

	int state = lora.begin();
	if (state == ERR_NONE)
	{
		myLog_d("Lora init success!");
	}
	else
	{
		myLog_e("Lora init failed, code %d", state);
	}

	lora.standby();
	lora.setOutputPower(17);
	lora.setFrequency(RF_FREQUENCY / 1000000.0F);
	float bw = 125.0F;
	if (LORA_BANDWIDTH != 0)
	{
		bw = LORA_BANDWIDTH * 250.0F;
	}
	lora.setBandwidth(bw); // How to calculate from LORA_BANDWIDTH [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
	lora.setCodingRate(LORA_CODINGRATE+4);
	lora.setPreambleLength(LORA_PREAMBLE_LENGTH);
	lora.setSpreadingFactor(LORA_SPREADING_FACTOR);
	lora.setCRC(true);

	lora.setDio0Action(OnReceive);

	// Create message queue for LoRa
	meshMsgQueue = xQueueCreate(10, sizeof(uint8_t));
	if (meshMsgQueue == NULL)
	{
		myLog_e("Could not create LoRa message queue!");
	}
	else
	{
		myLog_d("LoRa message queue created!");
	}

	if (!xTaskCreate(meshTask, "MeshSync", 3096, NULL, 1, &meshTaskHandle))
	{
		myLog_e("Starting Mesh Sync Task failed");
	}
	else
	{
		myLog_d("Starting Mesh Sync Task success");
	}
}

/**
 * Task to handle the mesh
 * @param pvParameters
 * 		Unused task parameters
 */
void meshTask(void *pvParameters)
{
	// Queue variable to be sent to the task
	uint8_t queueIndex;

	time_t notifyTimer = millis() + syncTime;
	// time_t cleanTimer = millis();
	time_t checkSwitchSyncTime = millis();

	loraState = MESH_IDLE;
	// Start waiting for data package
	lora.standby();
	lora.startReceive();

	time_t txTimeout = millis();

	while (1)
	{
		if (hasData)
		{
			receiveData();
			hasData = false;
		}

		if (nodesChanged)
		{
			nodesChanged = false;
			if ((_MeshEvents != NULL) && (_MeshEvents->NodesListChanged != NULL))
			{
				_MeshEvents->NodesListChanged();
			}
		}
		// Time to sync the Mesh ???
		if ((millis() - notifyTimer) >= syncTime)
		{
			if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
			{
				myLog_v("Checking mesh map");
				if (!cleanMap())
				{
					syncTime = INIT_SYNCTIME;
					checkSwitchSyncTime = millis();
					if ((_MeshEvents != NULL) && (_MeshEvents->NodesListChanged != NULL))
					{
						_MeshEvents->NodesListChanged();
					}
				}
				myLog_d("Sending mesh map");
				syncMsg.from = deviceID;
				syncMsg.type = LORA_NODEMAP;
				memset(syncMsg.nodes, 0, 48 * 5);

				// Get sub nodes
				uint8_t subsLen = nodeMap(syncMsg.nodes);

				xSemaphoreGive(accessNodeList);

				if (subsLen != 0)
				{
					for (int idx = 0; idx < subsLen; idx++)
					{
						uint32_t checkNode = syncMsg.nodes[idx][0];
						checkNode |= syncMsg.nodes[idx][1] << 8;
						checkNode |= syncMsg.nodes[idx][2] << 16;
						checkNode |= syncMsg.nodes[idx][3] << 24;
					}
				}
				syncMsg.nodes[subsLen][0] = 0xAA;
				syncMsg.nodes[subsLen][1] = 0x55;
				syncMsg.nodes[subsLen][2] = 0x00;
				syncMsg.nodes[subsLen][3] = 0xFF;
				syncMsg.nodes[subsLen][4] = 0xAA;
				subsLen++;

				subsLen = MAP_HEADER_SIZE + (subsLen * 5);

				if (!addSendRequest((dataMsg *)&syncMsg, subsLen))
				{
					myLog_e("Cannot send map because send queue is full");
				}
				notifyTimer = millis();
			}
			else
			{
				myLog_e("Cannot access map for clean up and syncing");
			}
		}

		// Time to relax the syncing ???
		if (((millis() - checkSwitchSyncTime) >= SWITCH_SYNCTIME) && (syncTime != DEFAULT_SYNCTIME))
		{
			myLog_v("Switching sync time to DEFAULT_SYNCTIME");
			syncTime = DEFAULT_SYNCTIME;
			checkSwitchSyncTime = millis();
		}

		// Check if loraState is stuck in MESH_TX
		if ((loraState == MESH_TX) && ((millis() - txTimeout) > 2000))
		{
			lora.standby();
			lora.startReceive();
			loraState = MESH_IDLE;
			myLog_e("loraState stuck in TX for 2 seconds");
		}

		// Check if we have something in the queue
		if (xQueuePeek(sendQueue, &queueIndex, (TickType_t)10) == pdTRUE)
		{
			if (loraState != MESH_TX)
			{
// #ifdef ESP32
// 				portENTER_CRITICAL(&accessMsgQueue);
// #else
// 				taskENTER_CRITICAL();
// #endif
// 				txLen = sendMsgSize[queueIndex];
// 				memset(txPckg, 0, 256);
// 				memcpy(txPckg, &sendMsg[queueIndex].mark1, txLen);

				// Check if the channel is free
				boolean channelAvailable = true;
				time_t cadTimeout = millis();
				while (lora.scanChannel() != CHANNEL_FREE)
				{
					if ((millis() - cadTimeout) > 5000)
					{
						myLog_e("CAD failed after 5 seconds");
						channelAvailable = false;
					}
				}
				
				if (channelAvailable && (xQueueReceive(sendQueue, &queueIndex, portMAX_DELAY) == pdTRUE))
				{
#ifdef ESP32
					portENTER_CRITICAL(&accessMsgQueue);
#else
					taskENTER_CRITICAL();
#endif
					txLen = sendMsgSize[queueIndex];
					memset(txPckg, 0, 256);
					memcpy(txPckg, &sendMsg[queueIndex].mark1, txLen);

					sendMsg[queueIndex].type = 0;
#ifdef ESP32
					portEXIT_CRITICAL(&accessMsgQueue);
#else
					taskEXIT_CRITICAL();
#endif

					myLog_d("Sending msg #%d with len %d", queueIndex, txLen);

					loraState = MESH_TX;

					lora.transmit(txPckg, txLen);
					lora.standby();
					lora.startReceive();
					loraState = MESH_IDLE;
				}
// 				else
// 				{
// #ifdef ESP32
// 					portEXIT_CRITICAL(&accessMsgQueue);
// #else
// 					taskEXIT_CRITICAL();
// #endif
// 				}
			}
		}

		// Enable a task switch
		delay(100);
	}
}

/**
 * Callback after a LoRa package was received
 * @param packetSize
 * 			Length of received data
 */
// void OnReceive(int packetSize)
void OnReceive(void)
{
	if (!enableInterrupt)
	{
		return;
	}
	hasData = true;
}

/**
 * Callback after a LoRa package was received
 * @param packetSize
 * 			Length of received data
 */
void receiveData(void)
{
	// disable the interrupt service routine while processing the data
	enableInterrupt = false;

	// Read received data as byte array
	uint16_t rxSize = lora.getPacketLength(true);

	int state = lora.readData(rxBuffer, rxSize);
	if (state != ERR_NONE)
	{
		myLog_e("Read data error %d", state);
		lora.standby();
		lora.startReceive();
		enableInterrupt = true;
		return;
	}

	if (rxSize < 256)
	{
		// Make sure the data is null terminated
		rxBuffer[rxSize] = 0;
	}

	uint16_t tempSize = rxSize;

	delay(1);

	loraState = MESH_IDLE;

	myLog_v("OnRxDone");

	int16_t rxRssi = lora.getRSSI();
	int8_t rxSnr = lora.getSNR();

	myLog_d("LoRa Packet received size:%d, rssi:%d, snr:%d", rxSize, rxRssi, rxSnr);
#if MYLOG_LOG_LEVEL == MYLOG_LOG_LEVEL_VERBOSE
	for (int idx = 0; idx < rxSize; idx++)
	{
		Serial.printf(" %02X", rxBuffer[idx]);
	}
	Serial.println("");
#endif

	// Restart listening
	enableInterrupt = true;
	lora.standby();
	lora.startReceive();

	// Check the received data
	if ((rxBuffer[0] == 'L') && (rxBuffer[1] == 'o') && (rxBuffer[2] == 'R'))
	{
		// Valid Mesh data received
		mapMsg *thisMsg = (mapMsg *)rxBuffer;
		dataMsg *thisDataMsg = (dataMsg *)rxBuffer;

		if (thisMsg->type == LORA_NODEMAP)
		{
			/// \todo for debug make some nodes unreachable
#ifdef BROKEN_NET
			switch (deviceID)
			{
			case 0x1E2F8C8F:
				if ((thisMsg->from == 0x2DDF3A8F) || (thisMsg->from == 0xFBAFD33E))
				{
				}
				else
				{
					myLog_d("0x1E2F8C8F connects only to 0x2DDF3A8F & 0xFBAFD33E");
					return;
				}
				break;
			case 0x2DDF3A8F:
				if ((thisMsg->from == 0xBF6CED4E) || (thisMsg->from == 0x1E2F8C8F))
				{
				}
				else
				{
					myLog_d("0x2DDF3A8F connects only to 0x1E2F8C8F & 0xBF6CED4E");
					return;
				}
				break;
			case 0xFBAFD33E:
				if ((thisMsg->from == 0x2DDF3A8F) || (thisMsg->from == 0x1E2F8C8F))
				{
				}
				else
				{
					myLog_d("0xFBAFD33E connects only to 0x1E2F8C8F & 0x2DDF3A8F");
					return;
				}
				break;
			case 0xBF6CED4E:
				if ((thisMsg->from == 0xBF6C660E) || (thisMsg->from == 0x1E2F8C8F) || (thisMsg->from == 0xFBAFD33E))
				{
					myLog_d("No connection from 0xBF6CED4E to 0x1E2F8C8F & 0xBF6C660E & 0xFBAFD33E");
					return;
				}
				break;
			case 0xBF6C660E:
				if ((thisMsg->from == 0xBF6CED4E) || (thisMsg->from == 0x1E2F8C8F) || (thisMsg->from == 0x2DDF3A8F) || (thisMsg->from == 0xFBAFD33E))
				{
					myLog_d("No connection from 0xBF6C660E to 0x1E2F8C8F & 0xBF6CED4E & 0x2DDF3A8F & 0xFBAFD33E");
					return;
				}
				break;
			default:
				if ((thisMsg->from == 0x2DDF3A8F) || (thisMsg->from == 0x1E2F8C8F) || (thisMsg->from == 0xFBAFD33E))
				{
					myLog_d("No connection from 0x1E2F8C8F & 0x2DDF3A8F & 0xFBAFD33E to any other node");
					return;
				}
				break;
			}
#endif
			myLog_d("Got map message");
			// Mapping received
			uint8_t subsSize = tempSize - MAP_HEADER_SIZE;
			uint8_t numSubs = subsSize / 5;

			// Serial.println("********************************");
			// for (int idx = 0; idx < tempSize; idx++)
			// {
			// 	Serial.printf("%02X ", rxBuffer[idx]);
			// }
			// Serial.println("");
			// Serial.printf("subsSize %d -> # subs %d\n", subsSize, subsSize / 5);
			// Serial.println("********************************");

			// Check if end marker is in the message
			if ((thisMsg->nodes[numSubs - 1][0] != 0xAA) ||
				(thisMsg->nodes[numSubs - 1][1] != 0x55) ||
				(thisMsg->nodes[numSubs - 1][2] != 0x00) ||
				(thisMsg->nodes[numSubs - 1][3] != 0xFF) ||
				(thisMsg->nodes[numSubs - 1][4] != 0xAA))
			{
				myLog_w("Invalid map, end marker is missing from %08X", thisMsg->from);
				xSemaphoreGive(accessNodeList);
				return;
			}
			if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
			{
				nodesChanged = addNode(thisMsg->from, 0, 0);

				// Remove nodes that use sending node as hop
				clearSubs(thisMsg->from);

				myLog_v("From %08X", thisMsg->from);
				myLog_v("Dest %08X", thisMsg->dest);

				if (subsSize != 0)
				{
					// Mapping contains subs

					myLog_v("Msg size %d", tempSize);
					myLog_v("#subs %d", numSubs);

					// Serial.println("++++++++++++++++++++++++++++");
					// Serial.printf("From %08X Dest %08X #Subs %d\n", thisMsg->from, thisMsg->dest, numSubs);
					// for (int idx = 0; idx < numSubs; idx++)
					// {
					// 	uint32_t subId = (uint32_t)thisMsg->nodes[idx][0];
					// 	subId += (uint32_t)thisMsg->nodes[idx][1] << 8;
					// 	subId += (uint32_t)thisMsg->nodes[idx][2] << 16;
					// 	subId += (uint32_t)thisMsg->nodes[idx][3] << 24;
					// 	uint8_t hops = thisMsg->nodes[idx][4];
					// 	Serial.printf("ID: %08X numHops: %d\n", subId, hops);
					// }
					// Serial.println("++++++++++++++++++++++++++++");

					for (int idx = 0; idx < numSubs - 1; idx++)
					{
						uint32_t subId = (uint32_t)thisMsg->nodes[idx][0];
						subId += (uint32_t)thisMsg->nodes[idx][1] << 8;
						subId += (uint32_t)thisMsg->nodes[idx][2] << 16;
						subId += (uint32_t)thisMsg->nodes[idx][3] << 24;
						uint8_t hops = thisMsg->nodes[idx][4];
						if (subId != deviceID)
						{
							nodesChanged |= addNode(subId, thisMsg->from, hops + 1);
							myLog_v("Subs %08X", subId);
						}
					}
				}
				xSemaphoreGive(accessNodeList);
			}
			else
			{
				myLog_e("Could not access map to add node");
			}
		}
		else if (thisDataMsg->type == LORA_DIRECT)
		{
			if (thisDataMsg->dest == deviceID)
			{
				// Message is for us, call user callback to handle the data
				myLog_d("Got data message type %c >%s<", thisDataMsg->data[0], (char *)&thisDataMsg->data[1]);
				if ((_MeshEvents != NULL) && (_MeshEvents->DataAvailable != NULL))
				{
					_MeshEvents->DataAvailable(thisDataMsg->orig, thisDataMsg->data, tempSize - 12, rxRssi, rxSnr);
				}
			}
			else
			{
				// Message is not for us
			}
		}
		else if (thisDataMsg->type == LORA_FORWARD)
		{
			if (thisDataMsg->dest == deviceID)
			{
				// Message is for sub node, forward the message
				nodesList route;
				if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
				{
					if (getRoute(thisDataMsg->from, &route))
					{
						// We found a route, send package to next hop
						if (route.firstHop == 0)
						{
							myLog_i("Route for %lX is direct", route.nodeId);
							// Destination is a direct
							thisDataMsg->dest = thisDataMsg->from;
							thisDataMsg->from = deviceID;
							thisDataMsg->type = LORA_DIRECT;
						}
						else
						{
							myLog_i("Route for %lX is to %lX", route.nodeId, route.firstHop);
							// Destination is a sub
							thisDataMsg->dest = route.firstHop;
							thisDataMsg->type = LORA_FORWARD;
						}

						// Put message into send queue
						if (!addSendRequest(thisDataMsg, tempSize))
						{
							myLog_e("Cannot forward message because send queue is full");
						}
					}
					else
					{
						myLog_e("No route found for %lX", thisMsg->from);
					}
					xSemaphoreGive(accessNodeList);
				}
				else
				{
					myLog_e("Could not access map to forward package");
				}
			}
			else
			{
				// Message is not for us
			}
		}
		else if (thisDataMsg->type == LORA_BROADCAST)
		{
			// This is a broadcast. Forward to all direct nodes, but not to the one who sent it
			myLog_d("Handling broadcast with ID %08X from %08X", thisDataMsg->dest, thisDataMsg->from);
			// Check if this broadcast is coming from ourself
			if ((thisDataMsg->dest & 0xFFFFFF00) == (deviceID & 0xFFFFFF00))
			{
				myLog_w("We received our own broadcast, dismissing it");
				return;
			}
			// Check if we handled this broadcast already
			if (isOldBroadcast(thisDataMsg->dest))
			{
				myLog_w("Got an old broadcast, dismissing it");
				return;
			}

			// Put broadcast into send queue
			if (!addSendRequest(thisDataMsg, tempSize))
			{
				myLog_e("Cannot forward broadcast because send queue is full");
			}

			// This is a broadcast, call user callback to handle the data
			myLog_d("Got data broadcast %s", (char *)thisDataMsg->data);
			if ((_MeshEvents != NULL) && (_MeshEvents->DataAvailable != NULL))
			{
				_MeshEvents->DataAvailable(thisDataMsg->from, thisDataMsg->data, tempSize - 12, rxRssi, rxSnr);
			}
		}
	}
	else
	{
		myLog_e("Invalid package");
		for (int idx = 0; idx < tempSize; idx++)
		{
			Serial.printf("%02X ", rxBuffer[idx]);
		}
		Serial.println("");
	}
}

/**
 * Add a data package to the queue
 * @param package
 * 			dataPckg * to the package data
 * @param msgSize
 * 			Size of the data package
 * @return result
 * 			TRUE if task could be added to queue
 * 			FALSE if queue is full or not initialized 
 */
bool addSendRequest(dataMsg *package, uint8_t msgSize)
{
	if (sendQueue != NULL)
	{
#ifdef ESP32
		portENTER_CRITICAL(&accessMsgQueue);
#else
		taskENTER_CRITICAL();
#endif
		// Find unused entry in queue list
		int next = SEND_QUEUE_SIZE;
		for (int idx = 0; idx < SEND_QUEUE_SIZE; idx++)
		{
			if (sendMsg[idx].type == LORA_INVALID)
			{
				next = idx;
				break;
			}
		}

		if (next != SEND_QUEUE_SIZE)
		{
			// Found an empty entry!
			memcpy(&sendMsg[next], package, msgSize);
			sendMsgSize[next] = msgSize;

			myLog_d("Queued msg #%d with len %d", next, msgSize);

			// Try to add to cloudTaskQueue
			if (xQueueSend(sendQueue, &next, (TickType_t)1000) != pdTRUE)
			{
				myLog_e("Send queue is busy");
#ifdef ESP32
				portEXIT_CRITICAL(&accessMsgQueue);
#else
				taskEXIT_CRITICAL();
#endif
				return false;
			}
			else
			{
				myLog_v("Send request queued:");
#ifdef ESP32
				portEXIT_CRITICAL(&accessMsgQueue);
#else
				taskEXIT_CRITICAL();
#endif
				return true;
			}
		}
		else
		{
			myLog_e("Send queue is full");
			// Queue is already full!
#ifdef ESP32
			portEXIT_CRITICAL(&accessMsgQueue);
#else
			taskEXIT_CRITICAL();
#endif
			return false;
		}
	}
	else
	{
		myLog_e("Send queue not yet initialized");
		return false;
	}
}
#endif

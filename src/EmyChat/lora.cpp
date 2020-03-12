#include "main.h"

#ifdef USE_RFM95
///////////////////////////////////////////
// Pin definitions are in mesh_rfm95.cpp
///////////////////////////////////////////
#else
/** HW configuration structure for the LoRa library */
extern hw_config _hwConfig;

#ifdef ESP32
#ifdef IS_WROVER
// ESP32 Wrover - SX126x pin configuration
/** LORA RESET */
int PIN_LORA_RESET = 4;
/** LORA DIO_1 */
int PIN_LORA_DIO_1 = 21;
/** LORA SPI BUSY */
int PIN_LORA_BUSY = 22;
/** LORA SPI CS */
int PIN_LORA_NSS = 5;
/** LORA SPI CLK */
int PIN_LORA_SCLK = 18;
/** LORA SPI MISO */
int PIN_LORA_MISO = 19;
/** LORA SPI MOSI */
int PIN_LORA_MOSI = 23;
/** LORA ANTENNA TX ENABLE */
int RADIO_TXEN = 26;
/** LORA ANTENNA RX ENABLE */
int RADIO_RXEN = 27;

#define LED_BUILTIN 15
#else
// ESP32 Feather - SX126x pin configuration
/** LORA RESET */
int PIN_LORA_RESET = 32;
/** LORA DIO_1 */
int PIN_LORA_DIO_1 = 14;
/** LORA SPI BUSY */
int PIN_LORA_BUSY = 27;
/** LORA SPI CS */
int PIN_LORA_NSS = 33;
/** LORA SPI CLK */
int PIN_LORA_SCLK = SCK;
/** LORA SPI MISO */
int PIN_LORA_MISO = MISO;
/** LORA SPI MOSI */
int PIN_LORA_MOSI = MOSI;
/** LORA ANTENNA TX ENABLE */
int RADIO_TXEN = -1;
/** LORA ANTENNA RX ENABLE */
int RADIO_RXEN = -1;
#endif
#ifdef RED_ESP
#undef LED_BUILTIN
#define LED_BUILTIN 16
#endif
#endif
#ifdef NRF52
#ifdef ADAFRUIT
/** LORA RESET */
int PIN_LORA_RESET = 30;
/** LORA DIO_1 */
int PIN_LORA_DIO_1 = 27;
/** LORA SPI BUSY */
int PIN_LORA_BUSY = 7;
/** LORA SPI CS */
int PIN_LORA_NSS = 11;
/** LORA SPI CLK */
int PIN_LORA_SCLK = SCK;
/** LORA SPI MISO */
int PIN_LORA_MISO = MISO;
/** LORA SPI MOSI */
int PIN_LORA_MOSI = MOSI;
/** LORA ANTENNA TX ENABLE */
int RADIO_TXEN = -1;
/** LORA ANTENNA RX ENABLE */
int RADIO_RXEN = -1;
// Singleton for SPI connection to the LoRa chip
SPIClass SPI_LORA(NRF_SPIM1, MISO, SCK, MOSI);
#else
// Singleton for SPI connection to the LoRa chip
SPIClass SPI_LORA(NRF_SPIM2, MISO, SCK, MOSI);
#undef LED_BUILTIN
#define LED_BUILTIN 22
#endif
#endif
#endif

/** Callback if data was received over LoRa SX1262 */
void OnLoraData(uint32_t fromID, uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr);
/** Callback if data was received over LoRa RFM95 */
void OnReceive(int packetSize);
/** Callback if Mesh map changed */
void onNodesListChange(void);

/** Structure with Mesh event callbacks */
static MeshEvents_t MeshEvents;

/** Buffer for incoming LoRa data */
char loraRX[256];
/** NodeID of the LoRa sender */
uint32_t loraSenderID;
/** Size of received data */
uint8_t loraDataSize;

/** Flag if the Mesh map has changed */
boolean nodesListChanged = false;

/** Flag if a new message arrived */
boolean newLoRaData = false;

/** Node ID of the selected receiver node */
uint32_t nodeId[48];
/** First hop ID of the selected receiver node */
uint32_t firstHop[48];
/** Number of hops to the selected receiver node */
uint8_t numHops[48];
/** Number of nodes in the map */
uint8_t numElements;

/**
 * Initialize the LoRa HW
 * @return bool
 * 		True if initialization was successfull, false if not
 */
bool initLoRa(void)
{
	bool initResult = true;

	// Initialize the mesh node name list
	initNodeNames(MAX_NODES);

#ifdef USE_RFM95
////////////////////////////
// Initialization for RFM95
////////////////////////////
// Done in mesh_rfm95.cpp
#else
////////////////////////////
// Initialization for SX126
////////////////////////////
#if defined(ESP32) || defined(ADAFRUIT)
	// Define the HW configuration between MCU and SX126x
	_hwConfig.CHIP_TYPE = SX1262_CHIP;		   // eByte E22 module with an SX1262
	_hwConfig.PIN_LORA_RESET = PIN_LORA_RESET; // LORA RESET
	_hwConfig.PIN_LORA_NSS = PIN_LORA_NSS;	 // LORA SPI CS
	_hwConfig.PIN_LORA_SCLK = PIN_LORA_SCLK;   // LORA SPI CLK
	_hwConfig.PIN_LORA_MISO = PIN_LORA_MISO;   // LORA SPI MISO
	_hwConfig.PIN_LORA_DIO_1 = PIN_LORA_DIO_1; // LORA DIO_1
	_hwConfig.PIN_LORA_BUSY = PIN_LORA_BUSY;   // LORA SPI BUSY
	_hwConfig.PIN_LORA_MOSI = PIN_LORA_MOSI;   // LORA SPI MOSI
	_hwConfig.RADIO_TXEN = RADIO_TXEN;		   // LORA ANTENNA TX ENABLE
	_hwConfig.RADIO_RXEN = RADIO_RXEN;		   // LORA ANTENNA RX ENABLE
	_hwConfig.USE_DIO2_ANT_SWITCH = true;	  // Example uses an eByte E22 module which uses RXEN and TXEN pins as antenna control
	_hwConfig.USE_DIO3_TCXO = true;			   // Example uses an eByte E22 module which uses DIO3 to control oscillator voltage
	_hwConfig.USE_DIO3_ANT_SWITCH = false;	 // Only Insight ISP4520 module uses DIO3 as antenna control

	if (lora_hardware_init(_hwConfig) != 0)
	{
		myLog_e("Error in hardware init");
	}
#else // ISP4520
	if (lora_isp4520_init(SX1262) != 0)
	{
		myLog_e("Error in hardware init");
		initResult = false;
	}
#endif
#endif

	MeshEvents.DataAvailable = OnLoraData;
	MeshEvents.NodesListChanged = onNodesListChange;

	// Initialize the LoRa Mesh
	// * events, number of nodes, frequency, TX power
#ifdef ESP32
	initMesh(&MeshEvents, MAX_NODES);
#else
	initMesh(&MeshEvents, MAX_NODES);
#endif
	return initResult;
}

/**
 * Handle LoRa data
 */
void handleLoraData(void)
{
	// char *dispNodeName = getNodeName(loraSenderID);
	char tempName[17];

	myLog_v("Got type 0x%0X", loraRX[0]);
	switch (loraRX[0])
	{
	case CHAT_TYPE: // Chat message
		// BLE output
		sendBleData(loraSenderID, CHAT_TYPE, &loraRX[1], loraDataSize - 1);

		// Console output
		sendConsoleData(loraSenderID, CHAT_TYPE, &loraRX[1], loraDataSize - 1);

#ifdef HAS_DISPLAY
		// Update display
		dispWriteHeader();
		// // Display is very small, remove @ tags
		if (loraRX[1] == '@')
		{
			// Remove the @ tag
			int txtStart;
			for (txtStart = 1; txtStart < loraDataSize - 1; txtStart++)
			{
				if (loraRX[txtStart] == 0x20)
				{
					break;
				}
			}
			loraRX[1] = '!';
			memcpy(&loraRX[2], &loraRX[txtStart + 1], loraDataSize - 1);
		}

		dispAddLine(&loraRX[1]);
		dispShow();
#endif
		break;
	case LOCATION_TYPE: // Location message
		// BLE output
		sendBleData(loraSenderID, LOCATION_TYPE, &loraRX[1], loraDataSize - 1);

		/// \todo Is the location message required to show on the console?
		// Console output
		// sendConsoleData(loraSenderID, LOCATION_TYPE, &loraRX[1], loraDataSize - 1);
		break;
	case NAME_TYPE: // Nickname message
		snprintf(tempName, 17, &loraRX[1]);
		// Add name to local names list
		addNodeName(loraSenderID, tempName);
		// Send name of node to the BLE app
		sendBleData(loraSenderID, NAME_TYPE, &loraRX[1], loraDataSize - 1);
		break;
	}
}

/**
 * Handle mesh nodes changes
 */
void handleNodesListChanges(void)
{
#ifdef HAS_DISPLAY
	// Update display
	dispWriteHeader();
#endif
	// Update BLE client
	sendBleData(0, MAP_TYPE, NULL, 0);

	// Send my name to update new nodes with the name
	if (userName[0] != 0)
	{
		char nameMsg[64];
		size_t dataLen = snprintf(nameMsg, 64, "%s", userName);
		sendLoRaData(getNextBroadcastID(), NAME_TYPE, nameMsg, dataLen);
	}

	/// \todo should this been shown in console or as log output only
	myLog_d("Nodes map changed");
	if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
	{
		numElements = numOfNodes();
		for (int idx = 0; idx < numElements; idx++)
		{
			getNode(idx, nodeId[idx], firstHop[idx], numHops[idx]);
		}
		// Release access to nodes list
		xSemaphoreGive(accessNodeList);
		// Display the nodes
		myLog_d("%d nodes in the map", numElements + 1);
		myLog_d("Node #01 id: %08X", deviceID);
		for (int idx = 0; idx < numElements; idx++)
		{
			if (firstHop[idx] == 0)
			{
				myLog_d("Node #%02d id: %08X direct", idx + 2, nodeId[idx]);
			}
			else
			{
				myLog_d("Node #%02d id: %08X first hop %08X #hops %d", idx + 2, nodeId[idx], firstHop[idx], numHops[idx]);
			}
		}
	}
	else
	{
		myLog_e("Could not access the nodes list");
	}
}

/**
 * Send data over LoRa
 * @param receiver
 * 			Node ID the data was received from
 * @param type
 * 			Type of data (chat direct or broadcast, location, name, node map)
 * @param data
 * 			Data buffer
 * @param len
 * 			Size of data buffer
 */
void sendLoRaData(uint32_t receiver, uint8_t type, char *data, size_t len)
{
	myLog_v("Got data %s", data);
	/** Structure for outgoing data */
	dataMsg outData;

	nodesList routeToNode;

	// Release access to nodes list
	xSemaphoreGive(accessNodeList);

	// Prepare data
	outData.mark1 = 'L';
	outData.mark2 = 'o';
	outData.mark3 = 'R';

	uint32_t nodeIdFromName = 0;

	switch (type)
	{
	case LOCATION_TYPE:
	case NAME_TYPE:
		// Location or Name message, send as broadcast
		outData.dest = getNextBroadcastID();
		outData.from = outData.orig = deviceID;
		outData.type = LORA_BROADCAST;
		myLog_d("Sending message type 0x%0X with content %s", type, data);
		break;
	default:
		// Assume chat type
		if (data[0] == '@')
		{
			myLog_v("Found @, try to get nodeID");
			// Try to get the node ID from the @ name
			String tempString = String(data);
			int endIdx = tempString.indexOf(" ");
			tempString = tempString.substring(1, endIdx);
			myLog_v("Found @ name '%s'", tempString.c_str());
			nodeIdFromName = getNodeIdFromName((char *)tempString.c_str());
			if (nodeIdFromName != 0)
			{
				// Found the name in the list, use the matching node ID
				outData.dest = nodeIdFromName;
				myLog_d("Found nodeID %08X from @ name %s", nodeIdFromName, tempString.c_str());
			}
			else
			{
				// Assume the @ name is the node ID
				// Not a registered name, try to get the number
				sscanf(tempString.c_str(), "%08X", &nodeIdFromName);
				if (nodeIdFromName != 0)
				{
					myLog_d("Got nodeID from sscanf '%08X'", nodeIdFromName);
				}
				else
				{
					myLog_d("No valid nodeID from sscanf");
				}
			}
		}
		if (nodeIdFromName == 0)
		{
			// No nodeID found, send as broadcast
			outData.dest = getNextBroadcastID();
			outData.from = outData.orig = deviceID;
			outData.type = LORA_BROADCAST;
			myLog_d("Sending chat as broadcast");
		}
		else
		{
			if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
			{
				getRoute(nodeIdFromName, &routeToNode);
				xSemaphoreGive(accessNodeList);
				if (routeToNode.firstHop != 0)
				{
					outData.dest = routeToNode.firstHop;
					outData.from = routeToNode.nodeId;
					outData.orig = deviceID;
					outData.type = LORA_FORWARD;
					myLog_d("Queuing msg to hop to %08X over %08X", outData.from, outData.dest);
				}
				else
				{
					outData.dest = routeToNode.nodeId;
					outData.from = outData.orig = deviceID;
					outData.type = LORA_DIRECT;
					myLog_d("Queuing msg direct to %08X", outData.dest);
				}
			}
			else
			{
				myLog_e("Could not access the nodes list, sending as broadcast");
				outData.dest = getNextBroadcastID();
				outData.from = outData.orig = deviceID;
				outData.type = LORA_BROADCAST;
			}
		}
		break;
	}
	outData.data[0] = type;
	memcpy(&outData.data[1], data, len);
	int dataLen = DATA_HEADER_SIZE + len + 1;
	// Add package to send queue
	if (!addSendRequest(&outData, dataLen))
	{
		myLog_e("Sending package failed");
	}
}

/**
 * Callback after a LoRa package was received
 * @param rxPayload
 * 			Pointer to the received data
 * @param rxSize
 * 			Length of the received package
 * @param rxRssi
 * 			Signal strength while the package was received
 * @param rxSnr
 * 			Signal to noise ratio while the package was received
 */
void OnLoraData(uint32_t fromID, uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr)
{
	if (!newLoRaData)
	{
		memcpy(loraRX, rxPayload, rxSize);
		loraSenderID = fromID;
		loraRX[rxSize] = 0;
		loraDataSize = rxSize;
		newLoRaData = true;
	}
	else
	{
		myLog_e("New data arrived before old data was processed");
	}
#if defined(IS_WROVER) || defined(RED_ESP)
	digitalWrite(LED_BUILTIN, LOW);
#else
	digitalWrite(LED_BUILTIN, HIGH);
#endif
#ifdef ESP32
	ledOffTick.detach();
	ledOffTick.once(1, ledOff);
#else
	timer.attachInterrupt(&ledOff, 1000 * 1000); // microseconds
#endif
}

/**
 * Callback after the nodes list changed
 */
void onNodesListChange(void)
{
	nodesListChanged = true;
}
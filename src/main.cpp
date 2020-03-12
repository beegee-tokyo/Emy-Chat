#include "main.h"

#ifdef IS_WROVER
#define LED_BUILTIN 15
#endif
#ifdef RED_ESP
#undef LED_BUILTIN
#define LED_BUILTIN 16
#endif

#ifdef NRF52
#if not defined ADAFRUIT
#undef LED_BUILTIN
#define LED_BUILTIN 22
#endif
#endif

#ifdef ESP32
/** Timer for the LED control */
Ticker ledOffTick;
#else
#define nrf_timer_num (1)
#define cc_channel_num (0)
/** Timer for the LED control */
TimerClass timer(nrf_timer_num, cc_channel_num);
#endif

/** Timer to send data frequently to random nodes */
time_t sendRandom;

/** User alias (nickname) */
char userName[17] = {0};

/** Flag for a new BLE client connection */
bool newBLEConnection = false;

/** 
 * Switch off the LED
 * Triggered by a timer
 */
void ledOff(void)
{
#if defined(IS_WROVER) || defined(RED_ESP)
	digitalWrite(LED_BUILTIN, HIGH);
#else
	digitalWrite(LED_BUILTIN, LOW);
#endif
}

/**
 * Arduino setup
 */
void setup()
{
#ifdef NRF52
	pinMode(17, OUTPUT);
	digitalWrite(17, HIGH);
#endif

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	// Start Serial
	Serial.begin(115200);
#ifdef ADAFRUIT
	// The serial interface of the nRF52's seems to need some time before it is ready
	delay(500);
#endif

	// Create node ID
#ifdef ESP32
	uint8_t deviceMac[8];
#ifdef USE_RFM95
	uint64_t uniqueId = ESP.getEfuseMac();
	// Using ESP32 MAC (48 bytes only, so upper 2 bytes will be 0)
	deviceMac[7] = (uint8_t)(uniqueId >> 56);
	deviceMac[6] = (uint8_t)(uniqueId >> 48);
	deviceMac[5] = (uint8_t)(uniqueId >> 40);
	deviceMac[4] = (uint8_t)(uniqueId >> 32);
	deviceMac[3] = (uint8_t)(uniqueId >> 24);
	deviceMac[2] = (uint8_t)(uniqueId >> 16);
	deviceMac[1] = (uint8_t)(uniqueId >> 8);
	deviceMac[0] = (uint8_t)(uniqueId);
	// deviceID = (uint32_t)(ESP.getEfuseMac() & 0x00000000ffffffff) + 2;
#else
	BoardGetUniqueId(deviceMac);
#endif
	deviceID += (uint32_t)deviceMac[2] << 24; // Last byte of the OUI
	deviceID += (uint32_t)deviceMac[3] << 16; // High octet of NIC
	deviceID += (uint32_t)deviceMac[4] << 8;  // Middle octet of NIC
	deviceID += (uint32_t)deviceMac[5];		  // Low octet of NIC
	deviceID += 2;
#else
	// Get nRF52 MAC address
	deviceID = *((uint32_t *)(0x100000a4));
#endif
	myLog_n("Mesh NodeId = %08X", deviceID);

	// Get username if saved
	getNickname(userName);
#ifdef HAS_DISPLAY
	// Initialize Display
	initDisplay();
#endif

	// Initialize BLE
	initBLE();

	// Initialize the LoRa
	if (!initLoRa())
	{
		myLog_e("LoRa initialization failed!");
	}

	// Initialize timer for random data sending
	sendRandom = millis();

	Serial.println("\n\n##################################");
	Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
	Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
	Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
	Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
	Serial.println("##################################\n\n");
}

/**
 * Arduino loop
 */
void loop()
{
	delay(100);

	// Handle LoRa data
	if (newLoRaData)
	{
		handleLoraData();
		newLoRaData = false;
	}

	// Handle Mesh nodes list changes
	if (nodesListChanged)
	{
		// Nodes list changed, update display and report it
		handleNodesListChanges();
		nodesListChanged = false;
	}

	// Handle Console input
	if (Serial.available())
	{
		handleConsoleData();
	}

	// Handle new BLE client
	if (bleUARTnotifyEnabled && !newBLEConnection)
	{
		newBLEConnection = true;
		// Tell the app your username
		if (userName[0] != 0)
		{
			sendBleData(0, SET_NAME_TYPE, userName, strlen(userName));
		}
		else
		{
			sprintf(userName, "<%08X>", deviceID);
			sendBleData(0, SET_NAME_TYPE, userName, strlen(userName));
			userName[0] = 0;
		}

		// Send the mesh and names info
		sendBleData(0, MAP_TYPE, NULL, 0);
	}

	// Handle BLE disconnect
	if (!bleUARTnotifyEnabled && newBLEConnection)
	{
		newBLEConnection = false;
	}

	// Handle BLE data
	if (bleUartAvailable())
	{
		handleBleData();
	}

	/// \todo Random sending is for testing only!
	if ((millis() - sendRandom) >= 60000)
	{
		sendRandom = millis();

		/** Structure for outgoing data */
		dataMsg outData;
		if (random(0, 10) > 7)
		{
			// Send a broadcast
			char testMsg[64] = {0};
			int dataLen;
			if (userName[0] != 0)
			{
				dataLen = DATA_HEADER_SIZE + sprintf((char *)testMsg, "Hello all from %s", userName);
			}
			else
			{
				dataLen = DATA_HEADER_SIZE + sprintf((char *)testMsg, "Hello all from %08lX", deviceID);
			}
			sendLoRaData(getNextBroadcastID(), CHAT_TYPE, testMsg, dataLen);
		}
		else
		{
			// Send a direct package
			if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
			{
				uint8_t numElements = numOfNodes();
				// nodesList routeToNode;
				uint32_t nodeId;
				uint32_t firstHop;
				uint8_t numHops;
				if (numOfNodes() >= 2)
				{
					getNode(random(0, numElements), nodeId, firstHop, numHops);

					char testMsg[64] = {0};
					int dataLen;
					if (userName[0] != 0)
					{
						dataLen = DATA_HEADER_SIZE + sprintf((char *)testMsg, "@%08X Hello from %s", nodeId, userName);
					}
					else
					{
						dataLen = DATA_HEADER_SIZE + sprintf((char *)testMsg, "@%08X Hello from %08X", nodeId, deviceID);
					}
					sendLoRaData(nodeId, CHAT_TYPE, testMsg, dataLen);
				}
				else
				{
					myLog_d("Not enough nodes in the list");
				}
				// Release access to nodes list
				xSemaphoreGive(accessNodeList);
			}
			else
			{
				myLog_e("Could not access the nodes list");
			}
		}
	}
}

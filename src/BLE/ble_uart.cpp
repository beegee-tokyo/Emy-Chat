#include "main.h"
#include "ble_uart.h"

/** Buffer for BLE data */
char bleOutData[512] = {0};

/**
 * Read incoming BLE data and handle it
 */
void handleBleData(void)
{
	memset(bleOutData, 0, 512);
	size_t len = bleUartRead(bleOutData, 512);

	myLog_d("Got data %s with len %d", bleOutData, len);
	uint8_t type = bleOutData[0];

	if ((type != CHAT_TYPE) &&
		(type != LOCATION_TYPE) &&
		(type != MAP_TYPE) &&
		(type != NAME_TYPE) &&
		(type != SET_NAME_TYPE))
	{
		myLog_e("Invalid data type, discarding");
		return;
	}

	if (type == SET_NAME_TYPE)
	{
		// Save the username on the device
		saveNickname(&bleOutData[1], len - 1);
		// Get the username
		memset(userName,0,17);
		getNickname(userName);
		// Change the type so that the new name is announced
		type = NAME_TYPE;
	}

	// LoRa send function will take care of the rest
	sendLoRaData(0, type, &bleOutData[1], len - 1);
}

/**
 * Send data over BLE
 * @param receiver
 * 			Node ID the data was received from
 * @param type
 * 			Type of data (chat, location, name, node map)
 * @param data
 * 			Data buffer
 * @param len
 * 			Size of data buffer
 */
void sendBleData(uint32_t receiver, uint8_t type, char *data, size_t len)
{
	/// \todo to be implemented, send data to BLE app
	if (bleUARTnotifyEnabled)
	{
		int sendLen = 0;
		uint8_t mapData[48][5];
		uint8_t nodesInMap;

		switch (type)
		{
		case CHAT_TYPE:
		case NAME_TYPE:
			sendLen = snprintf(bleOutData, 512, "%c<%08X>%s\n", type, receiver, data);
			break;
		case LOCATION_TYPE:
			// Location data
			if (getNodeName(receiver) != NULL)
			{
				sendLen = snprintf(bleOutData, 512, "%c<%s>%s\n", type, getNodeName(receiver), data);
			}
			sendLen = snprintf(bleOutData, 512, "%s\n", data);
			break;
		case MAP_TYPE:
			// Mesh map data
			nodesInMap = nodeMap(mapData);
			bleOutData[0] = 0x34;
			myLog_d("Sending mesh map with %d entries and len %d", nodesInMap, (nodesInMap * 5)+1);
			memcpy(&bleOutData[1], mapData, nodesInMap * 5);
			sendLen = (nodesInMap * 5) + 1;
			break;
		case SET_NAME_TYPE:
			//Tell the app your saved username
			sendLen = snprintf(bleOutData, 512, "%c%s\n", type, userName);
			break;
		default:
			myLog_e("Invalid type");
			return;
		}
		bleUartWrite(bleOutData, sendLen);
	}
}
#include "main.h"

#ifdef NRF52
#include <nrf_nvic.h>
#endif

char consoleOut[512] = {0};

/**
 * Read incoming Console data and handle it
 */
void handleConsoleData(void)
{
	String consoleData = Serial.readStringUntil('\n');

	consoleData.trim();

	myLog_v("Console received: %s", consoleData.c_str());
	if (consoleData.charAt(0) == '/')
	{
		String args[5] = {""};

		int argIndex = 0;
		int start = 0, end;
		while (start < consoleData.length())
		{
			int index = consoleData.indexOf(' ', start);
			end = index == -1 ? consoleData.length() : index;
			args[argIndex] = consoleData.substring(start, end);
			start = end + 1;
			argIndex++;
		}

		String command = args[0].substring(1);
		if (command.startsWith("help"))
		{
			Serial.print("Commands: /help /join /nick /nodes /names /restart\n");
		}
		else if ((command == "join" || command == "nick") && argIndex >= 1)
		{
			String msg;
			if (userName[0] == 0)
			{
				msg = "~ " + args[1] + " joined the channel";
			}
			else
			{
				msg = "~ " + String(userName) + " is now known as " + args[1];
			}

			int len = snprintf(userName, 16, "%s", args[1].c_str());
			saveNickname(userName, len);
			myLog_v("userName set to >%s<", userName);
			myLog_v("Name announcement = %s", msg.c_str());
			sendLoRaData(0, CHAT_TYPE, (char *)msg.c_str(), msg.length());
			delay(500);
			sendLoRaData(0, NAME_TYPE, userName, len);
		}
		else if (command == "restart")
		{
#ifdef ESP32
			ESP.restart();
#else
			sd_nvic_SystemReset();
#endif
		}
		else if (command == "nodes")
		{
			Serial.println("++++++++++++++++++++++++++++++++");
			Serial.println("Mesh map:");
			if (xSemaphoreTake(accessNodeList, (TickType_t)1000) == pdTRUE)
			{
				uint32_t nodeId[48];
				uint32_t firstHop[48];
				uint8_t numHops[48];
				uint8_t numElements = numOfNodes();
				for (uint8_t idx = 0; idx < numElements; idx++)
				{
					getNode(idx, nodeId[idx], firstHop[idx], numHops[idx]);
				}
				// Release access to nodes list
				xSemaphoreGive(accessNodeList);
				// Display the nodes
				Serial.printf("%d nodes in the map\n", numElements + 1);
				Serial.printf("Node #01 id: %08X\n", deviceID);
				for (int idx = 0; idx < numElements; idx++)
				{
					if (firstHop[idx] == 0)
					{
						Serial.printf("Node #%02d id: %08X direct\n", idx + 2, nodeId[idx]);
					}
					else
					{
						Serial.printf("Node #%02d id: %08X first hop %08X #hops %d\n", idx + 2, nodeId[idx], firstHop[idx], numHops[idx]);
					}
				}
			}
			else
			{
				Serial.printf("Could not access the nodes list\n");
			}
			Serial.println("++++++++++++++++++++++++++++++++");
		}
		else if (command == "names")
		{
			Serial.println("++++++++++++++++++++++++++++++++");
			Serial.println("Known nick names:");
			namesList *nickName;
			for (uint8_t idx = 0; idx < _numOfNodes; idx++)
			{
				nickName = getNodeNameByIndex(idx);
				if (nickName != NULL)
				{
					Serial.printf("'%08X' --> '%s'\n", nickName->nodeId, nickName->name);
				}
				else
				{
					break;
				}
			}
			Serial.println("++++++++++++++++++++++++++++++++");
		}
		else
		{
			Serial.println("Unknown command '" + command + "'");
		}
	}
	else
	{
		// LoRa send function will take care of the rest
		sendLoRaData(0, CHAT_TYPE, (char *)consoleData.c_str(), consoleData.length());
	}
}

/**
 * Send data in the right format to the console
 * @param receiver
 * 			sending node ID, can be 0
 * @param type
 * 			type of data
 * @param data
 * 			pointer to the data
 * @param len
 * 			length of data
 */
void sendConsoleData(uint32_t receiver, uint8_t type, char *data, size_t len)
{
	switch (type)
	{
	case CHAT_TYPE:
		if (getNodeName(receiver) != NULL)
		{
			snprintf(consoleOut, 512, "<%s>%s\n", getNodeName(receiver), data);
		}
		else
		{
			snprintf(consoleOut, 512, "<%08X>%s\n", receiver, data);
		}
		break;
	case LOCATION_TYPE:
		if (getNodeName(receiver) != NULL)
		{
			snprintf(consoleOut, 512, "<%s> Loc: %s\n", getNodeName(receiver), data);
		}
		else
		{
			snprintf(consoleOut, 512, "<%08X> Loc: %s\n", receiver, data);
		}
		break;
	case NAME_TYPE:
	case MAP_TYPE:
		// Console does not show name or map messages
		break;
	default:
		myLog_e("Invalid type");
		return;
	}
	Serial.print(consoleOut);
}
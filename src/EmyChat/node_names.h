#include <Arduino.h>

struct namesList
{
	uint32_t nodeId;
	char name[17];
};

void initNodeNames(int numOfNames);
void addNodeName(uint32_t nodeID, char *nodeName);
char *getNodeName(uint32_t nodeID);
uint32_t getNodeIdFromName(char *nodeName);
namesList *getNodeNameByIndex(uint8_t index);
void deleteNodeName(uint32_t nodeId);
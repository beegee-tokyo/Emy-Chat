#include "main.h"

/** The list with all known names */
namesList *namesMap;
/** Index to the first free name entry */
int namesListIndex = 0;

/** Max number of nodes in the list */
int _numOfNames = 0;

/**
 * Initialize list of node names
 * @param numOfNames
 * 		max number of node names
 */
void initNodeNames(int numOfNames)
{
	_numOfNames = numOfNames;
	// Prepare empty names map
	namesMap = (namesList *)malloc(numOfNames * sizeof(namesList));

	if (namesMap == NULL)
	{
		myLog_e("Could not allocate memory for names map");
	}
	else
	{
		myLog_d("Memory for names map is allocated");
	}
	memset(namesMap, 0, numOfNames * sizeof(namesList));
}

/**
 * Add the name of a node with its nodeID
 * @param nodeID
 * 		Node ID
 * @param nodeName
 * 		Node name (char [17] max, larger will be discarded)
 */
void addNodeName(uint32_t nodeID, char *nodeName)
{
	for (int idx = 0; idx < namesListIndex; idx++)
	{
		if (namesMap[idx].nodeId == nodeID)
		{
			// Found the node already in the list, update the name
			snprintf(namesMap[idx].name, 17, "%s", nodeName);
			return;
		}
	}

	if (namesListIndex == _numOfNames)
	{
		// Names list is full
		myLog_e("Names map is already full %d", namesListIndex);
		return;
	}

	namesMap[namesListIndex].nodeId = nodeID;
	snprintf(namesMap[namesListIndex].name, 17, "%s", nodeName);
	namesListIndex++;
}

/**
 * Get the name of a node
 * @param nodeID
 * 		Node ID
 * @return char *
 * 		Pointer to the nodes name or null if no name was found
 */
char *getNodeName(uint32_t nodeID)
{
	for (int idx = 0; idx < _numOfNames; idx++)
	{
		if (namesMap[idx].nodeId == nodeID)
		{
			// Found the node in the list, return pointer to the name
			return namesMap[idx].name;
		}
	}
	myLog_w("Didn't find the name in the list");
	return NULL;
}

/**
 * Get the id of a node by its name
 * @param nodeName
 * 		Node name
 * @return nodeId
 * 		NodeID belonging to node name or 0 if not found
 */
uint32_t getNodeIdFromName(char *nodeName)
{
	for (int idx = 0; idx < _numOfNames; idx++)
	{
		if (strcmp(namesMap[idx].name, nodeName) == 0)
		{
			// Found the node in the list, return pointer to the name
			return namesMap[idx].nodeId;
		}
	}
	myLog_w("Didn't find the nodeID in the list");
	return 0;
}

/**
 * Get node name by index
 * @param index
 * 		Index of request
 * @return char *
 * 		Pointer to namesList entry or NULL if end of list
 */
namesList *getNodeNameByIndex(uint8_t index)
{
	if (namesMap[index].name[0] != 0)
	{
		return &namesMap[index];
	}
	return NULL;
}

/**
 * Delete node name by node ID
 * @param nodeId
 * 		nodeID to be deleted
 */
void deleteNodeName(uint32_t nodeId)
{
	for (int idx = 0; idx < namesListIndex; idx++)
	{
		if (namesMap[idx].nodeId == nodeId)
		{
			myLog_d("Found name entry for %08X", nodeId);
			// Found the node in the list,
			// delete it by copying following names on top of it
			memcpy(&namesMap[idx], &namesMap[idx + 1],
				   sizeof(namesList) * (_numOfNames - idx - 1));
			namesListIndex--;
			return;
		}
	}
}

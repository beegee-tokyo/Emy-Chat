#include "main.h"

/** The list with all known nodes */
nodesList *nodesMap;

namesList *namesMap;

/** Timeout to remove unresponsive nodes */
time_t inActiveTimeout = 120000;

/**
 * Delete a node route by copying following routes on top of it.
 * @param index
 * 		The node to be deleted
 */
void deleteRoute(uint8_t index)
{
	// Delete a route by copying following routes on top of it
	memcpy(&nodesMap[index], &nodesMap[index + 1],
		   sizeof(nodesList) * (_numOfNodes - index - 1));
	nodesMap[_numOfNodes - 1].nodeId = 0;
}

/**
 * Find a route to a node
 * @param id
 * 		Node ID we need a route to
 * @param route
 * 		nodesList struct that will be filled with the route
 * @return bool
 * 		True if a route was found, false if not
 */
bool getRoute(uint32_t id, nodesList *route)
{
	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == id)
		{
			route->firstHop = nodesMap[idx].firstHop;
			route->nodeId = nodesMap[idx].nodeId;
			// Node found in map
			return true;
		}
	}
	// Node not in map
	return false;
}

/** 
 * Add a node into the list.
 * Checks if the node already exists and
 * removes node if the existing entry has more hops
 */
boolean addNode(uint32_t id, uint32_t hop, uint8_t hopNum)
{
	boolean listChanged = false;
	nodesList _newNode;
	_newNode.nodeId = id;
	_newNode.firstHop = hop;
	_newNode.timeStamp = millis();
	_newNode.numHops = hopNum;

	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == id)
		{
			if (nodesMap[idx].firstHop == 0)
			{
				if (hop == 0)
				{ // Node entry exist already as direct, update timestamp
					nodesMap[idx].timeStamp = millis();
				}
				myLog_d("Node %08X already exists as direct", _newNode.nodeId);
				return listChanged;
			}
			else
			{
				if (hop == 0)
				{
					// Found the node, but not as direct neighbor
					myLog_d("Node %08X removed because it was a sub", _newNode.nodeId);
					deleteRoute(idx);
					idx--;
					break;
				}
				else
				{
					// Node entry exists, check number of hops
					if (nodesMap[idx].numHops < hopNum)
					{
						// Node entry exist with smaller # of hops
						myLog_d("Node %08X exist with a lower number of hops", _newNode.nodeId);
						return listChanged;
					}
					else
					{
						// Found the node, but with higher # of hops
						myLog_d("Node %08X exist with a higher number of hops", _newNode.nodeId);
						deleteRoute(idx);
						idx--;
						break;
					}
				}
			}
		}
	}

	// Find unused node entry
	int newEntry = 0;
	for (newEntry = 0; newEntry < _numOfNodes; newEntry++)
	{
		if (nodesMap[newEntry].nodeId == 0)
		{
			// Found an empty slot
			break;
		}
	}

	if (newEntry == _numOfNodes)
	{
		// Map is full, remove oldest entry
		deleteRoute(0);
		listChanged = true;
	}

	// New node entry
	memcpy(&nodesMap[newEntry], &_newNode, sizeof(nodesList));
	listChanged = true;
	myLog_d("Added node %lX with hop %lX and num hops %d", id, hop, hopNum);

	if (getNodeName(_newNode.nodeId) == NULL)
	{
		char nodeName[17] = {0};
		snprintf(nodeName, 17, "%08X", _newNode.nodeId);
		addNodeName(_newNode.nodeId, nodeName);
	}
	return listChanged;
}

/**
 * Remove all nodes that are a non direct and have a given node as first hop.
 * This is to clean up the nodes list from left overs of an unresponsive node
 * @param id
 * 		The node which is listed as first hop
 */
void clearSubs(uint32_t id)
{
	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].firstHop == id)
		{
			myLog_d("Removed node %lX with hop %lX", nodesMap[idx].nodeId, nodesMap[idx].firstHop);
			deleteRoute(idx);
			idx--;
		}
	}
}

/**
 * Check the list for nodes that did not be refreshed within a given timeout
 * @return bool
 * 			True if no changes were done, false if any node was removed
 */
bool cleanMap(void)
{
	// Check active nodes list
	bool mapUpToDate = true;
	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == 0)
		{
			// Last entry found
			break;
		}
		if ((millis() > (nodesMap[idx].timeStamp + inActiveTimeout)))
		{
			// Node was not refreshed for inActiveTimeout milli seconds
			myLog_w("Node %lX with hop %lX timed out", nodesMap[idx].nodeId, nodesMap[idx].firstHop);
			if (nodesMap[idx].firstHop == 0)
			{
				clearSubs(nodesMap[idx].nodeId);
			}
			deleteRoute(idx);
			idx--;
			mapUpToDate = false;
		}
	}
	return mapUpToDate;
}

/**
 * Create a list of nodes and hops to be broadcasted as this nodes map
 * @param subs[]
 * 		Pointer to an array to hold the node IDs
 * @param hops[]
 * 		Pointer to an array to hold the hops for the node IDs
 * @return uint8_t
 * 		Number of nodes in the list
 */
uint8_t nodeMap(uint32_t subs[], uint8_t hops[])
{
	uint8_t subsNameIndex = 0;

	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == 0)
		{
			// Last node found
			break;
		}
		hops[subsNameIndex] = nodesMap[idx].numHops;

		subs[subsNameIndex] = nodesMap[idx].nodeId;
		subsNameIndex++;
	}

	return subsNameIndex;
}

/**
 * Create a list of nodes and hops to be broadcasted as this nodes map
 * @param nodes[]
 * 		Pointer to an two dimensional array to hold the node IDs and hops
 * @return uint8_t
 * 		Number of nodes in the list
 */
uint8_t nodeMap(uint8_t nodes[][5])
{
	uint8_t subsNameIndex = 0;

	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == 0)
		{
			// Last node found
			break;
		}
		nodes[subsNameIndex][0] = nodesMap[idx].nodeId & 0x000000FF;
		nodes[subsNameIndex][1] = (nodesMap[idx].nodeId >> 8) & 0x000000FF;
		nodes[subsNameIndex][2] = (nodesMap[idx].nodeId >> 16) & 0x000000FF;
		nodes[subsNameIndex][3] = (nodesMap[idx].nodeId >> 24) & 0x000000FF;
		nodes[subsNameIndex][4] = nodesMap[idx].numHops;

		subsNameIndex++;
	}

	return subsNameIndex;
}

/**
 * Get number of nodes in the map
 * @return uint8_t
 * 		Number of nodes
 */
uint8_t numOfNodes(void)
{
	uint8_t subsNameIndex = 0;

	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (nodesMap[idx].nodeId == 0)
		{
			// Last node found
			break;
		}
		subsNameIndex++;
	}

	return subsNameIndex;
}

/**
 * Get the information of a specific node
 * @param nodeNum
 * 		Index of the node we want to query
 * @param nodeId
 * 		Pointer to an uint32_t to save the node ID to
 * @param firstHop
 *		Pointer to an uint32_t to save the nodes first hop ID to
 * @param numHops
 * 		Pointer to an uint8_t to save the number of hops to
 * @return bool
 * 		True if the data could be found, false if the requested index is out of range
 */
bool getNode(uint8_t nodeNum, uint32_t &nodeId, uint32_t &firstHop, uint8_t &numHops)
{
	if (nodeNum >= numOfNodes())
	{
		return false;
	}

	nodeId = nodesMap[nodeNum].nodeId;
	firstHop = nodesMap[nodeNum].firstHop;
	numHops = nodesMap[nodeNum].numHops;
	return true;
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
	for (int idx = 0; idx < _numOfNodes; idx++)
	{
		if (namesMap[idx].nodeId == nodeID)
		{
			// Found the node already in the list, update the name
			snprintf(namesMap[idx].name, 17, "%s", nodeName);
			return;
		}
	}

	// Find unused names entry
	int newEntry = 0;
	for (newEntry = 0; newEntry < _numOfNodes; newEntry++)
	{
		if (namesMap[newEntry].nodeId == 0)
		{
			// Found a empty slot
			namesMap[newEntry].nodeId = nodeID;
			snprintf(namesMap[newEntry].name, 17, "%s", nodeName);
			return;
		}
	}

	myLog_e("Names map is already full");
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
	for (int idx = 0; idx < _numOfNodes; idx++)
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
	for (int idx = 0; idx < _numOfNodes; idx++)
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

extern uint32_t broadcastID;

/**
 * Get next broadcast ID
 * @return
 * 		next broadcast ID
 */
uint32_t getNextBroadcastID(void)
{
	uint32_t broadcastNum = broadcastID & 0x000000FF;
	// Create new one by just counting up
	broadcastNum++;
	broadcastNum &= 0x000000FF;
	broadcastID = (broadcastID & 0xFFFFFF00) | broadcastNum;
	return broadcastID;
}

byte broadcastIndex = 0;
#define NUM_OF_LAST_BROADCASTS 10
uint32_t broadcastList[NUM_OF_LAST_BROADCASTS] = {0L};
/**
 * Handle broadcast message ID's
 * to avoid circulating same broadcast over and over
 * 
 * @param broadcastID
 * 			The broadcast ID to be checked
 * @return bool
 * 			True if the broadcast is an old broadcast, else false
 */
bool isOldBroadcast(uint32_t broadcastID)
{
	for (int idx = 0; idx < NUM_OF_LAST_BROADCASTS; idx++)
	{
		if (broadcastID == broadcastList[idx])
		{
			// Broadcast ID is already in the list
			return true;
		}
	}
	// This is a new broadcast ID
	broadcastList[broadcastIndex] = broadcastID;
	broadcastIndex++;
	if (broadcastIndex == NUM_OF_LAST_BROADCASTS)
	{
		// Index overflow, reset to 0
		broadcastIndex = 0;
	}
	return false;
}
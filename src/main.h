#include <vector>
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <Ticker.h>
#include <Log/my-log.h>
#elif defined(NRF52)
#include <nrf.h>
#include "nrf_timer.h"
#include "nrf52Timer.h"
#include <Log/my-log_nrf52.h>
#endif

// Global stuff
void ledOff(void);
extern uint32_t deviceID;
extern char userName[];
#ifdef ESP32
extern Ticker ledOffTick;
#else
extern TimerClass timer;
#endif

/** Internal package types */
#define CHAT_TYPE 0x31
#define LOCATION_TYPE 0x32
#define NAME_TYPE 0x33
#define MAP_TYPE 0x34
#define SET_NAME_TYPE 0x35

/** LoRa package types */
#define LORA_INVALID 0
#define LORA_DIRECT 1
#define LORA_FORWARD 2
#define LORA_BROADCAST 3
#define LORA_NODEMAP 4

// BLE
#include "BLE/ble_uart.h"
void handleBleData(void);
void sendBleData(uint32_t receiver, uint8_t type, char *data, size_t len);
extern bool bleUARTnotifyEnabled;

// LoRa & Mesh
#ifdef USE_RFM95
#include <RadioLib.h>
#else
#include <SX126x-Arduino.h>
#endif
#include <Mesh/mesh.h>
#include <SPI.h>

bool initLoRa(void);
void handleLoraData(void);
void handleNodesListChanges(void);
void sendLoRaData(uint32_t receiver, uint8_t type, char *data, size_t len);
extern boolean newLoRaData;
extern boolean nodesListChanged;
extern uint32_t loraSenderID;

// Console
void handleConsoleData(void);
void sendConsoleData(uint32_t receiver, uint8_t type, char *data, size_t len);

// Display
#include <Display/display.h>

struct localMsg
{
	uint8_t type;
	union {
		uint8_t data[242];
		char msg[242];
	};
};

// Configuration
#include <EmyChat/config.h>

// Emy node names
#include <EmyChat/node_names.h>
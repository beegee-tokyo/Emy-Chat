Emy-Chat
===    
## Scope of the tutorial:
This tutorial will create a chat application that uses my [SX126x-Mesh-Network](https://github.com/beegee-tokyo/SX126x-Mesh-Network) as a base.   
It is inspired by **[DisasterRadio](https://github.com/sudomesh/disaster-radio)**, but is setup on a different code base and has different features:
- uses a different data package structure
- uses my own mesh router (from above mentioned Mesh network)
- allows addressed messages (seen only by addressed nodes) and broadcasts.    
- is limited to a maximum of 48 nodes within a Mesh network.
- detects nodes that disconnect from the Mesh network.
- This does not claim to work reliable in all scenarios and is only tested with 6 active nodes at a time.    
- **_This software is written as a proof of concept and can be used as a base for a LoRa Mesh network based chat application_**

## Target hardware:
The code is written to work on either an ESP32 or a nRF52 microcontroller and is not compatible with other microcontrollers. The LoRa transceivers used are [Semtech SX1262 transceivers](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262). They are either connected to the microcontroller as a Adafruit Feather compatible breakout ([Circuitrocks Alora RFM1262](https://circuit.rocks/product:2685)) or integrated into a SOC together with a Nordic nRF52 ([Insight ISP4520](https://www.insightsip.com/products/combo-smart-modules/isp4520)).    
Using Jan Gromes [RadioLib](https://github.com/jgromes/RadioLib) this chat node SW works as well with RFM95 modules. 

## UI options
For now, the software supports 3 user interfaces.
- BLE
  - A matching Android application is available to connect to a Emy-Chat node, send and receive messages and, if available, show locations of other nodes in a map.    
- Serial console
  - With some simple commands informations about the Mesh network can be shown
  - Messages can be sent to other nodes either as direct addressed messages or broadcasts.
- OLED display (only for ESP32 modules)
  - OLED displays with SH1106 chips and I2C interface to show incoming messages.   
 
**_WiFi is not supported and I have no plans to implement it._**

## And another important thing:
This code is **NOT** written to work with the ArduinoIDE. It is structured to be compiled under [PlatformIO](https://platformio.org/) using one of the IDE’s supported by PlatformIO. My favorite is Visual Studio Code.

## Documentation for this example
At this stage, there is no documentation for the SW available. If I find the time, I will write something, at least a tutorial on [LEARN@CIRCUITROCKS](https://learn.circuit.rocks/) or on [my own website](https://desire.giesecke.tk)    

## How to compile for the different platforms
Have a look into the platformio.ini file. You can see examples for 5 different target boards, 2 nRF52 based boards and 3 ESP32 boards from different sources.

The code depends heavily on compiler defines to adapt different LoRa transceiver boards, used GPIO's, used libraries and available peripherals (display). These defines are set in the platformio.ini file.    
Here is a list of the important defines:    
- -DHAS_DISPLAY=1
  - Set for that have an OLED display based on the SH1106 chipset. This option requires to set as well the GPIOs for SCL and SDA lines. Keep in mind that the display is not supported by nRF52 based modules
  - -DOLED_SDA=23
    - defines the GPIO used to connect the SDA
  - -DOLED_SCL=22
    - defines the GPIO used to connect the SCL
- -DADAFRUIT=1
  - Select code specific for the Adafruit nRF52832 Feather board, if not set, the compilation is for the ISP4520 modules
- -DRED_ESP=1
  - Used to select a specific setup for an Elecrow ESP32S WIFI BLE Board v1.0 board
- -DIS_WROVER=1
  - Used to select a specific setup for a ESP32-Wrover board.
- -DUSE_RFM95=1
  - Selects the use of an RFM95 module instead of the SX1262 chips. Code support is at the moment only for the [Sparkfun Lora Gateway 1 channel](https://www.sparkfun.com/products/15006) board implemented.
Other defines used in the code, but setup by the PlatformIO packages
- ISP4520
  - Set for Insight ISP4520 boards
- ESP32
  - Set for ESP32 boards
- NRF52
  - Set for nRF52 boards

## Android app for BLE connection
The Android app to connect to an Emy-Chat node is ready, but not yet published in Google Play or F-Droid. For now the source codes are in my [Emy-Chat-Android](https://github.com/beegee-tokyo/Emy-Chat-Android) repo.    

## Library Dependencies
#### [SX126x-Arduino](https://github.com/beegee-tokyo/SX126x-Arduino)
- Arduino library for LoRa communication with Semtech SX126x chips. It is based on Semtech's SX126x libraries and adapted to the Arduino framework for ESP32, ESP8266 and nRF52832. It will not work with other uC's like AVR.    

#### [RadioLib](https://github.com/jgromes/RadioLib)
- Support for RFM95 and similar LoRa modules

#### [esp8266-oled-ssd1306](https://github.com/ThingPulse/esp8266-oled-ssd1306)
- Driver for the SSD1306 and SH1106 based 128x64, 128x32, 64x48 pixel OLED display running on ESP8266/ESP32

## Platform Dependencies
#### [Adafruit_nRF52_Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
- The Arduino BSP for Adafruit Bluefruit nRF52 series

#### [arduino-esp32](https://github.com/espressif/arduino-esp32)
- The Arduino BSP for Espressif ESP32 based board.

#### [Arduino Core for Circuitrocks Alora Boards](https://github.com/beegee-tokyo/Circuitrocks_ISP4520_Arduino)
- The Arduino BSP for the ISP4520 boards. Manual installation pf the BSP is required as explained [here](https://github.com/beegee-tokyo/Circuitrocks_ISP4520_Arduino#bsp-installation)
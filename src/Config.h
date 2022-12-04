#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <stdio.h>

/********************************************\
|*  Pin Definitions
\********************************************/

/*
https://drive.google.com/file/d/1gbKM7DA7PI7s1-ne_VomcjOrb0bE2TPZ/view
---+------+----+-----+-----+-----------+---------------------------
No.| GPIO | IO | RTC | ADC | Default   | Function
---+------+----+-----+-----+-----------+---------------------------
25 |   0* | IO | R11 | 2_1 | Boot      | 
35 |   1  | IO |     |     | UART0_TXD | USB Programming/Debug
24 |   2* | IO | R12 | 2_2 |           | LED
34 |   3  | IO |     |     | UART0_RXD | USB Programming/Debug
26 |   4* | IO | R10 | 2_0 |           |
29 |   5* | IO |     |     | SPI0_SS   | 
14 |  12* | IO | R15 | 2_5 |           | Start OTA (Has pulldown for strapping)
16 |  13  | IO | R14 | 2_4 |           | 
13 |  14  | IO | R16 | 2_6 |           | 
23 |  15* | IO | R13 | 2_3 |           | Connect to GND during boot to disable boot messages
27 |  16+ | IO |     |     | UART2_RXD | 
28 |  17+ | IO |     |     | UART2_TXD | 
30 |  18  | IO |     |     | SPI0_SCK  | 
31 |  19  | IO |     |     | SPI0_MISO | 
33 |  21  | IO |     |     | I2C0_SDA  | I2C Sensors
36 |  22  | IO |     |     | I2C0_SCL  | I2C Sensors
37 |  23  | IO |     |     | SPI0_MOSI | I2C Power
10 |  25  | IO | R06 | 2_8 |DAC1/I2S-DT| Battery Enable
11 |  26  | IO | R07 | 2_9 |DAC2/I2S-WS| 
12 |  27  | IO | R17 | 2_7 | I2S-BCK   | 
8  |  32  | IO | R09 | 1_4 |           | Button 1
9  |  33  | IO | R08 | 1_5 |           | Button 2
6  |  34  | I  | R04 | 1_6 |           | Motion 1
7  |  35  | I  | R05 | 1_7 |           | Motion 2
4  |  36  | I  | R00 | 1_0 | SENSOR_VP | 
5  |  39  | I  | R03 | 1_3 | SENSOR_VN | Battery ADC
3  |  EN  | I  |     |     | RESET     | 
---+------+----+-----+-----+-----------+---------------------------
(IO6 to IO11 are used by the internal FLASH and are not useable)
22 x I/O  + 4 x input only = 26 usable pins 
GPIO_34 - GPIO_39 have no internal pullup / pulldown.
+ GPIO 16 and 17 are not available on WROVER (PSRAM)
* Strapping pins: IO0, IO2, IO4, IO5 (HIGH), IO12 (LOW), IO15 (HIGH)
*/

// Power Usage (GPIO Wake enabled)
// 30-Pin CH9102X ESP32 Module Deep Sleep Current
// Unmodified 3.28 mA 
// LED removed: 1.8 mA
// AMS117 regulator removed: 0.127 mA
// CH9102X removed: 29.5 uA

#ifdef NODE_CONFIG
#define NODE_INCLUDE <Nodes/NODE_CONFIG.h>
#include NODE_INCLUDE
#endif

#define LED_PIN 2

#define I2C_POWER_PIN 23
#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#define I2C_FREQUENCY 2000000

#ifndef USE_I2C
#define USE_I2C 1
#endif
#ifndef USE_BH1750
#define USE_BH1750 1
#endif
#ifndef USE_BME280
#define USE_BME280 1
#endif
#ifndef USE_BME680
#define USE_BME680 0
#endif
#ifndef BME_ADDR
#define BME_ADDR 0x76
#endif

// For old ESP32 that do not have the reference voltage in a fuse:
// The reference voltage can be routed to a pin and measured with a multimeter.
// #define ADC_CHECK_VREF_PIN 26
#ifndef ADC_VREF
#define ADC_VREF 1174
#endif
#ifndef BATTERY_ADC_ENABLE_PIN
#define BATTERY_ADC_ENABLE_PIN 25
#endif
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN 39
#endif
#ifndef BATTERY_OVERSAMPLE
#define BATTERY_OVERSAMPLE 8
#endif

#ifndef BATTERY_SEND_INTERVAL_S
#define BATTERY_SEND_INTERVAL_S (60 * 60)
#endif
#ifndef BATTERY_PROTECT_VOLTAGE
#define BATTERY_PROTECT_VOLTAGE 2.95
#endif

// Pin Wakeup uses 15uA in deep sleep.
// RTC IO pins: 0,2,4,12-15,25-27,32-39.
#ifndef BUTTON1_PIN
#define BUTTON1_PIN -1
#endif
#ifndef BUTTON2_PIN
#define BUTTON2_PIN -1
#endif
#ifndef BUTTON3_PIN
#define BUTTON3_PIN -1
#endif
#ifndef BUTTON4_PIN
#define BUTTON4_PIN -1
#endif

#ifndef MOTION1_PIN
#define MOTION1_PIN -1
#endif
#ifndef MOTION1_NAME
#define MOTION1_NAME "AM312 PIR Motion"
#endif
#ifndef MOTION2_PIN
#define MOTION2_PIN -1
#endif
#ifndef MOTION2_NAME
#define MOTION2_NAME "AM312 PIR Motion 2"
#endif
#ifndef MOTION3_PIN
#define MOTION3_PIN -1
#endif
#ifndef MOTION3_NAME
#define MOTION3_NAME "AM312 PIR Motion 3"
#endif
#ifndef MOTION4_PIN
#define MOTION4_PIN -1
#endif
#ifndef MOTION4_NAME
#define MOTION4_NAME "AM312 PIR Motion 4"
#endif
#ifndef MOTION_PULSE_DURATION_S
#define MOTION_PULSE_DURATION_S 30
#endif
#ifndef MOTION_STATE_SEND_INTERVAL_S
#define MOTION_STATE_SEND_INTERVAL_S (30 * 60)
#endif

#ifndef USE_OTA
#define USE_OTA 1
#endif
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME  "esp32-now-ota"
#endif
#define MAX_OTA_WAIT_S 300
#if USE_OTA
#define BUTTON_OTA_PIN 12
#else
#define BUTTON_OTA_PIN -1
#endif

#define  RTC_WAKE_PIN_MASK ( \
  (BUTTON1_PIN < 0 ? 0 : 1LL<<BUTTON1_PIN) | \
  (BUTTON2_PIN < 0 ? 0 : 1LL<<BUTTON2_PIN) | \
  (BUTTON3_PIN < 0 ? 0 : 1LL<<BUTTON3_PIN) | \
  (BUTTON4_PIN < 0 ? 0 : 1LL<<BUTTON4_PIN) | \
  (MOTION1_PIN < 0 ? 0 : 1LL<<MOTION1_PIN) | \
  (MOTION2_PIN < 0 ? 0 : 1LL<<MOTION2_PIN) | \
  (MOTION3_PIN < 0 ? 0 : 1LL<<MOTION3_PIN) | \
  (MOTION4_PIN < 0 ? 0 : 1LL<<MOTION4_PIN) ) 

#ifndef NODE_NAME
#define NODE_NAME "Tester"
#endif
#define ESPNOW_CHANNEL 0 // No others work at the moment
#define ESPNOW_ENCRYPTED 0 // Encryption does not work for broadcasts

#ifndef USE_AES_ENCRYPT
#define USE_AES_ENCRYPT 1
#endif
#define AES_MAGIC_NO_ACK 0xAA01
#define AES_MAGIC_WAIT_ACK 0xAA02
#define AES_MAGIC_ACK 0xAB00
#define AES_MAGIC_OTA 0xAB01
#ifndef USE_AES_ACK
#define USE_AES_ACK 1
#endif
#ifndef AES_RETRY_MS
#define AES_RETRY_MS 50
#endif
#ifndef AES_MAX_RETRY
#define AES_MAX_RETRY (3000 / AES_RETRY_MS)
#endif

/* ============================================== *\
 * Constants
\* ============================================== */

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds 
#define TIME_TO_SLEEP  120          // How long the ESP32 will go to sleep (in seconds) 
#define MAX_FAIL_TIME_TO_SLEEP (60 * 60 * 12) // Maximum time to sleep if receiver is unavailable.

extern const char EMPTY_STRING[];
extern const char NEW_LINE[];

extern const char HOSTNAME[] PROGMEM;
extern const char PROJECT_NAME[] PROGMEM;
extern const char PROJECT_VERSION[] PROGMEM;
extern const char COMPILE_DATE[] PROGMEM;
extern const char COMPILE_TIME[] PROGMEM;

#define FST (const char *)F
#define PM (const __FlashStringHelper *)



/* ============================================== *\
 * Debug
\* ============================================== */

#define SERIAL_BAUD 115200

#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG 1
#endif

#if SERIAL_DEBUG < 1
#define DEBUG_println(...) 
#define DEBUG_print(...) 
#define DEBUG_printf(...) 
#else
#define DEBUG_println(...) if (debugStream) {debugStream->println(__VA_ARGS__);}
#define DEBUG_print(...) if (debugStream) {debugStream->print(__VA_ARGS__);}
#define DEBUG_printf(...) if (debugStream) {debugStream->printf(__VA_ARGS__);}
#endif

#ifndef SHOW_INFO
#define SHOW_INFO 1
#endif
#if SHOW_INFO
#define INFO(...) __VA_ARGS__
#else
#define INFO(...)
#endif


extern Print* debugStream;

#endif // CONFIG_H
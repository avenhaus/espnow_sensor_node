
/*
ESP-C3-01M
---+------+----+-----+-----+-----------+---------------------------
No.| GPIO | IO | RTC | ADC | Default   | Function
---+------+----+-----+-----+-----------+---------------------------
12 |   0  | IO |  +  | 1_0 |           | PIR 1 
13 |   1  | IO |  +  | 1_1 |           | PIR 2
14 |   2* | IO |  +  | 1_2 |           | 
 5 |   3  | IO |  +  | 1_3 |           | Battery Measure
 4 |   4  | IO |  +  | 1_4 |           | Battery Enable
15 |   5  | IO |  +  | 2_0 | SPI-DI    | 
16 |   6  | IO |     |     | SPI-CLK   | 
17 |   7  | IO |     |     | SPI-DO    | 
 3 |   8* | IO |     |     |           | 
 2 |   9* | IO |     |     |           | Boot
18 |  10  | IO |     |     |           | 
11 |  18  | IO |     |     |           | 
10 |  19  | IO |     |     |           | 
 6 |  20  | IO |     |     | UART0_RXD | USB Programming/Debug / JTAG
 7 |  21  | IO |     |     | UART0_TXD | USB Programming/Debug / JTAG
 1 |  EN  | I  |     |     | RESET     | 
---+------+----+-----+-----+-----------+---------------------------
* Strapping Pins
*/

#define NODE_NAME "Hallway Motion"
#define OTA_HOSTNAME  "espnow-hallway-ota"

#define USE_I2C 0
#define USE_BH1750 0
#define USE_BME280 0
#define USE_BME680 0

#define MOTION1_PIN 0
#define MOTION2_PIN 1

#define BATTERY_ADC_PIN 3
#define BATTERY_ADC_ENABLE_PIN 4


#define USE_OTA 1
#define SERIAL_DEBUG 1
#define SHOW_INFO 1

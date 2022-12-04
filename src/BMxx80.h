// Bosch Temperature, Humidity, Pressure (BMP280), VOC (BME680) Sensors

#ifndef BMxx80_H
#define BMxx80_H

#include <Adafruit_Sensor.h>
#ifdef BME680
  #include <Adafruit_BME680.h>
#else
  #include <Adafruit_BME280.h>
#endif

class Bme {
public:
  Bme() {};
  bool begin(uint8_t i2cAddr);
  size_t getSensorJson(char* buffer, size_t bSize=1<<30);
  size_t getDataJson(char* buffer, size_t bSize=1<<30);
  size_t getDataLcd(char* buffer, const char* fmt, size_t bSize=1<<30);
  size_t getHeaderCSV(char* buffer, size_t bSize=1<<30);
  size_t getDataCSV(char* buffer, size_t bSize=1<<30);

protected:
  bool bmeStatus;
  bool readOk;
  #ifdef BME680
    Adafruit_BME680 bme;
  #else
    Adafruit_BME280 bme;
  #endif  
};

extern Bme bme;

#endif // BMxx80_H

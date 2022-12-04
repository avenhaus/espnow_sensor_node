// Bosch Temperature, Humidity, Pressure (BMP280), VOC (BME680) Sensors

#include "Config.h"
#include <Wire.h>
#include "BMxx80.h"

#if USE_I2C && (USE_BME280 | USE_BME680)

Bme bme;

RTC_DATA_ATTR float _bmeOldTemp = -1000.0;
RTC_DATA_ATTR float _bmeOldHumidity = -1000.0;
RTC_DATA_ATTR float _bmeOldPressure = -1000.0;
RTC_DATA_ATTR float _bmeOldResistance = -1000.0;

#define SEALEVELPRESSURE_HPA (1013.25)

bool Bme::begin(uint8_t i2cAddr) {
   bmeStatus = bme.begin(i2cAddr);

  if (!bmeStatus) {
      DEBUG_println(F("Could not find a valid BME sensor, check wiring!"));
      return false;
  } else {
#if USE_BME680
    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
#endif   
  }
  return true;
}

size_t Bme::getSensorJson(char* buffer, size_t bSize){
  if (!bmeStatus) { return 0; }
  size_t n = 0;
#if USE_BME680
  n += snprintf(buffer+n, bSize-n-1, FST("\"BME680\":{\"$Temperature\":{\"dc\":\"temperature\",\"u\":\"°C\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Pressure\":{\"dc\":\"pressure\",\"u\":\"hPa\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Humidity\":{\"dc\":\"humidity\",\"u\":\"%%\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$HGas Resistance\":{\"dc\":\"resistance\",\"u\":\"Ω\"}},")); 
#elif USE_BME280
  n += snprintf(buffer+n, bSize-n-1, FST("\"BME280\":{\"$Temperature\":{\"dc\":\"temperature\",\"u\":\"°C\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Pressure\":{\"dc\":\"pressure\",\"u\":\"hPa\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Humidity\":{\"dc\":\"humidity\",\"u\":\"%%\"}},")); 
#endif
  return n;
}

size_t Bme::getDataJson(char* buffer, size_t bSize) {
  bool hasData = false;
#if USE_BME680
  if (!bmeStatus) {
    return snprintf(buffer, bSize, FST(",\"BME680\":{\"not_found\":true}"));  
  }
  readOk = bme.performReading();
  if (!readOk) {
    strcpy(buffer, FST(",\"BME680\":{\"error\":true}"));
    return strlen(buffer);
  }
  char temp[8]; dtostrf(bme.temperature, 1, 2, temp);
  char bp[16]; dtostrf(bme.pressure*0.01, 1, 2, bp);
  char rh[16]; dtostrf(bme.humidity, 1, 1, rh);
  char voc[16]; dtostrf(bme.gas_resistance, 1, 0, voc);
  return snprintf(buffer, bSize, FST(\
    "\"BME680\":{\"Temperature\":%s,\"Pressure\":%s,\"Humidity\":%s,\"Gas Resistance\":%s},"), temp, bp, rh, voc);
#elif USE_BME280
  if (!bmeStatus) {
    return snprintf(buffer, bSize, FST("\"BME280\":{\"not_found\":true}"));  
  }

  size_t n = snprintf(buffer, bSize, FST("\"BME280\":{"));
  float data = bme.readTemperature();
  if (abs(data - _bmeOldTemp) >= 0.1 ) {
    n += snprintf(buffer+n, bSize-n-2, FST("\"Temperature\":%0.1f,"), data);
    _bmeOldTemp = data;
    hasData = true;
  }
  data = bme.readPressure() * 0.01;
  if (abs(data - _bmeOldPressure) >= 0.05 ) {
    n += snprintf(buffer+n, bSize-n-2, FST("\"Pressure\":%0.2f,"), data);
    _bmeOldPressure = data;
    hasData = true;
  }
  data = bme.readHumidity();
  if (abs(data - _bmeOldHumidity) >= 0.2 ) {
    n += snprintf(buffer+n, bSize-n-2, FST("\"Humidity\":%0.1f,"), data);
    _bmeOldHumidity = data;
    hasData = true;
  }
#endif
  if (!hasData) { return 0; }
  buffer[n-1] = '}';
  buffer[n++] = ',';
  return n;
}

size_t Bme::getDataLcd(char* buffer, const char* fmt, size_t bSize) {
#if USE_BME680
  if (!readOk) {
    return snprintf(buffer, bSize, FST("BME680 no data"));
  }
  return snprintf(buffer, bSize, fmt, bme.temperature, bme.pressure*0.01, bme.humidity, bme.gas_resistance);
#elif USE_BME280
  return snprintf(buffer, bSize, fmt, bme.readTemperature(), bme.readPressure()*0.01, bme.readHumidity(), 0.0);
#endif
}

#endif // USE_I2C && (USE_BME280 | USE_BME680)

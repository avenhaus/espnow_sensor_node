// BH1750 Light Sensor

#include "Config.h"
#include <Wire.h>
#include <BH1750.h>

BH1750 bh1750;
bool bh1750IsConnected = false;
uint16_t bh1750Lux = 0;
RTC_DATA_ATTR uint16_t bh1750LuxOld = 0xFFFF;

void BH1750Setup() {
  bh1750IsConnected = bh1750.begin();
  if (!bh1750IsConnected) {
      DEBUG_println(F("Could not find a valid BH1750 light sensor, check wiring!"));
  }
  bh1750.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
}

size_t BH1750GetSensorJson(char* buffer, size_t bSize) {
  if (!bh1750IsConnected) { return 0; }
  return snprintf(buffer, bSize-1, FST("\"$BH1750 Illuminance\":{\"dc\":\"illuminance\",\"u\":\"lx\"},")); 
}


// Return true if difference is greater 1%
bool bh1750NeedsUpdate(uint16_t bh1750Lux) {
  if (bh1750Lux == bh1750LuxOld) { return false; }
  if (bh1750Lux == 0 || bh1750LuxOld == 0) { return true; }
  float diff = 0.0;
  if (bh1750Lux > bh1750LuxOld) { diff = (float) (bh1750Lux - bh1750LuxOld) / (float) bh1750LuxOld; }
  else { diff = (float) (bh1750LuxOld - bh1750Lux) / (float) bh1750Lux; }
  return diff > 0.01;
}

size_t BH1750GetDataJson(char* buffer, size_t bSize) {
  if (!bh1750IsConnected) {
    return snprintf(buffer, bSize, FST("\"BH1750 Illuminance\":-1,"));  
  }
  bh1750Lux = bh1750.readLightLevel();
  if (!bh1750NeedsUpdate(bh1750Lux)) { return 0; }
  bh1750LuxOld = bh1750Lux;
  return snprintf(buffer, bSize, FST("\"BH1750 Illuminance\":%d,"), bh1750Lux);
}

size_t BH1750GetDataLcd(char* buffer, const char* fmt, size_t bSize) {
  return snprintf(buffer, bSize, fmt, bh1750Lux);
}


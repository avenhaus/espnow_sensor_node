// BH1750 Light Sensor

#ifndef BH1750_H
#define BH1750_H
#include <Arduino.h>

void BH1750Setup();
size_t BH1750GetSensorJson(char* buffer, size_t bSize=1<<30);
size_t BH1750GetDataJson(char* buffer, size_t bSize=1<<30);
size_t BH1750GetDataLcd(char* buffer, const char* fmt, size_t bSize=1<<30);


#endif // BH1750_H

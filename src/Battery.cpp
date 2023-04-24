#include "Config.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"

void shutdown();

int batteryRaw = 0;
float batteryPinVoltage = -1.0;
float batteryVoltage = -1.0;


//Characterize ADC at particular atten
static esp_adc_cal_characteristics_t adc1Chars;

uint8_t getLiFePo4Charge(float voltage) {
  if (voltage >= 3.40) { return 100; }
  if (voltage >= 3.35) { return 90; }
  if (voltage >= 3.32) { return 80; }
  if (voltage >= 3.30) { return 70; }
  if (voltage >= 3.27) { return 60; }
  if (voltage >= 3.26) { return 50; }
  if (voltage >= 3.25) { return 40; }
  if (voltage >= 3.22) { return 30; }
  if (voltage >= 3.20) { return 20; }
  if (voltage >= 3.10) { return 15; }
  if (voltage >= 3.00) { return 10; }
  if (voltage >= 2.90) { return 5; }
  return 0;
}

void batteryAdcInit() {
  pinMode(BATTERY_ADC_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ADC_ENABLE_PIN, HIGH);
  // adc_power_acquire();

    esp_adc_cal_value_t valType = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, ADC_VREF, &adc1Chars);
    //Check type of calibration value used to characterize ADC
    INFO(
    if (valType == ESP_ADC_CAL_VAL_EFUSE_VREF) { DEBUG_println(FST("eFuse ADC VRef")); } 
    else if (valType == ESP_ADC_CAL_VAL_EFUSE_TP) { DEBUG_println(FST("Two Point ADC VRef")); } 
    else { DEBUG_printf(FST("Default ADC VRef: %3f\n"), ADC_VREF * 0.001); });

    #if ADC_CHECK_VREF_PIN > 0
    esp_err_t status;
    status = adc2_vref_to_gpio(ADC_CHECK_VREF_PIN);
    if (status == ESP_OK){
        printf("v_ref routed to GPIO %d\n", ADC_CHECK_VREF_PIN);
    }else{
        printf("failed to route v_ref\n");
    }
    while(1) {}
    #endif

}


float batteryAdcRead() {
  batteryPinVoltage = 0.0;
  for (int i=0; i<BATTERY_OVERSAMPLE; i++) {
    batteryRaw = analogRead(BATTERY_ADC_PIN); 
    batteryPinVoltage += esp_adc_cal_raw_to_voltage(batteryRaw, &adc1Chars);
  }
  batteryPinVoltage *= 0.001 / BATTERY_OVERSAMPLE;
  batteryVoltage = (batteryPinVoltage * 2.0 * BATTERY_VOLTAGE_CORRECT); // Voltage divider and GPIO drop
  INFO(DEBUG_printf(FST("Battery: %d  %.3fV  %.3fV\n"), batteryRaw, batteryPinVoltage, batteryVoltage));
  #ifdef BATTERY_PROTECT_VOLTAGE 
    if (batteryVoltage <= BATTERY_PROTECT_VOLTAGE) {
      DEBUG_printf(FST("Battery is low: %.3fV - Shutting down.\n"), batteryVoltage);
      shutdown();
    }
  #endif
  return batteryVoltage;
}

size_t batteryGetSensorJson(char* buffer, size_t bSize) {
  size_t n = 0;
  n += snprintf(buffer+n, bSize-n-1, FST("\"Battery\":{\"$Voltage\":{\"dc\":\"voltage\",\"i\":\"mdi:battery\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Charge\":{\"dc\":\"battery\"},")); 
  n += snprintf(buffer+n, bSize-n-1, FST("\"$Low\":{\"t\":1,\"dc\":\"battery\",\"nv\":0}},")); 
  return n;
}

size_t batteryGetDataJson(char* buffer, size_t bSize) {
  size_t n = 0;
  float bv = batteryAdcRead();
  n += snprintf(buffer+n, bSize-n-1, FST("\"Battery\":{\"Voltage\":%.3f,"), bv); 
  uint8_t charge = getLiFePo4Charge(bv);
  n += snprintf(buffer+n, bSize-n-1, FST("\"Charge\":%d,"), charge); 
  { n += snprintf(buffer+n, bSize-n-1, FST("\"Low\":%d,"), (charge <= 15)); }
  buffer[n-1] = '}';
  buffer[n++] = ',';
  return n;
}

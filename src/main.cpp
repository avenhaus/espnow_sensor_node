// Set Flash SPI mode from DIO to QIO or QOUT (Not working for WROOM) (in Serial flasher config)
// Set Flash SPI speed from 40 MHz to 80 MHz (in Serial flasher config)
// Set Flash Size from 2MB to 4MB (in Serial flasher config)
// Set Detect flash size when flashing bootloader (in Serial flasher config)
// Set Partition Table: Single factory app (large), no OTA
// Set Detect  (in Serial flasher config)
// Set Log output from Info to Warning (in component config)
// Set Bootloader log verbosity from Info to Warning (in Bootloader config)
// Enable Skip image validation from when exiting from deep sleep (in Bootloader config)
// Enable Skip image validation from power on (in Bootloader config)

// GPIO 15 to Ground  during boot / while writing bootloader can disable log messages

// %HOMEPATH%\.platformio\packages\tool-esptoolpy\esptool.py --chip esp32 --port "COM7" --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode qout --flash_freq 80m --flash_size detect 0x1000 bootloader.bin

// %HOMEPATH%\esp\esp-idf\components\esptool_py\esptool\esptool.py -p COM7 -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode qout --flash_freq 80m --flash_size detect 0x1000 bootloader.bin



// Normal Android boot loader: 206ms to wake up and sleep (4.44mC)
// Custom bootloader: 33.1ms to wake up and sleep (1.39mC) - Needs Fast Mem


#include "Config.h"
#include "Secret.h"
#include <WiFi.h>
#include <Wire.h>
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "BMxx80.h"

#include "time.h"
#include <sys/time.h>

bool espNowSetup();
size_t espNowInitBuffer(char* buffer);
size_t espNowFinishBuffer(char* buffer, size_t n);
bool espNowSend(char* buffer, size_t n);
uint8_t espNowCheckResponse();
extern bool espNowTriggerOTA;
RTC_DATA_ATTR extern uint32_t _msgFailCount;
RTC_DATA_ATTR extern uint32_t _msgTotalFailCount;
RTC_DATA_ATTR uint32_t _msgSentTotalFailCount = 0;

void OTA_setup();
void OTA_run();
extern uint32_t otaStartTs;

void batteryAdcInit();
float batteryAdcRead();
size_t batteryGetSensorJson(char* buffer, size_t bSize);
size_t batteryGetDataJson(char* buffer, size_t bSize);

void BH1750Setup();
size_t BH1750GetSensorJson(char* buffer, size_t bSize=1<<30);
size_t BH1750GetDataJson(char* buffer, size_t bSize=1<<30);

RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint64_t _motionActivePins = 0;
RTC_DATA_ATTR time_t _motionWakeTime = 0;
RTC_DATA_ATTR time_t _motionStateSendTime = 0;
RTC_DATA_ATTR time_t _batterySendTime = 0;


Print* debugStream = &Serial;

static time_t _wakeTime;


/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : DEBUG_println(FST("Wakeup caused by external signal using RTC_IO")); break;
    case ESP_SLEEP_WAKEUP_EXT1 : DEBUG_println(FST("Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : DEBUG_println(FST("Wakeup caused by timer")); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : DEBUG_println(FST("Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : DEBUG_println(FST("Wakeup caused by ULP program")); break;
    default : DEBUG_printf(FST("Wakeup was not caused by deep sleep: %d\n"), wakeup_reason); break;
  }
}

void shutdown() {
  DEBUG_println(FST("Shutting down"));
  WiFi.disconnect(true);  // Disconnect from the network
  WiFi.mode(WIFI_OFF);    // Switch WiFi off
  btStop();
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_OFF);  // RTC IO, sensors and ULP co-processor
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);  // RTC slow memory
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);  // RTC fast memory
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);  // XTAL oscillator
#if SOC_PM_SUPPORT_CPU_PD
  esp_sleep_pd_config(ESP_PD_DOMAIN_CPU,          ESP_PD_OPTION_OFF);  // CPU core
#endif
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M,        ESP_PD_OPTION_OFF);  // Internal 8M oscillator
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO,      ESP_PD_OPTION_OFF);  // VDD_SDIO
  
  Serial.flush(); 
  esp_deep_sleep_start();
}

void hibernate() {
#if USE_I2C
  Wire.end();
  digitalWrite(I2C_POWER_PIN, LOW);
  pinMode(I2C_POWER_PIN, INPUT);
#endif

  adc_power_off();
  pinMode(BATTERY_ADC_ENABLE_PIN, INPUT);

  WiFi.disconnect(true);  // Disconnect from the network
  WiFi.mode(WIFI_OFF);    // Switch WiFi off
  btStop();

  #if RTC_WAKE_PIN_MASK
  // AM312 high pulse is 2sec. Don't wake up immediately on that pin again. Instead wait for 10 sec to check pin state.
  uint64_t wakeMask = RTC_WAKE_PIN_MASK ^ _motionActivePins;

  INFO(DEBUG_printf(FST("Wake Mask: %llX Active Pins: %llX\n"), wakeMask, _motionActivePins));

#if SOC_PM_SUPPORT_EXT_WAKEUP
  // RTC IO pins: 0,2,4,12-15,25-27,32-39.
  if (wakeMask) { esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_HIGH); }
#elif SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
  // RTC IO pins: GPIO0 to GPIO5.
  if (wakeMask) { esp_deep_sleep_enable_gpio_wakeup(wakeMask, ESP_GPIO_WAKEUP_GPIO_HIGH); }
#endif
  else { esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF); }
  #else
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_OFF);
  #endif

  time_t now = time(nullptr);
  uint32_t timeToSleep = TIME_TO_SLEEP - (now % TIME_TO_SLEEP);
  if (_msgFailCount >= 5) {
    // Exponential backoff when receiver is unavailable to save battery.
    timeToSleep = timeToSleep << ((_msgFailCount-3) >> 1);
    if (timeToSleep > MAX_FAIL_TIME_TO_SLEEP) { timeToSleep > MAX_FAIL_TIME_TO_SLEEP; }
  } else {
    int32_t motionWakeTTS = _motionWakeTime - now;
    if (motionWakeTTS >= 0 && motionWakeTTS < timeToSleep) { timeToSleep = motionWakeTTS; }
  }

  esp_sleep_enable_timer_wakeup(timeToSleep > 0 ? timeToSleep * uS_TO_S_FACTOR : 1000);
  INFO(DEBUG_printf(FST("Sleep for %u seconds (now:%ld)\n"), timeToSleep, now));

  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF); Needed for fast boot
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF); Needed for fast boot
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);
  INFO(DEBUG_println(FST("Configured RTC Peripherals to be powered down in sleep. Going to sleep now.")));
  Serial.flush(); 
  esp_deep_sleep_start();
}



void sendSensorConfig() {

  char buffer[250];  // Max ESP-NOW mwssage size is 250 bytes.
  size_t n = espNowInitBuffer(buffer);
  n += batteryGetSensorJson(buffer+n, sizeof(buffer)-n-2);

#if MOTION1_PIN >= 0
  n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"$" MOTION1_NAME "\":{\"t\":1,\"dc\":\"motion\"},"));
#endif
#if MOTION2_PIN >= 0
  n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"$" MOTION2_NAME "\":{\"t\":1,\"dc\":\"motion\"},"));
#endif

  n = espNowFinishBuffer(buffer, n);
  espNowSend(buffer, n);
  espNowCheckResponse();


  n = espNowInitBuffer(buffer);

#if USE_BH1750
  n += BH1750GetSensorJson(buffer+n, sizeof(buffer)-n-2);
#endif

#if USE_BME280 || USE_BME680
  n += bme.getSensorJson(buffer+n, sizeof(buffer)-n-2);
#endif

  n = espNowFinishBuffer(buffer, n);
  espNowSend(buffer, n);
  espNowCheckResponse();
}

void handleGpioWake(int gpio) {
  espNowSetup();

  char buffer[250];  // Max ESP-NOW mwssage size is 250 bytes.
  size_t n = espNowInitBuffer(buffer);
  
  #if MOTION1_PIN >= 0
  if (gpio == MOTION1_PIN) {
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION1_NAME "\":1,"));
    _motionActivePins |= (1LL << MOTION1_PIN);
    _motionWakeTime = _wakeTime + MOTION_PULSE_DURATION_S;
  }
  #endif

  #if MOTION2_PIN >= 0
  if (gpio == MOTION2_PIN) {
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION2_NAME "\":1,"));  
    _motionActivePins |= (1LL << MOTION2_PIN);
    _motionWakeTime = _wakeTime + MOTION_PULSE_DURATION_S;
  }
  #endif

  n = espNowFinishBuffer(buffer, n);
  if(espNowSend(buffer, n)) {
    hibernate();
  }
  INFO(DEBUG_printf(FST("Send %d bytes\n"), n));
  espNowCheckResponse();

  hibernate();
}

void checkMotionClear() {
  INFO(DEBUG_printf(FST("Check Motion pins: %llX\n"), _motionActivePins));
  char buffer[250];  // Max ESP-NOW mwssage size is 250 bytes.
  size_t n = espNowInitBuffer(buffer);
  bool sendUpdate = false;

#if MOTION1_PIN >= 0
  if (_motionActivePins & (1LL << MOTION1_PIN) && !digitalRead(MOTION1_PIN)) {
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION1_NAME "\":0,"));
    _motionActivePins &= ~(1LL << MOTION1_PIN);
    sendUpdate = true;
  }
#endif

#if MOTION2_PIN >= 0
  if (_motionActivePins & (1LL << MOTION2_PIN) && !digitalRead(MOTION2_PIN)) {
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION2_NAME "\":0,"));
    _motionActivePins &= ~(1LL << MOTION2_PIN);
    sendUpdate = true;
  }
#endif

  if (sendUpdate) {
    espNowSetup();
    n = espNowFinishBuffer(buffer, n);
    if(espNowSend(buffer, n)) {
      hibernate();
    }
    INFO(DEBUG_printf(FST("Send %d bytes\n"), n));
    espNowCheckResponse();
  }
  hibernate();
}


void setup() {
  Serial.begin(SERIAL_BAUD);
  INFO(printWakeupReason());
  bootCount++;
  time(&_wakeTime);
  INFO(DEBUG_printf(FST("ESP-NOW Sender Boot number: %d ts:%d RTC:%d\n"), bootCount, millis(), _wakeTime));
  
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) { 
#if SOC_PM_SUPPORT_EXT_WAKEUP
    int wakeGPIO = log(esp_sleep_get_ext1_wakeup_status())/log(2);
#elif SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    int wakeGPIO = log(esp_sleep_get_gpio_wakeup_status())/log(2);
#endif
    INFO(DEBUG_printf(FST("GPIO that triggered the wake up: %d\n"), wakeGPIO));
#if USE_OTA &&  (BUTTON_OTA_PIN >= 0)
    pinMode(BUTTON_OTA_PIN, INPUT);
    if (digitalRead(BUTTON_OTA_PIN)) {
      Serial.println(FST("Starting OTA..."));
      OTA_setup();
      return; // Run OTA from main loop
    }
#endif
    return handleGpioWake(wakeGPIO);
  } 

  if (_motionActivePins && _wakeTime > _motionWakeTime) {
    return checkMotionClear();
  }

  bool checkBattery = false;
  if(_wakeTime >= _batterySendTime) {
    _batterySendTime = _wakeTime + BATTERY_SEND_INTERVAL_S;
    checkBattery = true;
    batteryAdcInit();
  }

#if USE_I2C
  pinMode(I2C_POWER_PIN, OUTPUT);
  digitalWrite(I2C_POWER_PIN, HIGH);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
  #if USE_BH1750
    BH1750Setup();
  #endif
  #if USE_BME280 || USE_BME680
    bme.begin(BME_ADDR);
  #endif
#endif

  if (espNowSetup()) { hibernate(); }

  if (bootCount == 1) { sendSensorConfig(); }

  char buffer[250];  // Max ESP-NOW mwssage size is 250 bytes.
  size_t n = espNowInitBuffer(buffer);
  size_t startN = n;

  if (checkBattery) { 
    n += batteryGetDataJson(buffer+n, sizeof(buffer)-n-2);
  }

#if USE_BH1750
  n += BH1750GetDataJson(buffer+n, sizeof(buffer)-n-2);
#endif
#if USE_BME280 || USE_BME680
  n += bme.getDataJson(buffer+n, sizeof(buffer)-n-2);
#endif

  if(_wakeTime >= _motionStateSendTime) {
    _motionStateSendTime = _wakeTime + MOTION_STATE_SEND_INTERVAL_S;
#if MOTION1_PIN >= 0
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION1_NAME "\":%d,"), digitalRead(MOTION1_PIN));
#endif
#if MOTION2_PIN >= 0
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"" MOTION2_NAME "\":%d,"), digitalRead(MOTION1_PIN));
#endif
  }

  if (_msgSentTotalFailCount != _msgTotalFailCount) {
    n += snprintf(buffer+n, sizeof(buffer)-n-2, FST("\"tf\":%d,"), _msgTotalFailCount);
  }

  if (startN != n) { // Only send packet if there is actually data
    n = espNowFinishBuffer(buffer, n);
    if(espNowSend(buffer, n)) {
      hibernate();
    }
    INFO(DEBUG_printf(FST("Send %d bytes\n"), n));
  } else {
    INFO(DEBUG_printf(FST("Nothing to send\n")));
  }

#if USE_I2C
  Wire.end();
  digitalWrite(I2C_POWER_PIN, LOW);
  pinMode(I2C_POWER_PIN, INPUT);
#endif

  adc_power_off();
  pinMode(BATTERY_ADC_ENABLE_PIN, INPUT);

  if ((startN != n) && (espNowCheckResponse() != -1)) { _msgSentTotalFailCount = _msgTotalFailCount; }

  if (espNowTriggerOTA) {
      Serial.println(FST("ESP-NOW starting OTA..."));
      OTA_setup();
      return; // Run OTA from main loop
  }
  
  hibernate();
}

void loop() {
  // We normally never get here
#if USE_OTA
  OTA_run();
  if (millis() > otaStartTs + (MAX_OTA_WAIT_S * 1000)) {
    DEBUG_println(FST("Timed out waiting for OTA"));
    hibernate();
  }
#endif
}
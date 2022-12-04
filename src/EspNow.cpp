#include "Config.h"
#include "Secret.h"
#include <esp_now.h>
#include <WiFi.h>

RTC_DATA_ATTR uint8_t _msgCount = 0;
RTC_DATA_ATTR uint32_t _msgFailCount = 0;
RTC_DATA_ATTR uint32_t _msgTotalFailCount = 0;

static int32_t _msgSendTs = 0;
static uint8_t _msgState = 0;
static uint8_t _msgSize = 0;
static uint16_t _msgRetry = 0;
bool espNowTriggerOTA = false;

esp_now_peer_info_t peerInfo = {};

#ifndef ESPNOW_GATEWAY_MAC
// Broadcast Address
const uint8_t peerMAC[] PROGMEM = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#else
const uint8_t peerMAC[] PROGMEM = ESPNOW_GATEWAY_MAC;
#endif


#ifdef NODE_NAME
const char* nodeName = FST(NODE_NAME);
#else 
const char* nodeName = nullptr;
#endif


#if USE_AES_ENCRYPT
#include "mbedtls/aes.h"
mbedtls_aes_context aes;
const uint8_t aesKey[] PROGMEM = AES_KEY;
const uint8_t aesIV[] PROGMEM = AES_IV;

typedef struct __attribute__ ((packed)) AesHeader {
  uint16_t magic;
  uint16_t sequence;
  uint8_t len;  // ESP-NOW messages can only be 250 bytes.
  uint8_t retry;  
  uint32_t uptime;
} AesHeader;

typedef struct __attribute__ ((packed)) AesMessage {
  AesHeader header;
  uint8_t data[250-sizeof(AesHeader)];
} AesMessage;

typedef struct __attribute__ ((packed)) AesResponse {
  uint16_t magic;
  uint16_t sequence;
  //uint8_t mac[6];
} AesMResponse;

AesMessage msg;
#endif

#if ESPNOW_ENCRYPTED
// ESP-NOW PMK and LMK keys (16 bytes each)
static const uint8_t PMK_KEY[] = ESPNOW_PMK_KEY;
static const uint8_t LMK_KEY[] = ESPNOW_LMK_KEY;
#endif

// Callback when data is received
void onDataRecv(const uint8_t* mac, const uint8_t* rdata, int len) {
  AesResponse* res = (AesResponse*) rdata;
  INFO(DEBUG_printf(FST("%02X:%02X:%02X:%02X:%02X:%02X: received %d bytes Magic:%04X Seq:%d time:%d\n"),
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len, res->magic, res->sequence, millis()-_msgSendTs));

  if (len == sizeof(AesResponse)) {
    #if USE_OTA
    if (res->magic == AES_MAGIC_OTA) { espNowTriggerOTA = true; return; }
    #endif
    if (res->magic == AES_MAGIC_ACK && res->sequence == _msgCount) {
        _msgState++; // Ack Received
        _msgCount++;
    }
  }
}
 

// Callback when data done sending
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  INFO(DEBUG_printf(FST("%02X:%02X:%02X:%02X:%02X:%02X - status: %d - duration: %d\n"),
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], status, millis() - _msgSendTs));
  _msgState++; // Sent
}

bool espNowSendPacket(uint8_t* data, size_t size) {
  _msgSendTs = millis();
  _msgState = 0;
  esp_err_t eres = esp_now_send(peerMAC, data, size);
  if (eres != ESP_OK) {
    char estr[256];
    esp_err_to_name_r(eres, estr, sizeof(estr));
    Serial.printf("Failed to send packet. Error: %X %s\n", eres, estr);
    return true;
  }
  return false;
}


bool espNowSend(char* buffer, size_t n) {
  uint8_t* data = (uint8_t*)buffer;
  size_t osize = n;
#if USE_AES_ENCRYPT
  msg.header.magic = USE_AES_ACK ? AES_MAGIC_WAIT_ACK : AES_MAGIC_NO_ACK;
  msg.header.sequence = _msgCount;
  msg.header.len = n;
  msg.header.retry = 0;
  time_t uptime = time(nullptr);
  msg.header.uptime = uptime;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc( &aes, (const unsigned char*) aesKey, sizeof(aesKey) * 8 );
  uint8_t iv[sizeof(aesIV)];
  memcpy(iv, aesIV, sizeof(aesIV));
  esp_fill_random(buffer, 4); // Add salt
  size_t blockLen = ((16 - (n & 0xF)) & 0xF) + n;
  esp_aes_crypt_cbc(&aes, ESP_AES_ENCRYPT, blockLen, iv, (uint8_t*)buffer, msg.data);
  mbedtls_aes_free(&aes);
  INFO(DEBUG_printf(FST("Encrypted %d (%d) bytes including 4 byte salt.\n"), blockLen, n));
  n = blockLen + sizeof(AesHeader); // Add size of header
  data = (uint8_t*) &msg;
#endif
  _msgSize = n;
  _msgRetry = 0;
  if (_msgSize > 250) {
    Serial.printf(FST("Packet size max is 250. Size=%d (orig:%d) %s\n"), _msgSize, osize, buffer);
  }
  return espNowSendPacket(data, _msgSize);
}

void espNowWaitSent() {
  while (!_msgState) {
    if(millis() > _msgSendTs + 25) {
      DEBUG_println(FST("Timed out sending data"));
      break;
    }
  }
}

uint8_t espNowCheckResponse() {
  espNowWaitSent();
  #if USE_AES_ACK
  uint32_t retry = 0;
  while (_msgState < 2) {
    if (espNowTriggerOTA) { return -2; }
    if(millis() > _msgSendTs + AES_RETRY_MS) {
      retry++;
      msg.header.retry++;
      DEBUG_printf(FST("Timed out waiting for response. Retry: %d\n"), msg.header.retry);
      if (retry >= AES_MAX_RETRY) {
        DEBUG_printf(FST("Giving up to send message %d Retry: %d\n"), msg.header.sequence, msg.header.retry);
        _msgCount++;
        _msgState = -1;
        _msgFailCount++;
        _msgTotalFailCount++;
        return _msgState;
      }
      espNowSendPacket((uint8_t*) &msg, _msgSize);
      espNowWaitSent();
    }
  }
  _msgFailCount = 0;
  #endif
  return _msgState;
}

size_t espNowInitBuffer(char* buffer) {
  size_t n = 0;
  #if USE_AES_ENCRYPT
  buffer[n++] = 'S'; // Space for Salt
  buffer[n++] = 'a';
  buffer[n++] = 'l';
  buffer[n++] = 't';
  #endif
  buffer[n++] = '{';
  if (nodeName) { 
    n += sprintf(buffer+n, FST("\"name\":\"%s\","), nodeName); 
  }
  return n;
}

size_t espNowFinishBuffer(char* buffer, size_t n) {
  if (buffer[n-1] == ',') { n--; }
  buffer[n++] = '}';
  buffer[n++] = '\0';
  INFO(DEBUG_println(buffer));
  return n;
}

bool espNowSetup() {
    // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  INFO(DEBUG_println(WiFi.macAddress()));

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    DEBUG_println(FST("Error initializing ESP-NOW"));
    return true;
  }
  // esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, ESPNOW_CHANNEL);
  esp_now_register_send_cb(onDataSent);
#if ESPNOW_ENCRYPTED 
    esp_now_set_pmk(PMK_KEY);
#endif

  // Add peer
  memcpy(peerInfo.peer_addr, peerMAC, sizeof(peerMAC));
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = ESPNOW_ENCRYPTED; // Multicast does not support encryption
#if ESPNOW_ENCRYPTED
  memcpy(peerInfo.lmk, LMK_KEY, 16);
#endif
  esp_err_t eres = esp_now_add_peer(&peerInfo);
  if (eres != ESP_OK) {
    char estr[256];
    esp_err_to_name_r(eres, estr, sizeof(estr));
    Serial.printf("Failed to add peer. Error: %X %s\n", eres, estr);
    return true;
  }
  INFO(DEBUG_printf(FST("Added peer %02X:%02X:%02X:%02X:%02X:%02X\n"),
      peerMAC[0], peerMAC[1], peerMAC[2], peerMAC[3], peerMAC[4], peerMAC[5]));
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(onDataRecv);
  return false;
}

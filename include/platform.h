#pragma once
// Слой совместимости ESP8266 / ESP32: всё, что отличается между платформами, собрано здесь.
// Профиль выбирается в PlatformIO (env:d1_mini или env:esp32dev) — код менять не нужно.
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define PLATFORM_ESP32 1
  #define PLATFORM_NAME  "ESP32"
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
  #include <ESP32Servo.h>
  #include <LittleFS.h>
  #if __has_include(<esp_arduino_version.h>)
    #include <esp_arduino_version.h>
  #endif
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    #define LEDC_NEW_API 1               // Arduino-ESP32 3.x: ledcAttach(pin, ...), ledcWrite(pin, ...)
  #endif
  typedef WebServer HttpServer;
  #define OTA_FS_CMD U_SPIFFS

  inline String   chipIdHex()          { return String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX); }
  inline uint32_t hwRandomSeed()       { return esp_random(); }
  inline void     setHostName(const char *n) { WiFi.mode(WIFI_STA); WiFi.setHostname(n); }
  inline void     mdnsTick()           {}          // на ESP32 mDNS обновлять не нужно
  inline void     ntpConfigure(const char *tz, const char *s1, const char *s2) { configTzTime(tz, s1, s2); }
  inline bool     fsBegin()            { return LittleFS.begin(true); }   // true = отформатировать, если не смонтировалась
  inline void     tcpSetTimeoutMs(WiFiClient &c, uint32_t ms) { c.setTimeout((ms + 999) / 1000); }   // на ESP32 таймаут в секундах

#elif defined(ARDUINO_ARCH_ESP8266)
  #define PLATFORM_ESP8266 1
  #define PLATFORM_NAME  "ESP8266"
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>
  #include <Servo.h>
  #include <LittleFS.h>
  typedef ESP8266WebServer HttpServer;
  #define OTA_FS_CMD U_FS

  inline String   chipIdHex()          { return String(ESP.getChipId(), HEX); }
  inline uint32_t hwRandomSeed()       { return analogRead(A0) ^ ESP.getChipId() ^ micros(); }
  inline void     setHostName(const char *n) { WiFi.hostname(n); }
  inline void     mdnsTick()           { MDNS.update(); }
  inline void     ntpConfigure(const char *tz, const char *s1, const char *s2) { configTime(tz, s1, s2); }
  inline bool     fsBegin()            { return LittleFS.begin(); }
  inline void     tcpSetTimeoutMs(WiFiClient &c, uint32_t ms) { c.setTimeout(ms); }                  // на ESP8266 таймаут в миллисекундах

#else
  #error "Поддерживаются только ESP8266 и ESP32 (выбери профиль в platformio.ini)"
#endif

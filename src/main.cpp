/*
 * 🐱 CatLaser — лазерная игрушка для кошки на ESP8266 (Wemos D1 mini), одна или две серво.
 *
 * Модули (src/):
 *   hw.h          пины и тайминги             config.*    настройки в EEPROM и автосохранение
 *   servo_ctl.*   серво, калибровка, плавность, парковка
 *   motion.*      режимы движения, "Авто", разгон/затухание и финал сессии
 *   laser.*       режимы лазера               stats.*     статистика и время (NTP)
 *   session.*     дежурный режим              mqtt_ctl.*  умный дом (MQTT)
 *   web.*         веб-сервер и API; интерфейс лежит в data/ (LittleFS)
 *
 * Подключение:
 *   Серво PAN  (горизонталь): сигнал -> D4
 *   Серво TILT (вертикаль)  : сигнал -> D5
 *   Серво: красный -> 5V, коричневый/чёрный -> GND (GND общий с платой!)
 *   Лазер: управляющий пин D1 (через транзистор/резистор). Если лазер горит постоянно — LASER_PIN = -1 в hw.h
 *
 * Первый запуск: точка доступа "CatLaser-Setup" -> выбираешь свой WiFi.
 * Дальше: http://catlaser.local или IP из монитора порта.
 * Веб-интерфейс лежит в data/: загрузи его командой  pio run -t uploadfs
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>

#include "hw.h"
#include "config.h"
#include "servo_ctl.h"
#include "motion.h"
#include "laser.h"
#include "stats.h"
#include "session.h"
#include "mqtt_ctl.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  Serial.println();

  cfgLoad();
  randomSeed(analogRead(A0) ^ ESP.getChipId() ^ micros());

  if (!LittleFS.begin()) Serial.println(F("LittleFS не запустилась"));
  servoInit();
  laserInit();
  statsInit();

  WiFi.hostname(HOSTNAME);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setTitle("Cat Laser");
  if (!wm.autoConnect(AP_NAME)) {
    // WiFi не настроен — работаем как точка доступа, чтобы игрушка всё равно была доступна
    Serial.println(F("WiFi не подключен, запускаю собственную точку доступа CatLaser"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CatLaser");
  }
  Serial.print(F("IP: "));
  Serial.println(WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP());

  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() {
    servoDetachAll();
    laserOff();
    if (ArduinoOTA.getCommand() == U_FS) LittleFS.end();   // обновление файловой системы по воздуху
  });
  ArduinoOTA.begin();

  webInit();
  mqSetup();

  // по умолчанию стартуем в безопасном режиме "Стоп", если не включён автозапуск
  if (cfg.autoStart && cfg.bootMode != M_OFF && cfg.bootMode <= M_LAST) setMode((Mode)cfg.bootMode);
  else setMode(M_OFF);
}

void loop() {
  webLoop();
  MDNS.update();
  ArduinoOTA.handle();
  updatePark();
  motionLoop();
  laserLoop();
  sessLoop();
  statsLoop();
  mqLoop();
  cfgLoop();
  yield();
}

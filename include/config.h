#pragma once
#include "common.h"

// ===================== Основные настройки (EEPROM, адрес 0) =====================
struct Settings {
  uint16_t magic;
  uint8_t  panEn, tiltEn;      // какие серво включены
  uint8_t  xMin, xMax;         // зона по горизонтали
  uint8_t  yMin, yMax;         // зона по вертикали
  uint8_t  speed;              // 1..10
  uint16_t pauseMin;           // мс
  uint16_t pauseMax;           // мс
  uint8_t  laserMode;
  uint8_t  laserBright;        // 10..100 %
  uint8_t  laserRate;          // 1..10
  uint8_t  invX, invY;         // инверсия осей
  uint16_t timerMin;           // минут, 0 = без таймера
  uint16_t autoSec;            // период смены режима в "Авто"
  uint8_t  autoStart;          // запускать при включении
  uint8_t  bootMode;           // какой режим запускать
};

// ===================== Умный дом / MQTT (EEPROM, адрес 64) =====================
struct NetCfg {
  uint16_t magic;
  uint8_t  mqEn;        // MQTT включён
  uint8_t  onMode;      // режим при включении по MQTT: 0 = последний использованный
  uint8_t  lastMode;    // последний запущенный режим
  uint16_t mqPort;
  char     host[40];
  char     user[32];
  char     pass[48];
  char     topic[64];
};

// ===================== Дежурный режим (EEPROM, адрес 320) =====================
struct SessCfg {
  uint16_t magic;
  uint8_t  en;
  uint8_t  mode;       // 0 = "Авто"
  uint16_t restMin;    // пауза между играми, минут: от
  uint16_t restMax;    // до
  uint16_t playMin;    // длительность игры, минут
};

// ===================== Расширения (EEPROM, адрес 400) =====================
struct FeatCfg {
  uint16_t magic;
  // --- поведение игры (всё опционально) ---
  uint8_t  smoothEn;        // плавный разгон и торможение серво
  uint8_t  smoothAcc;       // 1..10 — ускорение (чем больше, тем резче)
  uint8_t  finaleEn;        // финал игры: замедление, замирание, плавное гашение лазера
  uint8_t  finaleSec;       // длительность финала, секунд
  uint8_t  rampEn;          // разгон и затухание сессии
  uint8_t  rampPct;         // какую долю сессии занимает разгон (и столько же затухание), %
  uint8_t  playlistEn;      // свой плейлист в "Авто"
  uint8_t  weights[24];     // вес режима в плейлисте (0 = не играть), индекс = номер режима
  // --- калибровка серво (0 = горизонталь, 1 = вертикаль) ---
  uint16_t minUs[2];        // импульс для 0°
  uint16_t maxUs[2];        // импульс для 180°
  int8_t   offset[2];       // смещение центра, градусы
  // --- статистика и время ---
  uint8_t  statsEn;
  char     tz[40];          // часовой пояс (POSIX TZ)
};

extern Settings cfg;
extern NetCfg   mqc;
extern SessCfg  sess;
extern FeatCfg  feat;

void cfgLoad();
void cfgSaveNow();      // записать всё в EEPROM немедленно
void cfgMarkDirty();    // отложенное автосохранение
void cfgLoop();         // вызывать из loop()
void cfgFlush();        // сохранить, если есть несохранённые изменения

// ===================== Удобные хелперы по настройкам =====================
inline bool  enAx(int i)  { return i == 0 ? cfg.panEn : cfg.tiltEn; }
inline bool  twoAx()      { return cfg.panEn && cfg.tiltEn; }
inline float xLo()  { return cfg.xMin; }
inline float xHi()  { return cfg.xMax; }
inline float yLo()  { return cfg.yMin; }
inline float yHi()  { return cfg.yMax; }
inline float xMid() { return (cfg.xMin + cfg.xMax) / 2.0f; }
inline float yMid() { return (cfg.yMin + cfg.yMax) / 2.0f; }
inline float xAmp() { return (cfg.xMax - cfg.xMin) / 2.0f; }
inline float yAmp() { return (cfg.yMax - cfg.yMin) / 2.0f; }

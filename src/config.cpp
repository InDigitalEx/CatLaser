#include "config.h"
#include "hw.h"
#include <EEPROM.h>

Settings cfg;
NetCfg   mqc;
SessCfg  sess;
FeatCfg  feat;

static const uint16_t CFG_MAGIC  = 0xCA74;
static const uint16_t NET_MAGIC  = 0xCA55;
static const uint16_t SESS_MAGIC = 0xCA66;
static const uint16_t FEAT_MAGIC = 0xCA77;

static const int NET_ADDR  = 64;
static const int SESS_ADDR = 320;
static const int FEAT_ADDR = 400;
static const int EEPROM_SIZE = 1024;

static_assert(sizeof(Settings) <= NET_ADDR, "Settings залезает на область NetCfg");
static_assert(NET_ADDR + sizeof(NetCfg) <= SESS_ADDR, "NetCfg залезает на область SessCfg");
static_assert(SESS_ADDR + sizeof(SessCfg) <= FEAT_ADDR, "SessCfg залезает на область FeatCfg");
static_assert(FEAT_ADDR + sizeof(FeatCfg) <= EEPROM_SIZE, "FeatCfg не помещается в EEPROM");

static bool     dirty = false;
static uint32_t dirtyAt = 0;

// ---------- значения по умолчанию ----------
static void defaultCfg() {
  cfg.magic = CFG_MAGIC;
  cfg.panEn = 1;  cfg.tiltEn = 1;
  cfg.xMin = 30;  cfg.xMax = 150;
  cfg.yMin = 50;  cfg.yMax = 110;
  cfg.speed = 5;
  cfg.pauseMin = 300;
  cfg.pauseMax = 2000;
  cfg.laserMode = L_ON;
  cfg.laserBright = 100;
  cfg.laserRate = 5;
  cfg.invX = 0;
  cfg.invY = 0;
  cfg.timerMin = 10;
  cfg.autoSec = 20;
  cfg.autoStart = 0;
  cfg.bootMode = M_OFF;
}

static void defaultNet() {
  memset(&mqc, 0, sizeof(mqc));
  mqc.magic = NET_MAGIC;
  mqc.mqPort = 1883;
  mqc.onMode = 0;
  mqc.lastMode = M_AUTO;
  strcpy(mqc.topic, "home/catlaser/power");
}

static void defaultSess() {
  memset(&sess, 0, sizeof(sess));
  sess.magic = SESS_MAGIC;
  sess.en = 0;
  sess.mode = M_AUTO;
  sess.restMin = 30;
  sess.restMax = 90;
  sess.playMin = 5;
}

static void defaultFeat() {
  memset(&feat, 0, sizeof(feat));
  feat.magic = FEAT_MAGIC;
  feat.smoothEn = 0;   feat.smoothAcc = 5;
  feat.finaleEn = 0;   feat.finaleSec = 10;
  feat.rampEn = 0;     feat.rampPct = 25;
  feat.playlistEn = 0;
  for (int i = 0; i < 24; i++) feat.weights[i] = 1;
  for (int i = 0; i < 2; i++) {
    feat.minUs[i] = CAL_MIN_US_DEF;
    feat.maxUs[i] = CAL_MAX_US_DEF;
    feat.offset[i] = 0;
  }
  feat.statsEn = 1;
  strcpy(feat.tz, "EET-2EEST,M3.5.0/3,M10.5.0/4");   // Хельсинки
}

template <typename T> static T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------- загрузка / сохранение ----------
void cfgLoad() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  if (cfg.magic != CFG_MAGIC) defaultCfg();

  EEPROM.get(NET_ADDR, mqc);
  if (mqc.magic != NET_MAGIC) defaultNet();
  mqc.host[sizeof(mqc.host) - 1] = 0;          // защита от мусора в строках
  mqc.user[sizeof(mqc.user) - 1] = 0;
  mqc.pass[sizeof(mqc.pass) - 1] = 0;
  mqc.topic[sizeof(mqc.topic) - 1] = 0;

  EEPROM.get(SESS_ADDR, sess);
  if (sess.magic != SESS_MAGIC) defaultSess();
  if (sess.restMin < 1) sess.restMin = 1;
  if (sess.restMax < sess.restMin) sess.restMax = sess.restMin;
  if (sess.playMin < 1) sess.playMin = 1;

  EEPROM.get(FEAT_ADDR, feat);
  if (feat.magic != FEAT_MAGIC) defaultFeat();
  feat.smoothAcc = clampv<uint8_t>(feat.smoothAcc, 1, 10);
  feat.finaleSec = clampv<uint8_t>(feat.finaleSec, 3, 60);
  feat.rampPct   = clampv<uint8_t>(feat.rampPct, 5, 45);
  for (int i = 0; i < 24; i++) if (feat.weights[i] > 10) feat.weights[i] = 10;
  for (int i = 0; i < 2; i++) {
    feat.minUs[i]  = clampv<uint16_t>(feat.minUs[i], 400, 1500);
    feat.maxUs[i]  = clampv<uint16_t>(feat.maxUs[i], 1500, 2700);
    feat.offset[i] = clampv<int8_t>(feat.offset[i], -30, 30);
  }
  feat.tz[sizeof(feat.tz) - 1] = 0;
}

void cfgSaveNow() {
  EEPROM.put(0, cfg);
  EEPROM.put(NET_ADDR, mqc);
  EEPROM.put(SESS_ADDR, sess);
  EEPROM.put(FEAT_ADDR, feat);
  EEPROM.commit();
  dirty = false;
}

// автосохранение с задержкой — не изнашиваем flash на каждом движении ползунка
void cfgMarkDirty() {
  dirty = true;
  dirtyAt = millis();
}

void cfgLoop() {
  if (dirty && millis() - dirtyAt > AUTOSAVE_MS) cfgSaveNow();
}

void cfgFlush() {
  if (dirty) cfgSaveNow();
}

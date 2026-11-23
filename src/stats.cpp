#include "stats.h"
#include "config.h"
#include "hw.h"
#include "motion.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <time.h>

// Статистика хранится в файле /stats.bin на LittleFS (с выравниванием износа).
// Внимание: "pio run -t uploadfs" перезаписывает файловую систему и обнуляет статистику.

struct StatsData {
  uint32_t magic;
  uint32_t totalSec;        // всего секунд игры
  uint32_t sessions;        // сколько раз игрушку включали из состояния "стоп"
  uint32_t runSec[MODE_COUNT];   // секунд в каждом режиме (для "Авто" — по реально игравшему под-режиму)
  uint32_t runCnt[MODE_COUNT];   // сколько раз режим запускался
  uint32_t dayId[7];        // кольцо из последних 7 дней: номер дня (дни с 1970 года)
  uint32_t daySec[7];       // секунд игры в этот день
};

static const uint32_t STATS_MAGIC = 0x53544132;
static const char *STATS_FILE = "/stats.bin";

static StatsData sd;
static bool     dirty = false;
static bool     flushReq = false;
static bool     ntpStarted = false;
static uint32_t lastTick = 0;
static uint32_t lastSave = 0;

static void statsClear() {
  memset(&sd, 0, sizeof(sd));
  sd.magic = STATS_MAGIC;
}

static void statsSave() {
  File f = LittleFS.open(STATS_FILE, "w");
  if (!f) return;
  f.write((const uint8_t *)&sd, sizeof(sd));
  f.close();
  dirty = false;
  flushReq = false;
  lastSave = millis();
}

void statsInit() {
  statsClear();
  File f = LittleFS.open(STATS_FILE, "r");
  if (f) {
    if (f.size() == sizeof(sd)) {
      StatsData tmp;
      if (f.read((uint8_t *)&tmp, sizeof(tmp)) == sizeof(tmp) && tmp.magic == STATS_MAGIC) sd = tmp;
    }
    f.close();
  }
  lastTick = millis();
  lastSave = millis();
}

void statsReset() {
  statsClear();
  statsSave();
}

// ---------- время ----------
bool statsTimeSynced() { return time(nullptr) > 1700000000L; }

void statsTzChanged() { ntpStarted = false; }

static bool localNow(struct tm &tm) {
  time_t t = time(nullptr);
  if (t < 1700000000L) return false;
  localtime_r(&t, &tm);
  return true;
}

// номер дня (число дней с 1970-01-01) по гражданской дате
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

static bool todayId(uint32_t &id) {
  struct tm tm;
  if (!localNow(tm)) return false;
  id = (uint32_t)daysFromCivil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return true;
}

String statsClock() {
  struct tm tm;
  if (!localNow(tm)) return String("");
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  return String(buf);
}

// ---------- учёт ----------
static void addSeconds(uint32_t n) {
  sd.totalSec += n;
  if (run < MODE_COUNT) sd.runSec[run] += n;
  uint32_t id;
  if (todayId(id)) {
    uint8_t slot = id % 7;
    if (sd.dayId[slot] != id) { sd.dayId[slot] = id; sd.daySec[slot] = 0; }
    sd.daySec[slot] += n;
  }
  dirty = true;
}

void statsOnStart() {
  if (!feat.statsEn) return;
  sd.sessions++;
  dirty = true;
}

void statsOnRun(Mode m) {
  if (!feat.statsEn) return;
  if (m < MODE_COUNT) sd.runCnt[m]++;
  dirty = true;
}

void statsOnStop() {
  if (dirty) flushReq = true;
}

void statsLoop() {
  uint32_t now = millis();

  // NTP нужен только для статистики по дням
  if (feat.statsEn && !ntpStarted && WiFi.status() == WL_CONNECTED) {
    configTime(feat.tz, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }

  uint32_t n = (now - lastTick) / 1000;
  if (n) {
    lastTick += n * 1000;
    if (feat.statsEn && mode != M_OFF) addSeconds(n);
  }

  if (dirty && (flushReq || now - lastSave > STATS_SAVE_MS)) statsSave();
}

// ---------- JSON для веб-интерфейса ----------
String statsJson() {
  String j;
  j.reserve(1200);
  bool sync = statsTimeSynced();
  uint32_t today = 0;
  bool haveToday = todayId(today);

  uint32_t daySecs[7];
  uint8_t  wds[7];
  uint32_t week = 0, todaySec = 0;
  for (int k = 6; k >= 0; k--) {                        // индекс 0 = 6 дней назад, 6 = сегодня
    uint32_t sec = 0;
    uint8_t wd = 0;
    if (haveToday) {
      uint32_t id = today - k;
      uint8_t slot = id % 7;
      sec = (sd.dayId[slot] == id) ? sd.daySec[slot] : 0;
      wd = (id + 4) % 7;                                // 0 = воскресенье
    }
    daySecs[6 - k] = sec;
    wds[6 - k] = wd;
    week += sec;
    if (k == 0) todaySec = sec;
  }

  j += F("{\"en\":");       j += feat.statsEn;
  j += F(",\"sync\":");     j += (sync ? 1 : 0);
  j += F(",\"clock\":\"");  j += statsClock();
  j += F("\",\"total\":");  j += (unsigned long)sd.totalSec;
  j += F(",\"sessions\":"); j += (unsigned long)sd.sessions;
  j += F(",\"today\":");    j += (unsigned long)todaySec;
  j += F(",\"week\":");     j += (unsigned long)week;
  j += F(",\"days\":[");
  for (int i = 0; i < 7; i++) { if (i) j += ','; j += (unsigned long)daySecs[i]; }
  j += F("],\"wd\":[");
  for (int i = 0; i < 7; i++) { if (i) j += ','; j += (int)wds[i]; }
  j += F("],\"run\":[");
  for (int i = 0; i < MODE_COUNT; i++) { if (i) j += ','; j += (unsigned long)sd.runSec[i]; }
  j += F("],\"cnt\":[");
  for (int i = 0; i < MODE_COUNT; i++) { if (i) j += ','; j += (unsigned long)sd.runCnt[i]; }
  j += F("]}");
  return j;
}

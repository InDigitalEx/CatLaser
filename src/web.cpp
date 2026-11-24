#include "web.h"
#include "config.h"
#include "hw.h"
#include "motion.h"
#include "servo_ctl.h"
#include "stats.h"
#include "session.h"
#include "mqtt_ctl.h"
#include "platform.h"
#include <WiFiManager.h>

static HttpServer server(80);

// Если файловая система ещё не загружена, показываем подсказку вместо интерфейса
static const char NO_FS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Cat Laser</title>
<style>body{font-family:system-ui,sans-serif;background:#14111f;color:#f1eefc;padding:24px;max-width:520px;margin:auto;line-height:1.5}
code{background:#282340;padding:2px 8px;border-radius:6px}</style></head><body>
<h2>🐱 Cat Laser</h2>
<p>Прошивка работает, но веб-интерфейс ещё не загружен в файловую систему.</p>
<p>Выполни в папке проекта: <code>pio run -t uploadfs</code></p>
<p>API доступно: <a style="color:#ff7a45" href="/api/state">/api/state</a></p>
</body></html>)rawliteral";

// ===================== Вспомогательное =====================
static long argL(const char *name, long lo, long hi) {
  long v = server.arg(name).toInt();
  return v < lo ? lo : (v > hi ? hi : v);
}

static String jsonEsc(const char *in) {
  String o;
  for (; *in; in++) {
    if (*in == '"' || *in == '\\') o += '\\';
    if ((uint8_t)*in >= 0x20) o += *in;
  }
  return o;
}

static void copyStr(char *dst, size_t n, String v, bool trim) {
  if (trim) v.trim();
  size_t l = v.length();
  if (l >= n) l = n - 1;
  memcpy(dst, v.c_str(), l);
  dst[l] = 0;
}

// ===================== Состояние =====================
static void sendStateCode(int code) {
  int snext = sessNextSec();
  int left = (mode != M_OFF) ? (int)(sessionRemainingMs() / 1000) : 0;

  String j;
  j.reserve(1100);
  j += F("{\"mode\":");   j += (int)mode;
  j += F(",\"run\":");    j += (int)run;
  j += F(",\"pen\":");    j += cfg.panEn;
  j += F(",\"ten\":");    j += cfg.tiltEn;
  j += F(",\"xmin\":");   j += cfg.xMin;
  j += F(",\"xmax\":");   j += cfg.xMax;
  j += F(",\"ymin\":");   j += cfg.yMin;
  j += F(",\"ymax\":");   j += cfg.yMax;
  j += F(",\"speed\":");  j += cfg.speed;
  j += F(",\"pmin\":");   j += cfg.pauseMin;
  j += F(",\"pmax\":");   j += cfg.pauseMax;
  j += F(",\"lmode\":");  j += cfg.laserMode;
  j += F(",\"lbr\":");    j += cfg.laserBright;
  j += F(",\"lrate\":");  j += cfg.laserRate;
  j += F(",\"ix\":");     j += cfg.invX;
  j += F(",\"iy\":");     j += cfg.invY;
  j += F(",\"astart\":"); j += cfg.autoStart;
  j += F(",\"timer\":");  j += cfg.timerMin;
  j += F(",\"auto\":");   j += cfg.autoSec;
  j += F(",\"left\":");   j += left;
  j += F(",\"finale\":"); j += (motionFinale() ? 1 : 0);
  j += F(",\"x\":");      j += (int)ax[0].cur;
  j += F(",\"y\":");      j += (int)ax[1].cur;
  j += F(",\"mx\":");     j += (int)manualX;
  j += F(",\"my\":");     j += (int)manualY;
  // --- поведение игры ---
  j += F(",\"sm\":");     j += feat.smoothEn;
  j += F(",\"sacc\":");   j += feat.smoothAcc;
  j += F(",\"fin\":");    j += feat.finaleEn;
  j += F(",\"finsec\":"); j += feat.finaleSec;
  j += F(",\"ramp\":");   j += feat.rampEn;
  j += F(",\"rpct\":");   j += feat.rampPct;
  j += F(",\"plist\":");  j += feat.playlistEn;
  j += F(",\"wts\":[");
  for (int i = 0; i < 24; i++) { if (i) j += ','; j += feat.weights[i]; }
  j += ']';
  // --- калибровка серво ---
  j += F(",\"mn0\":");    j += feat.minUs[0];
  j += F(",\"mx0\":");    j += feat.maxUs[0];
  j += F(",\"of0\":");    j += (int)feat.offset[0];
  j += F(",\"mn1\":");    j += feat.minUs[1];
  j += F(",\"mx1\":");    j += feat.maxUs[1];
  j += F(",\"of1\":");    j += (int)feat.offset[1];
  // --- статистика и время ---
  j += F(",\"sten\":");   j += feat.statsEn;
  j += F(",\"tz\":\"");   j += jsonEsc(feat.tz);
  j += F("\",\"ip\":\""); j += WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  j += F("\",\"rssi\":"); j += WiFi.RSSI();
  // --- MQTT и дежурный режим ---
  j += F(",\"mqen\":");   j += mqc.mqEn;
  j += F(",\"mqok\":");   j += (mqIsConnected() ? 1 : 0);
  j += F(",\"mqrc\":");   j += mqRc();
  j += F(",\"se\":");     j += sess.en;
  j += F(",\"smode\":");  j += sess.mode;
  j += F(",\"rmin\":");   j += sess.restMin;
  j += F(",\"rmax\":");   j += sess.restMax;
  j += F(",\"play\":");   j += sess.playMin;
  j += F(",\"snext\":");  j += snext;
  j += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", j);
}

static void sendState() { sendStateCode(200); }

// ===================== /api/set =====================
static void handleSet() {
  bool ch = false;          // изменились настройки, которые надо сохранить
  bool recal = false;       // изменилась калибровка серво

  if (server.hasArg("pen")) {
    uint8_t v = argL("pen", 0, 1);
    if (v != cfg.panEn) { cfg.panEn = v; axisToggled(0); ch = true; }
  }
  if (server.hasArg("ten")) {
    uint8_t v = argL("ten", 0, 1);
    if (v != cfg.tiltEn) { cfg.tiltEn = v; axisToggled(1); ch = true; }
  }
  if (server.hasArg("xmin")) { int v = argL("xmin", 0, 175); cfg.xMin = min(v, (int)cfg.xMax - 5); ch = true; }
  if (server.hasArg("xmax")) { int v = argL("xmax", 5, 180); cfg.xMax = max(v, (int)cfg.xMin + 5); ch = true; }
  if (server.hasArg("ymin")) { int v = argL("ymin", 0, 175); cfg.yMin = min(v, (int)cfg.yMax - 5); ch = true; }
  if (server.hasArg("ymax")) { int v = argL("ymax", 5, 180); cfg.yMax = max(v, (int)cfg.yMin + 5); ch = true; }
  if (server.hasArg("speed")) { cfg.speed = argL("speed", 1, 10); ch = true; }
  if (server.hasArg("pmin")) {
    cfg.pauseMin = argL("pmin", 0, 20000);
    if (cfg.pauseMax < cfg.pauseMin) cfg.pauseMax = cfg.pauseMin;
    ch = true;
  }
  if (server.hasArg("pmax")) {
    int v = argL("pmax", 0, 20000);
    cfg.pauseMax = max(v, (int)cfg.pauseMin);
    ch = true;
  }
  if (server.hasArg("lmode")) { cfg.laserMode = argL("lmode", 0, L_MOVEBLINK); ch = true; }
  if (server.hasArg("lbr"))   { cfg.laserBright = argL("lbr", 10, 100); ch = true; }
  if (server.hasArg("lrate")) { cfg.laserRate = argL("lrate", 1, 10); ch = true; }
  if (server.hasArg("ix"))    { cfg.invX = argL("ix", 0, 1); ch = true; }
  if (server.hasArg("iy"))    { cfg.invY = argL("iy", 0, 1); ch = true; }
  if (server.hasArg("mx"))    manualX = argL("mx", 0, 180);
  if (server.hasArg("my"))    manualY = argL("my", 0, 180);
  if (server.hasArg("auto")) {
    cfg.autoSec = argL("auto", 5, 300);
    if (mode == M_AUTO) autoRearm();
    ch = true;
  }
  if (server.hasArg("timer")) {
    cfg.timerMin = argL("timer", 0, 600);
    if (mode != M_OFF) sessionBegin((uint32_t)cfg.timerMin * 60000UL);   // 0 = без ограничения
    ch = true;
  }
  if (server.hasArg("astart")) {
    cfg.autoStart = argL("astart", 0, 1);
    if (cfg.autoStart) cfg.bootMode = mode;   // запоминаем текущий режим
    ch = true;
  }

  // --- поведение игры ---
  if (server.hasArg("sm"))     { feat.smoothEn = argL("sm", 0, 1); ch = true; }
  if (server.hasArg("sacc"))   { feat.smoothAcc = argL("sacc", 1, 10); ch = true; }
  if (server.hasArg("fin"))    { feat.finaleEn = argL("fin", 0, 1); ch = true; }
  if (server.hasArg("finsec")) { feat.finaleSec = argL("finsec", 3, 60); ch = true; }
  if (server.hasArg("ramp"))   { feat.rampEn = argL("ramp", 0, 1); ch = true; }
  if (server.hasArg("rpct"))   { feat.rampPct = argL("rpct", 5, 45); ch = true; }
  if (server.hasArg("plist"))  { feat.playlistEn = argL("plist", 0, 1); ch = true; }
  if (server.hasArg("wts")) {                    // веса режимов через запятую: "1,1,0,3,..."
    String v = server.arg("wts");
    int idx = 0, start = 0;
    while (idx < 24 && start <= (int)v.length()) {
      int c = v.indexOf(',', start);
      String part = (c < 0) ? v.substring(start) : v.substring(start, c);
      long w = part.toInt();
      feat.weights[idx++] = w < 0 ? 0 : (w > 10 ? 10 : w);
      if (c < 0) break;
      start = c + 1;
    }
    ch = true;
  }

  // --- калибровка серво ---
  if (server.hasArg("mn0")) { feat.minUs[0] = argL("mn0", 400, 1500);  recal = true; }
  if (server.hasArg("mx0")) { feat.maxUs[0] = argL("mx0", 1500, 2700); recal = true; }
  if (server.hasArg("of0")) { feat.offset[0] = argL("of0", -30, 30);   recal = true; }
  if (server.hasArg("mn1")) { feat.minUs[1] = argL("mn1", 400, 1500);  recal = true; }
  if (server.hasArg("mx1")) { feat.maxUs[1] = argL("mx1", 1500, 2700); recal = true; }
  if (server.hasArg("of1")) { feat.offset[1] = argL("of1", -30, 30);   recal = true; }
  if (recal) { servoRecalibrate(); ch = true; }

  // --- статистика и часовой пояс ---
  if (server.hasArg("sten")) { feat.statsEn = argL("sten", 0, 1); ch = true; }
  if (server.hasArg("tz")) {
    copyStr(feat.tz, sizeof(feat.tz), server.arg("tz"), true);
    statsTzChanged();
    ch = true;
  }

  // --- дежурный режим ---
  if (server.hasArg("se"))    { sess.en = argL("se", 0, 1); sessReschedule(); ch = true; }
  if (server.hasArg("smode")) { sess.mode = argL("smode", 0, M_LAST); ch = true; }
  if (server.hasArg("rmin"))  {
    int v = argL("rmin", 1, 600);
    sess.restMin = v;
    if (sess.restMax < v) sess.restMax = v;
    sessReschedule(); ch = true;
  }
  if (server.hasArg("rmax"))  { int v = argL("rmax", 1, 600); sess.restMax = max(v, (int)sess.restMin); sessReschedule(); ch = true; }
  if (server.hasArg("play"))  { sess.playMin = argL("play", 1, 120); ch = true; }

  bool rejected = false;                        // режим для двух осей при выключенной серво не включаем
  if (server.hasArg("mode")) {
    Mode m = (Mode)argL("mode", 0, M_LAST);
    if (!modeAllowed(m)) rejected = true;
    else {
      setMode(m);
      if (cfg.autoStart) { cfg.bootMode = mode; ch = true; }
    }
  }

  if (server.hasArg("save")) cfgSaveNow();
  else if (ch) cfgMarkDirty();
  sendStateCode(rejected ? 409 : 200);
}

// ===================== MQTT =====================
static void sendMqCfg() {
  String j;
  j.reserve(320);
  j += F("{\"en\":");        j += mqc.mqEn;
  j += F(",\"host\":\"");    j += jsonEsc(mqc.host);
  j += F("\",\"port\":");    j += mqc.mqPort;
  j += F(",\"user\":\"");    j += jsonEsc(mqc.user);
  j += F("\",\"haspass\":"); j += (mqc.pass[0] ? 1 : 0);
  j += F(",\"topic\":\"");   j += jsonEsc(mqc.topic);
  j += F("\",\"onmode\":");  j += mqc.onMode;
  j += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

static void handleMqPost() {
  if (server.hasArg("topic")) {                    // проверяем топик до любых изменений
    String t = server.arg("topic");
    t.trim();
    if (t.length() == 0 || t.length() > 60 || t.indexOf('#') >= 0 || t.indexOf('+') >= 0) {
      server.send(400, "text/plain", "bad topic");
      return;
    }
  }
  mqGoOffline();                                   // сообщаем "offline" в старый топик
  if (server.hasArg("en"))     mqc.mqEn = argL("en", 0, 1);
  if (server.hasArg("host"))   copyStr(mqc.host, sizeof(mqc.host), server.arg("host"), true);
  if (server.hasArg("port"))   mqc.mqPort = argL("port", 1, 65535);
  if (server.hasArg("user"))   copyStr(mqc.user, sizeof(mqc.user), server.arg("user"), true);
  if (server.hasArg("pass") && server.arg("pass").length()) copyStr(mqc.pass, sizeof(mqc.pass), server.arg("pass"), false);
  if (server.hasArg("clrpass") && server.arg("clrpass").toInt()) mqc.pass[0] = 0;
  if (server.hasArg("topic"))  copyStr(mqc.topic, sizeof(mqc.topic), server.arg("topic"), true);
  if (server.hasArg("onmode")) mqc.onMode = argL("onmode", 0, M_LAST);
  cfgSaveNow();                                    // настройки подключения сохраняем сразу
  mqApply();
  sendMqCfg();
}

// ===================== Статический интерфейс из LittleFS =====================
static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    server.send_P(200, "text/html; charset=utf-8", NO_FS_HTML);
    return;
  }
  server.streamFile(f, "text/html; charset=utf-8");
  f.close();
}

void webInit() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/set", HTTP_GET, handleSet);
  server.on("/api/mqtt", HTTP_GET, sendMqCfg);
  server.on("/api/mqtt", HTTP_POST, handleMqPost);
  server.on("/api/servotest", HTTP_GET, []() {       // проверка калибровки: 0 = минимум, 1 = центр, 2 = максимум
    servoTest(argL("axis", 0, 1), argL("pos", 0, 2));
    sendState();
  });
  server.on("/api/stats", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", statsJson());
  });
  server.on("/api/stats/reset", HTTP_POST, []() {
    statsReset();
    server.send(200, "application/json", statsJson());
  });
  server.on("/api/wifi-reset", HTTP_GET, []() {
    server.send(200, "text/plain", "ok");
    delay(300);
    cfgFlush();
    WiFiManager w;
    w.resetSettings();
    ESP.restart();
  });
  server.serveStatic("/app.js", LittleFS, "/app.js", "no-cache");
  server.serveStatic("/style.css", LittleFS, "/style.css", "no-cache");
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();
}

void webLoop() { server.handleClient(); }

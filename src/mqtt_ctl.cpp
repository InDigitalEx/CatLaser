#include "mqtt_ctl.h"
#include "config.h"
#include "motion.h"
#include "session.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

static WiFiClient   mqNet;
static PubSubClient mq(mqNet);
static uint32_t mqNextTry = 0;
static uint32_t mqBackoff = 5000;
static int8_t   mqLastPub = -1;
static int8_t   mqLastAuto = -1;

static String topicState()     { return String(mqc.topic) + "/state"; }
static String topicLwt()       { return String(mqc.topic) + "/lwt"; }
static String topicAuto()      { return String(mqc.topic) + "/auto"; }
static String topicAutoState() { return String(mqc.topic) + "/auto/state"; }

static void publishState(bool force) {
  if (!mq.connected()) return;
  int8_t on = (mode != M_OFF) ? 1 : 0;
  if (!force && on == mqLastPub) return;
  if (mq.publish(topicState().c_str(), on ? "ON" : "OFF", true)) mqLastPub = on;
}

static void publishAuto(bool force) {
  if (!mq.connected()) return;
  int8_t on = sess.en ? 1 : 0;
  if (!force && on == mqLastAuto) return;
  if (mq.publish(topicAutoState().c_str(), on ? "ON" : "OFF", true)) mqLastAuto = on;
}

static void powerSet(bool on) {
  if (on) {
    if (mode != M_OFF) return;
    Mode m = mqc.onMode ? (Mode)mqc.onMode : (Mode)mqc.lastMode;
    if (m == M_OFF || m > M_LAST || !modeAllowed(m)) m = M_AUTO;
    setMode(m);
  } else if (mode != M_OFF) {
    setMode(M_OFF);
  }
}

static void callback(char *topic, byte *payload, unsigned int len) {
  char buf[16];
  unsigned int n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = 0;
  String v(buf);
  v.trim();
  v.toUpperCase();
  int8_t cmd;                                   // 1 = ON, 0 = OFF, 2 = TOGGLE
  if (v == "ON" || v == "1" || v == "TRUE") cmd = 1;
  else if (v == "OFF" || v == "0" || v == "FALSE") cmd = 0;
  else if (v == "TOGGLE") cmd = 2;
  else return;
  if (String(topic) == topicAuto()) sessSet(cmd == 2 ? !sess.en : cmd == 1);   // <топик>/auto — дежурный режим
  else powerSet(cmd == 2 ? (mode == M_OFF) : cmd == 1);                        // <топик> — питание
}

void mqSetup() {
  mq.setServer(mqc.host, mqc.mqPort);
  mq.setCallback(callback);
  mq.setSocketTimeout(2);
  mqNet.setTimeout(2000);     // не зависаем надолго, если брокер недоступен
}

void mqGoOffline() {          // вызывать ДО смены настроек, пока в mqc ещё старый топик
  if (!mq.connected()) return;
  mq.publish(topicLwt().c_str(), "offline", true);
  mq.disconnect();
}

void mqApply() {              // применить новые настройки
  if (mq.connected()) mq.disconnect();
  mq.setServer(mqc.host, mqc.mqPort);
  mqNextTry = 0;
  mqBackoff = 5000;
  mqLastPub = -1;
  mqLastAuto = -1;
}

static bool connectNow() {
  String id = "catlaser-" + String(ESP.getChipId(), HEX);
  String lwt = topicLwt();
  bool ok;
  if (mqc.user[0]) ok = mq.connect(id.c_str(), mqc.user, mqc.pass, lwt.c_str(), 0, true, "offline");
  else             ok = mq.connect(id.c_str(), lwt.c_str(), 0, true, "offline");
  if (ok) {
    mq.subscribe(mqc.topic);
    mq.subscribe(topicAuto().c_str());
    mq.publish(lwt.c_str(), "online", true);
    mqLastPub = -1;
    mqLastAuto = -1;
    publishState(true);
    publishAuto(true);
  }
  return ok;
}

void mqLoop() {
  bool want = mqc.mqEn && mqc.host[0] && mqc.topic[0] && WiFi.status() == WL_CONNECTED;
  if (!want) {
    if (mq.connected()) mq.disconnect();
    return;
  }
  if (mq.connected()) {
    mq.loop();
    publishState(false);      // состояние изменилось из веба/по таймеру — сообщаем в умный дом
    publishAuto(false);
    return;
  }
  uint32_t now = millis();
  if ((int32_t)(now - mqNextTry) < 0) return;
  mqNextTry = now + mqBackoff;
  if (connectNow()) mqBackoff = 5000;
  else mqBackoff = (mqBackoff * 2 > 60000) ? 60000 : mqBackoff * 2;   // не долбим недоступный брокер
}

bool mqIsConnected() { return mq.connected(); }
int  mqRc()          { return mq.state(); }

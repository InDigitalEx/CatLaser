#include "session.h"
#include "config.h"
#include "motion.h"

static bool     sessArmed = false;   // следующая игра запланирована
static bool     sessWasOn = false;
static uint32_t sessNext = 0;

void sessSet(bool on) {
  sess.en = on ? 1 : 0;
  sessArmed = false;
  cfgMarkDirty();
}

void sessReschedule() { sessArmed = false; }

int sessNextSec() {
  if (!(sess.en && mode == M_OFF && sessArmed)) return -1;
  int32_t ms = (int32_t)(sessNext - millis());
  return ms > 0 ? ms / 1000 : 0;
}

void sessLoop() {
  uint32_t now = millis();
  bool on = (mode != M_OFF);
  if (!sess.en) { sessArmed = false; sessWasOn = on; return; }
  if (on) { sessArmed = false; sessWasOn = true; return; }     // игра идёт — ничего не планируем
  if (sessWasOn || !sessArmed) {                                // только что остановились или график не задан
    sessNext = now + (uint32_t)rnd(sess.restMin, sess.restMax) * 60000UL;
    sessArmed = true;
    sessWasOn = false;
    return;
  }
  if ((int32_t)(now - sessNext) >= 0) {
    setMode(sess.mode ? (Mode)sess.mode : M_AUTO);               // недоступный режим setMode заменит на "Авто"
    if (mode != M_OFF && sess.playMin) sessionBegin((uint32_t)sess.playMin * 60000UL);
    sessArmed = false;
    sessWasOn = true;
  }
}

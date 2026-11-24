#include "servo_ctl.h"
#include "config.h"
#include "hw.h"
#include "motion.h"
#include <math.h>

Axis     ax[2];
uint32_t lastMotionMs = 0;
bool     parkPending = false;
static uint32_t parkLast = 0;

void servoInit() {
#if defined(PLATFORM_ESP32)
  ESP32PWM::allocateTimer(0);     // аппаратные таймеры ШИМ под две серво
  ESP32PWM::allocateTimer(1);
#endif
  ax[0].pin = PAN_PIN;
  ax[1].pin = TILT_PIN;
  for (int i = 0; i < 2; i++) {
    ax[i].cur = ax[i].tgt = 90;
    ax[i].spd = 100;
    ax[i].vel = 0;
    ax[i].lastUs = 0;
    ax[i].lastMoveMs = 0;
    ax[i].parking = false;
    ax[i].parkHold = false;
    ax[i].parkDoneAt = 0;
  }
}

void servoDetachAll() {
  for (int i = 0; i < 2; i++) {
    ax[i].servo.detach();
    ax[i].lastUs = 0;
  }
}

// после смены калибровки серво надо заново подключить с новыми границами импульса
void servoRecalibrate() {
  for (int i = 0; i < 2; i++) {
    if (ax[i].servo.attached()) ax[i].servo.detach();
    ax[i].lastUs = 0;
  }
}

static void attachAxis(int i) {
  if (!ax[i].servo.attached()) {
    ax[i].lastUs = 0;
    ax[i].servo.attach(ax[i].pin, feat.minUs[i], feat.maxUs[i]);
  }
}

// отправить на серво текущее положение (без проверки, включена ли ось); true, если команда ушла
bool writeAxis(int i) {
  Axis &a = ax[i];
  bool inv = (i == 0) ? cfg.invX : cfg.invY;
  float ang = inv ? 180.0f - a.cur : a.cur;
  ang += feat.offset[i];                                // калибровка: смещение центра
  ang = constrain(ang, 0.0f, 180.0f);
  int us = feat.minUs[i] + (int)((ang / 180.0f) * (feat.maxUs[i] - feat.minUs[i]));   // калибровка: границы импульса
  if (abs(us - a.lastUs) < SERVO_DEADBAND_US) return false;   // сдвиг слишком мал — команду не шлём
  attachAxis(i);
  a.servo.writeMicroseconds(us);
  a.lastUs = us;
  a.lastMoveMs = millis();
  return true;
}

void applyAxis(int i) {
  Axis &a = ax[i];
  if (!enAx(i)) {                       // выключенную серво не трогаем вообще
    if (a.servo.attached()) a.servo.detach();
    return;
  }
  if (!writeAxis(i) && a.servo.attached() && millis() - a.lastMoveMs > IDLE_DETACH_MS) {
    a.servo.detach();                   // стоим на месте — отпускаем серво, чтобы не дребезжало
  }
}

// двигаем включённые оси к целям; true когда все доехали
bool stepAxes(float dt) {
  bool done = true, moved = false;

  float vmaxSpd = 1;                    // самая быстрая из осей — по ней масштабируем ускорение, чтобы линии оставались прямыми
  for (int i = 0; i < 2; i++) if (enAx(i) && ax[i].spd > vmaxSpd) vmaxSpd = ax[i].spd;
  float A = feat.smoothAcc * 150.0f;    // 150..1500 град/с²

  for (int i = 0; i < 2; i++) {
    if (!enAx(i)) continue;
    Axis &a = ax[i];
    float d = a.tgt - a.cur;
    float dist = fabsf(d);

    if (!feat.smoothEn) {               // обычное линейное движение
      float s = a.spd * dt;
      if (dist <= s) {
        if (d != 0) moved = true;
        a.cur = a.tgt;
      } else {
        a.cur += (d > 0) ? s : -s;
        moved = true;
        done = false;
      }
      continue;
    }

    // --- плавный режим: ускорение и торможение ---
    if (dist < 0.15f) {
      if (dist > 0) moved = true;
      a.cur = a.tgt;
      a.vel = 0;
      continue;
    }
    float Ai = A * (a.spd / vmaxSpd);
    if (Ai < 30) Ai = 30;
    float vstop = sqrtf(2.0f * Ai * dist);              // с какой скорости ещё успеем остановиться у цели
    float vdes = a.spd < vstop ? a.spd : vstop;
    float vtar = (d > 0 ? 1.0f : -1.0f) * vdes;
    float dv = Ai * dt;
    if (a.vel < vtar - dv) a.vel += dv;
    else if (a.vel > vtar + dv) a.vel -= dv;
    else a.vel = vtar;
    a.cur += a.vel * dt;
    moved = true;
    float nd = a.tgt - a.cur;
    if (nd * d <= 0) {                  // проскочили цель — встаём точно в неё
      a.cur = a.tgt;
      a.vel = 0;
    } else {
      done = false;
    }
  }
  if (moved) lastMotionMs = millis();
  return done;
}

// прямая линия в точку: обе оси приедут одновременно
void moveTo(float x, float y, float speed) {
  if (!enAx(0)) x = ax[0].cur;
  if (!enAx(1)) y = ax[1].cur;
  x = constrain(x, 0.0f, 180.0f);
  y = constrain(y, 0.0f, 180.0f);
  speed = max(speed, 5.0f);
  float dx = x - ax[0].cur, dy = y - ax[1].cur;
  float dist = sqrtf(dx * dx + dy * dy);
  ax[0].tgt = x;
  ax[1].tgt = y;
  if (dist < 0.05f) {
    ax[0].spd = ax[1].spd = speed;
    return;
  }
  float T = dist / speed;
  ax[0].spd = max(1.0f, fabsf(dx) / T);
  ax[1].spd = max(1.0f, fabsf(dy) / T);
}

// быстро "догонять" постоянно меняющуюся цель (параметрические режимы)
void chase(float x, float y, float speed) {
  if (!enAx(0)) x = ax[0].cur;
  if (!enAx(1)) y = ax[1].cur;
  ax[0].tgt = constrain(x, 0.0f, 180.0f);
  ax[1].tgt = constrain(y, 0.0f, 180.0f);
  ax[0].spd = ax[1].spd = speed;
}

// ===================== Парковка (середина зоны / тест калибровки) =====================
bool parkBusy() { return ax[0].parking || ax[1].parking; }

void startPark(int i, float target) {
  Axis &a = ax[i];
  a.tgt = target >= 0 ? target : (PARK_AT_ZONE_MID ? (i == 0 ? xMid() : yMid()) : 90.0f);
  a.spd = PARK_SPEED;
  a.vel = 0;
  a.parking = true;
  a.parkHold = false;
  parkLast = millis();
  writeAxis(i);               // сразу подаём сигнал на текущее положение, чтобы серво не дёрнулась в случайную точку
}

void updatePark() {
  if (!parkBusy()) return;
  uint32_t now = millis();
  if (now - parkLast < MOTION_STEP_MS) return;
  float dt = (now - parkLast) / 1000.0f;
  parkLast = now;
  if (dt > 0.1f) dt = 0.1f;

  for (int i = 0; i < 2; i++) {
    Axis &a = ax[i];
    if (!a.parking) continue;
    if (!a.parkHold) {
      float d = a.tgt - a.cur, s = PARK_SPEED * dt;
      if (fabsf(d) <= s) { a.cur = a.tgt; a.parkHold = true; a.parkDoneAt = now + PARK_HOLD_MS; }
      else a.cur += (d > 0) ? s : -s;
      writeAxis(i);
      lastMotionMs = now;
    } else if ((int32_t)(now - a.parkDoneAt) >= 0) {
      a.parking = false;
      a.parkHold = false;
      if (!enAx(i) || mode == M_OFF) a.servo.detach();     // выключенная или простаивающая серво остаётся на месте без сигнала
    }
  }
  if (!parkBusy() && parkPending) {
    parkPending = false;
    motionRestartAfterPark();
  }
}

// тест калибровки: серво ставится в минимум / центр / максимум (с учётом калибровки)
void servoTest(int i, int pos) {
  float ang = (pos <= 0) ? 0.0f : (pos >= 2 ? 180.0f : 90.0f);
  startPark(i, ang);
  parkPending = (mode != M_OFF);
}

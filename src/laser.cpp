#include "laser.h"
#include "config.h"
#include "hw.h"
#include "motion.h"
#include "servo_ctl.h"
#include <math.h>

static int16_t  lastLaserVal = -1;
static uint32_t lastLaserMs = 0;
static float    laserPh = 0;
static bool     flickerOn = true;
static uint32_t flickerUntil = 0;
static uint8_t  sosIdx = 0;
static uint32_t sosUntil = 0;
static bool     laserPwm = false;   // сейчас на пине работает программный ШИМ

void laserInit() {
  if (LASER_PIN < 0) return;
  pinMode(LASER_PIN, OUTPUT);
  analogWriteRange(255);
  analogWrite(LASER_PIN, 0);
}

// Программный ШИМ ESP8266 делит один генератор импульсов с серво, и пока серво двигаются,
// яркость плавает. Поэтому "горит" и "не горит" — обычный digitalWrite без ШИМ,
// а ШИМ включается только для промежуточных значений (пульс, яркость меньше 100%, плавное гашение в финале).
static void laserSet(uint8_t v) {
  if (LASER_PIN < 0 || v == lastLaserVal) return;   // не дёргаем пин зря
  lastLaserVal = v;
  if (v == 0 || v >= 255) {
    if (laserPwm) {                                  // сначала гасим ШИМ-волну на пине
      analogWrite(LASER_PIN, 0);
      laserPwm = false;
    }
    digitalWrite(LASER_PIN, v ? HIGH : LOW);
  } else {
    laserPwm = true;
    analogWrite(LASER_PIN, v);
  }
}

void laserOff() {
  lastLaserVal = -1;
  laserSet(0);
}

void laserLoop() {
  uint32_t now = millis();
  if (now - lastLaserMs < 15) return;
  float dt = (now - lastLaserMs) / 1000.0f;
  lastLaserMs = now;

  if (mode == M_OFF || cfg.laserMode == L_OFF) {
    laserSet(0);
    return;
  }

  bool moving = (now - lastMotionMs) < 150;
  float duty = 0;
  float f;

  switch (cfg.laserMode) {
    case L_ON:
      duty = 1;
      break;
    case L_BLINK:
      f = 1.0f + cfg.laserRate * 0.5f;           // 1.5..6 Гц
      laserPh += f * dt;
      laserPh -= floorf(laserPh);
      duty = (laserPh < 0.5f) ? 1 : 0;
      break;
    case L_PULSE: {
      f = 0.2f + cfg.laserRate * 0.15f;          // 0.35..1.7 Гц
      laserPh += f * dt;
      laserPh -= floorf(laserPh);
      float s = 0.5f + 0.5f * sinf(laserPh * 6.2832f);
      duty = s * s;
      break;
    }
    case L_STROBE:
      f = 6.0f + cfg.laserRate * 2.0f;           // 8..26 Гц
      laserPh += f * dt;
      laserPh -= floorf(laserPh);
      duty = (laserPh < 0.5f) ? 1 : 0;
      break;
    case L_FLICKER:
      if ((int32_t)(now - flickerUntil) >= 0) {
        flickerOn = random(100) < 70;
        flickerUntil = now + rnd(30, 250);
      }
      duty = flickerOn ? 1 : 0;
      break;
    case L_STILL:
      duty = moving ? 0 : 1;
      break;
    case L_MOVING:
      duty = moving ? 1 : 0;
      break;
    case L_HEART:                                   // тук-тук: двойная вспышка и пауза
      f = 0.6f + cfg.laserRate * 0.15f;             // 0.75..2.1 удара/с
      laserPh += f * dt;
      laserPh -= floorf(laserPh);
      duty = (laserPh < 0.10f || (laserPh >= 0.22f && laserPh < 0.32f)) ? 1 : 0;
      break;
    case L_SOS: {                                   // ... --- ...
      static const uint8_t sos[] = {1,1,1,1,1,3, 3,1,3,1,3,3, 1,1,1,1,1,7};   // вкл/выкл поочерёдно, в "точках"
      if ((int32_t)(now - sosUntil) >= 0) {
        sosIdx = (sosIdx + 1) % sizeof(sos);
        sosUntil = now + (uint32_t)sos[sosIdx] * (300 - cfg.laserRate * 22);
      }
      duty = (sosIdx % 2 == 0) ? 1 : 0;
      break;
    }
    case L_STILLBLINK:                              // в движении горит, на паузах мигает — привлекает внимание
      laserPh += 5.0f * dt;
      laserPh -= floorf(laserPh);
      duty = moving ? 1 : ((laserPh < 0.5f) ? 1 : 0);
      break;
    case L_MOVEBLINK:                               // стоит — горит, бежит — мерцает
      laserPh += 8.0f * dt;
      laserPh -= floorf(laserPh);
      duty = moving ? ((laserPh < 0.5f) ? 1 : 0) : 1;
      break;
    default: break;
  }

  // яркость + плавное гашение в финале игры
  float v = duty * cfg.laserBright / 100.0f * laserFinaleMul() * 255.0f + 0.5f;
  laserSet((uint8_t)constrain(v, 0.0f, 255.0f));
}

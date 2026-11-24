#pragma once
#include "common.h"
#include "platform.h"      // подключает Servo (ESP8266) или ESP32Servo (ESP32)

// Две оси: 0 = горизонталь (pan), 1 = вертикаль (tilt)
struct Axis {
  Servo    servo;
  uint8_t  pin;
  float    cur, tgt, spd;   // положение, цель (градусы), скорость (град/с)
  float    vel;             // текущая скорость (для плавного разгона)
  int      lastUs;
  uint32_t lastMoveMs;
  bool     parking;         // идёт "парковка" (в середину / тест калибровки)
  bool     parkHold;        // доехали, держим
  uint32_t parkDoneAt;
};

extern Axis     ax[2];
extern uint32_t lastMotionMs;   // когда в последний раз что-то двигалось (для режимов лазера)
extern bool     parkPending;    // после парковки перезапустить текущий режим

void servoInit();
void servoDetachAll();
void servoRecalibrate();          // вызвать после изменения калибровки

bool stepAxes(float dt);          // true, когда обе оси доехали
void moveTo(float x, float y, float speed);   // прямая: обе оси приезжают одновременно
void chase(float x, float y, float speed);    // догонять постоянно меняющуюся цель
bool writeAxis(int i);
void applyAxis(int i);

bool parkBusy();
void startPark(int i, float target);   // target < 0 = середина зоны (или 90°)
void updatePark();                     // вызывать из loop()
void servoTest(int i, int pos);        // 0 = минимум (0°), 1 = центр (90°), 2 = максимум (180°)

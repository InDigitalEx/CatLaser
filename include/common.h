#pragma once
#include <Arduino.h>

// ===================== Режимы движения =====================
enum Mode : uint8_t {
  M_OFF = 0,
  M_SWEEP,      // 1  маятник
  M_RASTER,     // 2  растр (строчками)           [нужны 2 оси]
  M_RANDOM,     // 3  хаос
  M_HUNT,       // 4  охота
  M_BUTTERFLY,  // 5  бабочка
  M_EIGHT,      // 6  восьмёрка                   [нужны 2 оси]
  M_SPIRAL,     // 7  спираль                     [нужны 2 оси]
  M_HOPS,       // 8  прыжки
  M_HOLE,       // 9  норка
  M_BOUNCE,     // 10 рикошет
  M_NERVOUS,    // 11 нервный
  M_WAVE,       // 12 волна
  M_ACCEL,      // 13 разгон
  M_DAMPED,     // 14 затухание
  M_AUTO,       // 15 авто-плейлист
  M_MANUAL,     // 16 ручной
  M_FLY,        // 17 муха
  M_LIGHTNING,  // 18 молния
  M_BEAT,       // 19 ритм
  M_STAR,       // 20 звезда          [нужны 2 оси]
  M_PERIM,      // 21 периметр        [нужны 2 оси]
  M_SNAKE,      // 22 змейка          [нужны 2 оси]
  M_VORTEX,     // 23 вихрь           [нужны 2 оси]
  M_LAST = M_VORTEX
};
const uint8_t MODE_COUNT = M_LAST + 1;   // 24

// ===================== Режимы лазера =====================
enum LaserMode : uint8_t {
  L_OFF = 0,
  L_ON,         // 1 горит
  L_BLINK,      // 2 мигает
  L_PULSE,      // 3 плавный пульс
  L_STROBE,     // 4 строб
  L_FLICKER,    // 5 случайное мерцание
  L_STILL,      // 6 горит только когда стоит
  L_MOVING,     // 7 горит только в движении
  L_HEART,      // 8 "сердцебиение" (двойная вспышка)
  L_SOS,        // 9 SOS азбукой Морзе
  L_STILLBLINK, // 10 мигает, когда стоит
  L_MOVEBLINK   // 11 мигает в движении
};

// случайное число, включительно a..b
inline long rnd(long a, long b) {
  if (b < a) b = a;
  return random(a, b + 1);
}

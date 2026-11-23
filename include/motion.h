#pragma once
#include "common.h"

extern Mode  mode;          // выбранный режим
extern Mode  run;           // реально выполняемый (в "Авто" — текущий под-режим)
extern float manualX, manualY;

bool modeNeeds2(Mode m);    // режим имеет смысл только на двух осях
bool modeAllowed(Mode m);   // режим можно запустить при текущем наборе серво

void setMode(Mode m);
void motionLoop();                  // вызывать из loop()
void axisToggled(int i);            // серво включили/выключили
void motionRestartAfterPark();      // вызывается после парковки серво
void autoRearm();                   // пересчитать интервал смены режимов в "Авто"

// --- таймер сессии (автостоп) и его эффекты ---
void     sessionBegin(uint32_t durMs);   // начать сессию длиной durMs (0 = без ограничения)
void     sessionClear();
uint32_t sessionRemainingMs();
bool     motionFinale();                 // сейчас идёт финал игры
float    laserFinaleMul();               // множитель яркости лазера (1 = обычная, <1 = гаснет в финале)

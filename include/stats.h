#pragma once
#include "common.h"

void   statsInit();            // загрузить статистику из файловой системы
void   statsLoop();            // вызывать из loop(): учёт времени, сохранение, запуск NTP
void   statsOnStart();         // игра началась (из выключенного состояния)
void   statsOnRun(Mode m);     // запущен очередной режим
void   statsOnStop();          // игра остановлена — сохраним статистику
void   statsReset();
void   statsTzChanged();       // часовой пояс изменили — перенастроить NTP
bool   statsTimeSynced();      // время получено по NTP
String statsClock();           // "ЧЧ:ММ" или пустая строка, если время не синхронизировано
String statsJson();            // для веб-интерфейса

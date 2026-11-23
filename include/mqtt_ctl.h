#pragma once
#include "common.h"

// MQTT (обычный TCP) для умного дома.
//   <топик>        — команды питания: ON/1 включить, OFF/0 выключить (также TOGGLE)
//   <топик>/state  — состояние игрушки (ON/OFF, retained)
//   <топик>/lwt    — доступность (online/offline, retained)
//   <топик>/auto   — дежурный режим (команды), <топик>/auto/state — его состояние
void mqSetup();
void mqLoop();            // вызывать из loop()
void mqApply();           // применить новые настройки подключения
void mqGoOffline();       // сообщить "offline" в старый топик (до смены настроек)
bool mqIsConnected();
int  mqRc();              // код последнего состояния клиента

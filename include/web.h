#pragma once
#include "common.h"

// Веб-сервер: API (/api/...) и отдача интерфейса из файловой системы LittleFS (папка data/).
void webInit();
void webLoop();     // вызывать из loop()

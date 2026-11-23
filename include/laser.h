#pragma once
#include "common.h"

void laserInit();
void laserLoop();     // вызывать из loop()
void laserOff();      // немедленно погасить (например, перед OTA)

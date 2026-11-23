#pragma once
#include <Arduino.h>

// ===================== Железо =====================
#define PAN_PIN            D4       // горизонтальная серво (GPIO2)
#define TILT_PIN           D5       // вертикальная серво
static const int8_t LASER_PIN = D1; // -1 если лазер не управляется с ESP

#define CAL_MIN_US_DEF     500      // импульс для 0° по умолчанию
#define CAL_MAX_US_DEF     2400     // импульс для 180° по умолчанию

// ===================== Тайминги и поведение =====================
#define SERVO_DEADBAND_US  5        // не двигать серво, если сдвиг меньше ~0.5°
#define IDLE_DETACH_MS     400      // через сколько мс неподвижности отключать сигнал на серво
#define MOTION_STEP_MS     10       // шаг пересчёта движения (серво обновляются 50 раз/с)
#define PARK_AT_ZONE_MID   1        // включение/выключение серво: 1 = встать в середину зоны игры, 0 = ровно на 90°
#define PARK_SPEED         100      // скорость парковки, град/с
#define PARK_HOLD_MS       700      // сколько держать серво после парковки, мс
#define AUTOSAVE_MS        4000     // задержка автосохранения настроек после последнего изменения
#define STATS_SAVE_MS      300000UL // как часто сохранять статистику во время игры (5 минут)

#define HOSTNAME           "catlaser"
#define AP_NAME            "CatLaser-Setup"

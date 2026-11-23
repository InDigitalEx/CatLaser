#include "motion.h"
#include "config.h"
#include "hw.h"
#include "servo_ctl.h"
#include "stats.h"
#include <math.h>

Mode  mode = M_OFF;
Mode  run  = M_OFF;
float manualX = 90, manualY = 90;

// ---------- состояние режимов ----------
static uint8_t  phase = 0;
static uint32_t waitUntil = 0;
static uint32_t autoNext = 0;
static float    bt = 0;

static uint8_t  row = 0;
static int8_t   rdir = 1, panDir = 1;                  // растр
static float    holeX = 30, holeY = 50;                // норка
static float    bx = 90, by = 90, bvx = 1, bvy = 0;    // рикошет / нервный / муха
static float    nSpeed = 100;
static uint32_t nNext = 0;
static uint8_t  pass = 0;                              // разгон
static float    flyTx = 90, flyTy = 90, flyOx = 0, flyOy = 0;   // муха
static uint8_t  legs = 0;                              // молния
static uint8_t  beatIdx = 0;                           // ритм
static uint8_t  starIdx = 0;                           // звезда
static float    perimS = 0;  static int8_t perimDir = 1;        // периметр
static int8_t   snakeDir = 1;                          // змейка
static float    vortT = 0;   static int8_t vortDir = 1;         // вихрь

static const uint8_t RASTER_ROWS = 5;

// ---------- таймер сессии, разгон/затухание и финал ----------
static uint32_t sessStart = 0, sessEnd = 0;
static float    g_speedMul = 1;      // множитель скорости (разгон/затухание/финал)
static float    g_laserMul = 1;      // множитель яркости лазера (гашение в финале)
static bool     g_frozen = false;    // финал: лазер замер на месте
static bool     g_finale = false;

void sessionBegin(uint32_t durMs) {
  if (!durMs) { sessEnd = 0; return; }
  sessStart = millis();
  sessEnd = sessStart + durMs;
}
void sessionClear() { sessEnd = 0; }
uint32_t sessionRemainingMs() {
  if (!sessEnd) return 0;
  int32_t r = (int32_t)(sessEnd - millis());
  return r > 0 ? (uint32_t)r : 0;
}
bool  motionFinale()    { return g_finale; }
float laserFinaleMul()  { return (mode == M_OFF) ? 1.0f : g_laserMul; }

// Опциональные эффекты сессии. Работают только если у сессии есть конечное время (автостоп)
// и режим не ручной.
static void sessionFx(uint32_t now) {
  g_speedMul = 1; g_laserMul = 1; g_frozen = false; g_finale = false;
  if (!sessEnd || mode == M_MANUAL) return;

  uint32_t total = sessEnd - sessStart;
  if (total < 1000) return;
  int32_t remS = (int32_t)(sessEnd - now);
  uint32_t rem = remS > 0 ? (uint32_t)remS : 0;
  float p = 1.0f - (float)rem / (float)total;
  if (p < 0) p = 0;
  if (p > 1) p = 1;

  // --- разгон в начале и затухание к концу ---
  if (feat.rampEn && total >= 20000UL) {
    float r = feat.rampPct / 100.0f;
    float m = 1.0f;
    if (p < r)              m = 0.4f + 0.6f * (p / r);
    else if (p > 1.0f - r)  m = 0.4f + 0.6f * ((1.0f - p) / r);
    g_speedMul = m;
  }

  // --- финал: замедляемся, замираем и плавно гасим лазер ---
  if (feat.finaleEn) {
    uint32_t fin = (uint32_t)feat.finaleSec * 1000UL;
    if (fin > total / 2) fin = total / 2;
    if (fin >= 1000 && rem <= fin) {
      float t = 1.0f - (float)rem / (float)fin;                     // 0..1 по ходу финала
      float fm = 1.0f - 0.8f * (t < 0.6f ? t / 0.6f : 1.0f);        // скорость падает до 20%
      if (fm < g_speedMul) g_speedMul = fm;
      g_finale = true;
      if (t >= 0.6f) {                                              // последние 40% — стоим и гасим лазер
        g_frozen = true;
        g_laserMul = 1.0f - (t - 0.6f) / 0.4f;
        if (g_laserMul < 0) g_laserMul = 0;
      }
    }
  }
}

// ---------- проверка режимов на количество осей ----------
bool modeNeeds2(Mode m) {
  switch (m) {
    case M_RASTER: case M_EIGHT: case M_SPIRAL:
    case M_STAR: case M_PERIM: case M_SNAKE: case M_VORTEX:
      return true;
    default:
      return false;
  }
}
bool modeAllowed(Mode m) { return !modeNeeds2(m) || twoAx(); }

// ---------- хелперы ----------
static float baseSpeed() { return cfg.speed * 30.0f * g_speedMul; }   // 30..300 град/с с учётом разгона/затухания

static long randPause() { return rnd(cfg.pauseMin, cfg.pauseMax); }

static void randPoint(float &x, float &y) {
  x = rnd(cfg.xMin, cfg.xMax);
  y = rnd(cfg.yMin, cfg.yMax);
}

// размер зоны с учётом только включённых осей
static float zoneDiag() {
  float dx = enAx(0) ? cfg.xMax - cfg.xMin : 0;
  float dy = enAx(1) ? cfg.yMax - cfg.yMin : 0;
  float d = sqrtf(dx * dx + dy * dy);
  return d < 1 ? 1 : d;
}

static float distTo(float x, float y) {
  float dx = enAx(0) ? x - ax[0].cur : 0;
  float dy = enAx(1) ? y - ax[1].cur : 0;
  return sqrtf(dx * dx + dy * dy);
}

// случайное направление с учётом включённых осей (на одной оси — просто туда или обратно)
static void randDir(float &ux, float &uy) {
  bool a = enAx(0), b = enAx(1);
  if (a && b) {
    float t = rnd(0, 359) * 0.01745f;
    ux = cosf(t); uy = sinf(t);
  } else if (a) {
    ux = random(2) ? 1 : -1; uy = 0;
  } else {
    ux = 0; uy = random(2) ? 1 : -1;
  }
}

static void normDir(float &ux, float &uy) {
  if (!enAx(0)) ux = 0;
  if (!enAx(1)) uy = 0;
  float l = sqrtf(ux * ux + uy * uy);
  if (l < 0.01f) {
    if (enAx(0)) { ux = 1; uy = 0; } else { ux = 0; uy = 1; }
  } else {
    ux /= l; uy /= l;
  }
}

// точка для "маятника": главная ось (pan, а если он выключен — tilt) идёт в край, вторая на середине
static void sweepPoint(bool toHi, float &x, float &y) {
  x = xMid(); y = yMid();
  if (enAx(0)) x = toHi ? xHi() : xLo();
  else         y = toHi ? yHi() : yLo();
}

static bool atPrimaryHi() {
  return enAx(0) ? fabsf(ax[0].cur - xHi()) < 1.0f : fabsf(ax[1].cur - yHi()) < 1.0f;
}

static float rowY(uint8_t r) { return yLo() + (yHi() - yLo()) * r / (RASTER_ROWS - 1); }

static void startCreep() {
  float dx = rnd(-20, 20), dy = rnd(-20, 20);     // сначала считаем в переменные: constrain() — макрос и вызывает аргумент несколько раз
  float x = constrain(ax[0].cur + dx, xLo(), xHi());
  float y = constrain(ax[1].cur + dy, yLo(), yHi());
  moveTo(x, y, max(8.0f, baseSpeed() * 0.2f));
  phase = 0;
}

static void startDash() {
  float x, y;
  randPoint(x, y);
  for (int i = 0; i < 5 && distTo(x, y) < zoneDiag() / 3.0f; i++) randPoint(x, y);
  moveTo(x, y, baseSpeed() * 3.0f);
  phase = 2;
}

// случайная точка, достаточно далёкая от текущего положения
static void randFarPoint(float &x, float &y, float minFrac) {
  randPoint(x, y);
  for (int i = 0; i < 6 && distTo(x, y) < zoneDiag() * minFrac; i++) randPoint(x, y);
}

static void lightningLeg() {                    // один быстрый зигзаг "молнии"
  float x, y;
  randFarPoint(x, y, 0.3f);
  moveTo(x, y, baseSpeed() * 4.0f);
}

static void beatAnchor(uint8_t i, float &x, float &y) {   // опорные точки "ритма": 4 угла и центр
  switch (i) {
    case 0:  x = xLo();  y = yLo();  break;
    case 1:  x = xHi();  y = yLo();  break;
    case 2:  x = xHi();  y = yHi();  break;
    case 3:  x = xLo();  y = yHi();  break;
    default: x = xMid(); y = yMid(); break;
  }
}

static float perimLen() { return 2.0f * ((xHi() - xLo()) + (yHi() - yLo())); }

static void perimPoint(float s, float &x, float &y) {     // точка на границе зоны по длине пути s
  float w = xHi() - xLo(), h = yHi() - yLo();
  if (s < w)              { x = xLo() + s;           y = yLo(); }
  else if (s < w + h)     { x = xHi();               y = yLo() + (s - w); }
  else if (s < 2 * w + h) { x = xHi() - (s - w - h); y = yHi(); }
  else                    { x = xLo();               y = yHi() - (s - 2 * w - h); }
}

static void vortexPoint(float a, float t, float &x, float &y) {   // маленький кружок, который блуждает по зоне
  float cx = xMid() + xAmp() * 0.6f * sinf(t);
  float cy = yMid() + yAmp() * 0.6f * sinf(t * 1.3f + 1.0f);
  x = cx + xAmp() * 0.35f * cosf(a);
  y = cy + yAmp() * 0.35f * sinf(a);
}

static void pickHole() {
  holeX = random(2) ? xHi() : xLo();
  holeY = random(2) ? yHi() : yLo();
}

// движение точки с отскоком от границ зоны (рикошет и нервный)
static void advancePoint(float dt, float v, bool jitter) {
  if (enAx(0)) bx += bvx * v * dt;
  if (enAx(1)) by += bvy * v * dt;
  int8_t sx = 0, sy = 0;
  if (enAx(0)) { if (bx > xHi()) { bx = xHi(); sx = -1; } else if (bx < xLo()) { bx = xLo(); sx = 1; } }
  if (enAx(1)) { if (by > yHi()) { by = yHi(); sy = -1; } else if (by < yLo()) { by = yLo(); sy = 1; } }
  if (sx || sy) {
    if (sx) bvx = sx * fabsf(bvx);
    if (sy) bvy = sy * fabsf(bvy);
    if (jitter) {                                   // чуть случайности при отскоке
      float a = atan2f(bvy, bvx) + rnd(-20, 20) * 0.01745f;
      bvx = cosf(a); bvy = sinf(a);
      normDir(bvx, bvy);
      if (sx && bvx * sx < 0) bvx = -bvx;
      if (sy && bvy * sy < 0) bvy = -bvy;
    }
  }
}

// узоры "бабочки": сумма трёх синусов
static float butterflyX(float t) { return 0.55f * sinf(t) + 0.30f * sinf(t * 2.3f + 1.3f) + 0.15f * sinf(t * 3.7f + 4.1f); }
static float butterflyY(float t) { return 0.55f * sinf(t * 1.13f + 2.0f) + 0.30f * sinf(t * 1.9f + 0.4f) + 0.15f * sinf(t * 3.1f + 5.2f); }

// ---------- старт режимов ----------
static void startRun(Mode m) {
  run = m;
  phase = 0;
  waitUntil = 0;
  bt = 0;
  statsOnRun(m);
  float sp = baseSpeed();
  float cx = constrain(ax[0].cur, xLo(), xHi());
  float cy = constrain(ax[1].cur, yLo(), yHi());
  float x, y;

  switch (m) {
    case M_SWEEP:
      sweepPoint(true, x, y);
      moveTo(x, y, sp);
      break;
    case M_RASTER:
      row = 0; rdir = 1; panDir = 1;
      moveTo(xHi(), rowY(0), sp);
      break;
    case M_RANDOM:
      randPoint(x, y);
      moveTo(x, y, sp);
      break;
    case M_HUNT:
      startCreep();
      break;
    case M_HOPS:
      moveTo(cx, cy, sp);
      break;
    case M_HOLE:
      pickHole();
      moveTo(holeX, holeY, sp * 3.0f);
      break;
    case M_BOUNCE:
      bx = cx; by = cy;
      randDir(bvx, bvy);
      break;
    case M_NERVOUS:
      bx = cx; by = cy;
      randDir(bvx, bvy);
      nSpeed = cfg.speed * 30.0f;
      nNext = millis();
      break;
    case M_ACCEL:
      pass = 0;
      sweepPoint(true, x, y);
      moveTo(x, y, sp * 0.3f);
      break;
    case M_DAMPED:               // сначала отводим в крайнюю точку
      sweepPoint(true, x, y);
      moveTo(x, y, sp);
      phase = 2;
      break;
    case M_MANUAL:
      chase(manualX, manualY, 400);
      break;
    case M_FLY:
      bx = cx; by = cy;
      randPoint(flyTx, flyTy);
      flyOx = flyOy = 0;
      nNext = millis();
      moveTo(cx, cy, sp);
      break;
    case M_LIGHTNING:
      legs = rnd(3, 6) - 1;
      lightningLeg();
      break;
    case M_BEAT:
      beatIdx = 0;
      waitUntil = millis();
      break;
    case M_STAR:
      starIdx = 0;
      phase = 1;
      waitUntil = millis();
      break;
    case M_PERIM:
      perimS = 0; perimDir = 1;
      nNext = millis() + 2000;
      moveTo(xLo(), yLo(), sp);
      phase = 2;                 // сначала едем в начальный угол
      break;
    case M_SNAKE:
      snakeDir = 1;
      moveTo(xLo(), yMid(), sp);
      phase = 2;
      break;
    case M_VORTEX:
      vortT = 0; vortDir = 1;
      nNext = millis() + 4000;
      vortexPoint(0, 0, x, y);
      moveTo(x, y, sp);
      phase = 2;
      break;
    default:                     // butterfly / eight / spiral / wave — считаются на лету
      break;
  }
}

// ---------- "Авто": плейлист с весами ----------
static const Mode AUTO_POOL[] = {
  M_SWEEP, M_RASTER, M_RANDOM, M_HUNT, M_BUTTERFLY, M_EIGHT, M_SPIRAL, M_HOPS, M_HOLE, M_BOUNCE,
  M_NERVOUS, M_WAVE, M_ACCEL, M_DAMPED, M_FLY, M_LIGHTNING, M_BEAT, M_STAR, M_PERIM, M_SNAKE, M_VORTEX
};

static void pickAuto() {
  const uint8_t total = sizeof(AUTO_POOL) / sizeof(AUTO_POOL[0]);
  Mode cand[total];
  uint16_t wt[total];
  uint8_t n = 0;
  uint32_t sum = 0;

  for (uint8_t attempt = 0; attempt < 2 && !n; attempt++) {     // с первого раза исключаем текущий режим, если вариантов нет — берём и его
    for (uint8_t i = 0; i < total; i++) {
      Mode m = AUTO_POOL[i];
      if (!modeAllowed(m)) continue;                            // режимы для 2 осей при одной серво не берём
      if (attempt == 0 && m == run && !feat.playlistEn) continue;   // без своего плейлиста два раза подряд один режим не берём
      uint16_t w = feat.playlistEn ? feat.weights[m] : 1;       // свой плейлист: вес 0 = режим не играет
      if (!w) continue;
      cand[n] = m; wt[n] = w; sum += w; n++;
    }
  }
  if (!n) { startRun(M_SWEEP); return; }

  uint32_t r = rnd(1, sum);                                     // выбор с учётом весов
  uint8_t k = 0;
  while (k < n - 1 && r > wt[k]) { r -= wt[k]; k++; }
  startRun(cand[k]);
}

void autoRearm() { autoNext = millis() + (uint32_t)cfg.autoSec * 1000UL; }

// ---------- смена режима ----------
void setMode(Mode m) {
  if (!modeAllowed(m)) m = M_AUTO;   // страховка: режим для 2 осей на одной серво не запускаем
  Mode prev = mode;
  mode = m;
  uint32_t now = millis();
  if (m != M_OFF && mqc.lastMode != m) { mqc.lastMode = m; cfgMarkDirty(); }   // помним последний режим для MQTT "ON"

  if (m != M_OFF && cfg.timerMin > 0) sessionBegin((uint32_t)cfg.timerMin * 60000UL);
  else sessionClear();

  if (m == M_OFF) {
    if (prev != M_OFF) statsOnStop();
    run = M_OFF;
    g_speedMul = 1; g_laserMul = 1; g_frozen = false; g_finale = false;
    servoDetachAll();
    return;
  }
  if (prev == M_OFF) statsOnStart();

  for (int i = 0; i < 2; i++) {      // форсируем первую отправку положения
    ax[i].lastUs = 0;
    ax[i].lastMoveMs = now;
    ax[i].vel = 0;
  }
  sessionFx(now);
  if (m == M_AUTO) {
    run = M_OFF;
    pickAuto();
    autoRearm();
  } else {
    startRun(m);
  }
}

// перезапуск режима после включения/выключения серво (когда они встали в середину)
void motionRestartAfterPark() {
  if (mode == M_OFF) return;
  if (modeNeeds2(mode) && !twoAx()) setMode(M_SWEEP);   // без второй оси этот режим невозможен
  else if (mode == M_AUTO) pickAuto();                  // выбираем под-режим заново под новый набор осей
  else startRun(mode);
}

// включили/выключили серво на лету: плавно встаёт в середину зоны, потом режим продолжается
void axisToggled(int i) {
  startPark(i, -1);
  parkPending = (mode != M_OFF);
}

// ===================== Основной цикл движения =====================
void motionLoop() {
  if (parkBusy()) return;                           // серво встают в середину — режим подождёт
  static uint32_t last = millis();
  uint32_t now = millis();
  if (now - last < MOTION_STEP_MS) return;          // не гоняем синусы тысячи раз в секунду
  float dt = (now - last) / 1000.0f;
  last = now;
  if (dt > 0.1f) dt = 0.1f;

  if (mode == M_OFF) return;

  if (sessEnd && (int32_t)(now - sessEnd) >= 0) {
    setMode(M_OFF);
    return;
  }
  sessionFx(now);

  if (g_frozen) {                                   // финал: лазер замер
    for (int i = 0; i < 2; i++) { ax[i].tgt = ax[i].cur; ax[i].vel = 0; }
    applyAxis(0);
    applyAxis(1);
    return;
  }

  if (mode == M_AUTO && (int32_t)(now - autoNext) >= 0) {
    pickAuto();
    autoRearm();
  }

  float before[2] = { ax[0].cur, ax[1].cur };
  bool arrived = stepAxes(dt);
  if (mode != M_MANUAL) {                           // не даём вылететь за зону игры (если до этого были внутри неё)
    const float lo[2] = { xLo(), yLo() }, hi[2] = { xHi(), yHi() };
    for (int i = 0; i < 2; i++) {
      bool wasIn = before[i] >= lo[i] - 0.01f && before[i] <= hi[i] + 0.01f;
      if (enAx(i) && wasIn && (ax[i].cur < lo[i] || ax[i].cur > hi[i])) {
        ax[i].cur = constrain(ax[i].cur, lo[i], hi[i]);
        ax[i].vel = 0;
      }
    }
  }
  float sp = baseSpeed();
  float dtv = dt * g_speedMul;                      // время с учётом разгона/затухания для параметрических узоров
  float x, y;

  switch (run) {

    case M_SWEEP:
      if (phase == 0) {
        if (arrived) { phase = 1; waitUntil = now + cfg.pauseMin; }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        sweepPoint(!atPrimaryHi(), x, y);
        moveTo(x, y, sp);
        phase = 0;
      }
      break;

    case M_RASTER:
      if (phase == 0) {            // идём по строке
        if (arrived) {
          phase = 1;
          row += rdir;
          if (row >= RASTER_ROWS - 1) { row = RASTER_ROWS - 1; rdir = -1; }
          else if (row <= 0)          { row = 0; rdir = 1; }
          moveTo(ax[0].cur, rowY(row), sp);
        }
      } else if (arrived) {        // перешли на следующую строку
        phase = 0;
        panDir = -panDir;
        moveTo(panDir > 0 ? xHi() : xLo(), rowY(row), sp);
      }
      break;

    case M_RANDOM:
      if (phase == 0) {
        if (arrived) { phase = 1; waitUntil = now + randPause(); }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        randPoint(x, y);
        moveTo(x, y, sp * rnd(60, 160) / 100.0f);
        phase = 0;
      }
      break;

    case M_HUNT:
      switch (phase) {
        case 0:  // крадётся
          if (arrived) { phase = 1; waitUntil = now + rnd(400, 1500); }
          break;
        case 1:  // замер
          if ((int32_t)(now - waitUntil) >= 0) {
            if (random(100) < 55) startCreep(); else startDash();
          }
          break;
        case 2:  // рывок
          if (arrived) { phase = 3; waitUntil = now + randPause(); }
          break;
        case 3:  // отдых
          if ((int32_t)(now - waitUntil) >= 0) startCreep();
          break;
      }
      break;

    case M_BUTTERFLY: {
      bt += dtv * (0.3f + cfg.speed * 0.12f);
      float vx = 0, vy = 0;
      if (enAx(0)) vx = butterflyX(bt);
      if (enAx(1)) vy = enAx(0) ? butterflyY(bt) : butterflyX(bt);   // одна вертикальная серво — основной узор
      chase(xMid() + xAmp() * vx, yMid() + yAmp() * vy, 600);
      break;
    }

    case M_EIGHT: {
      bt += dtv * (0.4f + cfg.speed * 0.15f);
      float ex = enAx(0) ? xMid() + xAmp() * sinf(bt) : 0;
      float ey = enAx(1) ? yMid() + yAmp() * sinf(2.0f * bt) : 0;
      chase(ex, ey, 600);
      break;
    }

    case M_SPIRAL: {
      bt += dtv * (0.5f + cfg.speed * 0.18f);
      float r = 0.2f + 0.8f * (0.5f + 0.5f * sinf(bt * 0.1f));
      float sx = enAx(0) ? xMid() + xAmp() * r * cosf(bt * 1.5f) : 0;
      float sy = enAx(1) ? yMid() + yAmp() * r * sinf(bt * 1.5f) : 0;
      chase(sx, sy, 600);
      break;
    }

    case M_HOPS:
      if (phase == 0) {
        if (arrived) { phase = 1; waitUntil = now + randPause(); }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        float ux, uy;
        randDir(ux, uy);
        float len = rnd(8, 25);
        moveTo(constrain(ax[0].cur + ux * len, xLo(), xHi()),
               constrain(ax[1].cur + uy * len, yLo(), yHi()), sp * 2.0f);
        phase = 0;
      }
      break;

    case M_HOLE:
      switch (phase) {
        case 0:  // прячется в норке
          if (arrived) { phase = 1; waitUntil = now + randPause() + 500; }
          break;
        case 1:  // осторожно выглядывает
          if ((int32_t)(now - waitUntil) >= 0) {
            float f = rnd(10, 35) / 100.0f;
            moveTo(holeX + (xMid() - holeX) * f, holeY + (yMid() - holeY) * f, sp * 0.4f);
            phase = 2;
          }
          break;
        case 2:
          if (arrived) { phase = 3; waitUntil = now + rnd(300, 900); }
          break;
        case 3:  // решает: спрятаться или выбежать
          if ((int32_t)(now - waitUntil) >= 0) {
            if (random(100) < 55) {
              moveTo(holeX, holeY, sp * 3.0f);
              phase = 0;
            } else {
              randPoint(x, y);
              moveTo(x, y, sp * 3.0f);
              phase = 4;
            }
          }
          break;
        case 4:
          if (arrived) { phase = 5; waitUntil = now + rnd(200, 700); }
          break;
        case 5:  // убегает в новую норку
          if ((int32_t)(now - waitUntil) >= 0) {
            pickHole();
            moveTo(holeX, holeY, sp * 3.0f);
            phase = 0;
          }
          break;
      }
      break;

    case M_BOUNCE:
      advancePoint(dt, sp * 0.8f, true);
      chase(bx, by, 600);
      break;

    case M_NERVOUS:
      advancePoint(dt, nSpeed * g_speedMul, false);
      if ((int32_t)(now - nNext) >= 0) {
        nNext = now + rnd(300, 1500);
        nSpeed = cfg.speed * 30.0f * rnd(40, 180) / 100.0f;
        if (random(100) < 40) randDir(bvx, bvy);    // внезапный разворот / смена курса
        if (random(100) < 15) nSpeed = 0;           // замирание
      }
      chase(bx, by, 600);
      break;

    case M_WAVE: {
      bt += dtv * (0.5f + cfg.speed * 0.2f);
      float r = 0.2f + 0.8f * (0.5f + 0.5f * sinf(bt * 0.12f));   // размах плавает 20..100%
      float w = sinf(bt * 1.5f);
      float wx = enAx(0) ? xMid() + xAmp() * r * w : 0;
      float wy = 0;
      if (enAx(1)) wy = enAx(0) ? yMid() + yAmp() * 0.6f * sinf(bt * 0.5f + 1.0f)   // вторая ось медленно "дышит"
                                : yMid() + yAmp() * r * w;
      chase(wx, wy, 600);
      break;
    }

    case M_ACCEL:      // каждый проход быстрее предыдущего, потом всё сначала
      if (phase == 0) {
        if (arrived) { phase = 1; waitUntil = now + cfg.pauseMin; pass = (pass + 1) % 9; }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        sweepPoint(!atPrimaryHi(), x, y);
        if (twoAx()) y = rnd(cfg.yMin, cfg.yMax);     // с двумя серво каждый проход на новой "строке"
        moveTo(x, y, sp * (0.3f + pass * 0.15f));
        phase = 0;
      }
      break;

    case M_DAMPED:     // раскачанный маятник, который постепенно затухает
      if (phase == 2) {
        if (arrived) { phase = 0; bt = 0; }
      } else if (phase == 0) {
        bt += dtv * (0.8f + cfg.speed * 0.25f);
        float A = expf(-bt / 5.0f);
        float c = cosf(bt * 2.0f);
        float dx = enAx(0) ? xMid() + xAmp() * A * c : 0;
        float dy = 0;
        if (enAx(1)) dy = enAx(0) ? yMid() + yAmp() * A * sinf(bt * 2.6f) : yMid() + yAmp() * A * c;
        chase(dx, dy, 600);
        if (A < 0.06f) { phase = 1; waitUntil = now + randPause(); }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        sweepPoint(true, x, y);
        moveTo(x, y, sp);
        phase = 2;
      }
      break;

    case M_FLY: {      // мельтешит у одной точки, сама точка медленно дрейфует
      float dx = enAx(0) ? flyTx - bx : 0, dy = enAx(1) ? flyTy - by : 0;
      float d = sqrtf(dx * dx + dy * dy), step = sp * 0.25f * dt;
      if (d <= step) randPoint(flyTx, flyTy);
      else { bx += dx / d * step; by += dy / d * step; }
      if ((int32_t)(now - nNext) >= 0) {
        nNext = now + rnd(40, 140);
        long amp = (random(100) < 10) ? 12 : 4;         // иногда резкий "перелёт" побольше
        flyOx = rnd(-amp, amp);
        flyOy = rnd(-amp, amp);
      }
      chase(constrain(bx + flyOx, xLo(), xHi()), constrain(by + flyOy, yLo(), yHi()), 500);
      break;
    }

    case M_LIGHTNING:  // серия быстрых зигзагов, потом пауза
      if (phase == 0) {
        if (arrived) {
          if (legs > 0) { legs--; lightningLeg(); }
          else { phase = 1; waitUntil = now + randPause() + 300; }
        }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        legs = rnd(3, 6) - 1;
        lightningLeg();
        phase = 0;
      }
      break;

    case M_BEAT:       // прыжки по углам в такт; скорость задаёт темп
      if ((int32_t)(now - waitUntil) >= 0) {
        waitUntil = now + (1000 - cfg.speed * 85);      // 915..150 мс на удар
        if (random(100) >= 15) {                        // иногда пропускаем долю
          beatIdx = (beatIdx + (random(100) < 25 ? 2 : 1)) % 5;
          beatAnchor(beatIdx, x, y);
          moveTo(x, y, 600);
        }
      }
      break;

    case M_STAR:       // рисует пентаграмму и поворачивает её при каждом новом круге
      if (phase == 0) {
        if (arrived) { phase = 1; waitUntil = now + (starIdx > 5 ? randPause() + 300 : 80); }
      } else if ((int32_t)(now - waitUntil) >= 0) {
        if (starIdx > 5) { starIdx = 0; bt += rnd(0, 71) * 0.01745f; }
        float a = bt - 1.5708f + ((starIdx * 2) % 5) * 1.2566f;
        moveTo(xMid() + xAmp() * cosf(a), yMid() + yAmp() * sinf(a), sp * 1.5f);
        starIdx++;
        phase = 0;
      }
      break;

    case M_PERIM:      // бегает по границе зоны, иногда замирает и разворачивается
      if (phase == 2) {
        if (arrived) phase = 0;
      } else {
        if (phase == 0) {
          float P = perimLen();
          perimS += perimDir * sp * 0.9f * dt;
          if (perimS >= P) perimS -= P;
          if (perimS < 0) perimS += P;
          if ((int32_t)(now - nNext) >= 0) {
            nNext = now + rnd(1500, 5000);
            int r = random(100);
            if (r < 35) { phase = 1; waitUntil = now + randPause(); }
            else if (r < 60) perimDir = -perimDir;
          }
        } else if ((int32_t)(now - waitUntil) >= 0) {
          phase = 0;
        }
        perimPoint(perimS, x, y);
        chase(x, y, 600);
      }
      break;

    case M_SNAKE:      // змейка: проход через всю зону с синусоидой поперёк
      if (phase == 2) {
        if (arrived) phase = 0;
      } else {
        bt += snakeDir * dtv * (0.08f + cfg.speed * 0.045f);
        if (bt >= 1) { bt = 1; snakeDir = -1; }
        if (bt <= 0) { bt = 0; snakeDir = 1; }
        chase(xLo() + (xHi() - xLo()) * bt, yMid() + yAmp() * sinf(bt * 12.566f), 600);
      }
      break;

    case M_VORTEX:     // быстрый кружок, который блуждает по зоне и меняет направление
      if (phase == 2) {
        if (arrived) phase = 0;
      } else {
        bt += vortDir * dtv * (1.5f + cfg.speed * 0.5f);
        vortT += dtv * 0.25f;
        if ((int32_t)(now - nNext) >= 0) { nNext = now + rnd(4000, 8000); vortDir = -vortDir; }
        vortexPoint(bt, vortT, x, y);
        chase(x, y, 600);
      }
      break;

    case M_MANUAL:
      chase(manualX, manualY, 400);
      break;

    default: break;
  }

  applyAxis(0);
  applyAxis(1);
}

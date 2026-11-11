/*
  Qatwti K10  v2  —  قطوتي
  UNIHIKER K10 (Arduino IDE) — landscape 320x240

  A  (short)      : +1 hour      (hold = fast)
  B  (short)      : +1 minute    (hold = fast)
  A + B together  : drop a fish for the cat
  Tilt            : the cat slides downhill, leans and looks that way (steep = "wheee!")
  Knock / bump    : the cat jumps, surprised
  Shake           : hearts + love points
  Shake a lot     : the cat gets dizzy
  Screen face down: bed time, the cat sleeps until you pick it up
  Left alone      : the cat strolls around, naps after 2 minutes (moving the board wakes it)
  Night (22-06)   : the cat falls asleep after 15 s (any movement wakes it)
  Hold A + B 2 s  : fix the temperature (then A = -0.5, B = +0.5, it saves itself)
*/
#include "unihiker_k10.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <WiFi.h>
#include <Preferences.h>

// ---------- real time from the internet ----------
// Your WiFi name and password live in wifi_secrets.h (next to this file), so they
// are never shared together with the code. Copy wifi_secrets.example.h to
// wifi_secrets.h and fill it in. Without that file the clock simply starts
// from the computer's time (or use SET_TIME.bat).
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif
#ifndef SECRET_WIFI_NAME
#define SECRET_WIFI_NAME ""
#define SECRET_WIFI_PASSWORD ""
#endif
const char *WIFI_NAME     = SECRET_WIFI_NAME;
const char *WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const float TIMEZONE_HOURS = 3;    // Saudi Arabia = +3

// ---------- options (set to 0 if you don't want it) ----------
#define ENABLE_SOUND   1   // small beeps + meow through the speaker
#define ENABLE_SENSORS 1   // temperature / humidity
#define USE_12H        0   // 1 = 12-hour clock
const int UPLOAD_DELAY_SEC = 10;   // compile -> boot delay compensation (only without WiFi)
const int BUMP_SENSITIVITY = 700;  // lower = the cat notices smaller knocks and shakes
const bool TILT_FLIP      = false; // true if the cat walks uphill when you tilt
const bool TILT_USE_X     = false; // true to use the board's other tilt axis
const float TEMP_OFFSET    = 0;    // starting correction in C (the board warms its own sensor)

UNIHIKER_K10 k10;
#if ENABLE_SOUND
Music music;
#endif
#if ENABLE_SENSORS
AHT20 *aht20 = nullptr;   // created in setup(), after the board is ready
Preferences prefs;
#endif
uint8_t screen_dir = 1;  // landscape
#define CV k10.canvas
struct Btn;  // forward declarations (Arduino auto-prototypes)
struct LineIn;

// ---------- palette ----------
const uint32_t SNOW = 0xFFFFFF, INK_DARK = 0x5B4658, INK_LIGHT = 0xF4E9FF;
const uint32_t PINK = 0xF6AFC4, PINK2 = 0xFFD8E4, HOT = 0xFF8FB1, PEACH = 0xF8C9AA, STRIPE = 0xEDAE8C;
const uint32_t CREAM = 0xFFE7D3, LAVENDER = 0xE8DDF7, LILAC = 0xD8A6F2, MINT = 0xCDEEDF;
const uint32_t SKY = 0xDDF3FF, FISH = 0x8CCBF0, HONEY = 0xFFE08A;

// ---------- layout ----------
const int FLOOR_Y = 224, CAT_BASE_Y = 174, CAT_MIN_X = 34, CAT_MAX_X = 286;
const int FRAME_MS = 40;

// ---------- color helpers ----------
uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
  int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  uint32_t r = ar + (br - ar) * t, g = ag + (bg - ag) * t, bl = ab + (bb - ab) * t;
  return (r << 16) | (g << 8) | bl;
}
uint32_t scaleColor(uint32_t c, float k) { return lerpColor(0x000000, c, k); }
int luma(uint32_t c) { return (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) / 1000; }

// ---------- day / night theme ----------
struct Theme { uint32_t bg, card, shadow, floor, accent, ink, inkBg; float night; };
struct ThemeKey { float h; uint32_t bg, card, shadow, floor, accent; float night; };
const ThemeKey KEYS[] = {
  { 0,  0x2B2747, 0x3B355E, 0x1E1B33, 0x3E5B57, 0xF6AFC4, 1.0 },
  { 5,  0x2B2747, 0x3B355E, 0x1E1B33, 0x3E5B57, 0xF6AFC4, 1.0 },
  { 7,  0xFFE3D6, 0xFFFFFF, 0xF1C7B5, 0xBFE8D2, 0xF59EB5, 0.2 },
  { 9,  0xFFF7FC, 0xFFFFFF, 0xE8DDF7, 0xCDEEDF, 0xF6AFC4, 0.0 },
  { 17, 0xFFF7FC, 0xFFFFFF, 0xE8DDF7, 0xCDEEDF, 0xF6AFC4, 0.0 },
  { 19, 0xE9D9F5, 0xFFFFFF, 0xD4C0EA, 0xB9DCCB, 0xE89AC7, 0.4 },
  { 21, 0x2B2747, 0x3B355E, 0x1E1B33, 0x3E5B57, 0xF6AFC4, 1.0 },
  { 24, 0x2B2747, 0x3B355E, 0x1E1B33, 0x3E5B57, 0xF6AFC4, 1.0 },
};
const int KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);
Theme th;

void updateTheme(float h) {
  int i = 0;
  while (i < KEY_COUNT - 2 && h >= KEYS[i + 1].h) i++;
  const ThemeKey &a = KEYS[i], &b = KEYS[i + 1];
  float t = (h - a.h) / (b.h - a.h);
  th.bg = lerpColor(a.bg, b.bg, t);
  th.card = lerpColor(a.card, b.card, t);
  th.shadow = lerpColor(a.shadow, b.shadow, t);
  th.floor = lerpColor(a.floor, b.floor, t);
  th.accent = lerpColor(a.accent, b.accent, t);
  th.night = a.night + (b.night - a.night) * t;
  th.ink = luma(th.card) > 140 ? INK_DARK : INK_LIGHT;
  th.inkBg = luma(th.bg) > 140 ? INK_DARK : INK_LIGHT;
}

// ---------- state ----------
enum Mood : uint8_t { M_NORMAL, M_HAPPY, M_SLEEP, M_DIZZY, M_EAT };
Mood mood = M_NORMAL, prevMood = M_NORMAL;

unsigned long bootMillis = 0, baseSeconds = 0, lastFrame = 0, frameNo = 0;
unsigned long lastInteract = 0, happyUntil = 0, dizzyUntil = 0, eatUntil = 0;
unsigned long lastShakeTick = 0, shakeWindowStart = 0, lastLove = 0, lastZzz = 0;
unsigned long toastUntil = 0, ledFlashUntil = 0, lastLed = 0, lastSensor = 0;
uint32_t ledFlashColor = PINK;
int shakeCount = 0, love = 0, lastHour = -1;
String toastText = "";
float tempC = 0, rawTemp = 0, humidity = 0, tempOffset = TEMP_OFFSET;
bool sensorsReady = false;
bool timeSynced = false, manualTime = false, syncAnnounced = false;
bool pcTimeArrived = false, wifiTestRequested = false;
int syncFails = 0;
String wifiProblem = "";
bool calMode = false, calWaitRelease = false;
unsigned long lastSyncTry = 0, comboSince = 0, calLastPress = 0;
bool beatOn = false, lastBeat = false, lastToastOn = false, prevOverlap = true, topChanged = true;
int lastMinuteDrawn = -1, lineW = 1;

struct Cat { float x = 160, vx = 0, y = 0, vy = 0, look = 0, squash = 0; bool grounded = true; } cat;
struct FishT { bool active = false; float x = 0, y = 0, vy = 0; } fish;
struct Btn { bool down = false, repeated = false; unsigned long since = 0, lastRepeat = 0; };
Btn btnA, btnB;
bool comboUsed = false;

// motion: how the board is being moved
float tiltSide = 0, wanderTarget = -1;
int accPrevX = 0, accPrevY = 0, accPrevZ = 0, bumpCount = 0, bumpStreak = 0, fps = 0, fpsFrames = 0;
bool motionPrimed = false, faceDown = false;
unsigned long lastMotionMs = 0, lastBumpMs = 0, bumpWindowStart = 0, lastHop = 0, faceDownSince = 0;
unsigned long scaredUntil = 0, surprisedUntil = 0, lastWhee = 0, lastBonk = 0, wanderNext = 0;
unsigned long fpsStamp = 0, accDebugUntil = 0;

// ---------- clock ----------
int twoDigitsToInt(const char *p) { return (p[0] - '0') * 10 + (p[1] - '0'); }

void setClockFromCompileTime() {
  const char *t = __TIME__;
  baseSeconds = (twoDigitsToInt(t) * 3600UL + twoDigitsToInt(t + 3) * 60UL + twoDigitsToInt(t + 6) + UPLOAD_DELAY_SEC) % 86400UL;
  bootMillis = millis();
}
unsigned long currentSeconds() { return (baseSeconds + (millis() - bootMillis) / 1000UL) % 86400UL; }
float secondFraction() { return ((millis() - bootMillis) % 1000UL) / 1000.0f; }

void addClockSeconds(long amount) {
  long value = ((long)currentSeconds() + amount) % 86400L;
  if (value < 0) value += 86400L;
  baseSeconds = (unsigned long)value;
  bootMillis = millis();
}
String twoDigit(int v) { return v < 10 ? String("0") + String(v) : String(v); }

// ---------- time from the computer over the USB cable ----------
#if ARDUINO_USB_MODE && !ARDUINO_USB_CDC_ON_BOOT
#define HAS_USB_SERIAL 1
#else
#define HAS_USB_SERIAL 0
#endif
struct LineIn { char buf[48]; uint8_t n = 0; };
LineIn lineUart, lineUsb;

void pcLog(const String &msg) {
  Serial.println(msg);
#if HAS_USB_SERIAL
  USBSerial.println(msg);
#endif
}

// set the clock from "local time" seconds since 1970
void setClockLocal(time_t localEpoch) {
  struct timeval tv = { localEpoch, 0 };
  settimeofday(&tv, nullptr);
  baseSeconds = (unsigned long)(localEpoch % 86400);
  bootMillis = millis();
  timeSynced = true;
}

// SET_TIME.bat on the computer sends a line like:  T=14:05:09,2026-11-11
bool applyPcTime(const char *line) {
  int h, m, s, Y = 0, M = 1, D = 1;
  if (sscanf(line, "T=%d:%d:%d,%d-%d-%d", &h, &m, &s, &Y, &M, &D) < 6) return false;
  if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59 || Y < 2024) return false;
  struct tm t = {};
  t.tm_year = Y - 1900; t.tm_mon = M - 1; t.tm_mday = D;
  t.tm_hour = h; t.tm_min = m; t.tm_sec = s;
  setClockLocal(mktime(&t));
  manualTime = true;                 // the computer's time wins, WiFi will not overwrite it
  pcTimeArrived = true;
  return true;
}
void pollStream(Stream &io, LineIn &in) {
  while (io.available()) {
    char c = io.read();
    if (c == '\n' || c == '\r') {
      in.buf[in.n] = 0;
      in.n = 0;
      if (applyPcTime(in.buf)) io.println("TIME OK");
      else if (strcmp(in.buf, "WIFI?") == 0) wifiTestRequested = true;
      else if (strcmp(in.buf, "ACC?") == 0) accDebugUntil = millis() + 8000;
    } else if (in.n < sizeof(in.buf) - 1) in.buf[in.n++] = c;
  }
}
void pollPcTime() {
  pollStream(Serial, lineUart);
#if HAS_USB_SERIAL
  pollStream(USBSerial, lineUsb);
#endif
}

// ---------- time from the internet ----------
time_t ntpUtcTime() {
  time_t before = time(nullptr);
  unsigned long t0 = millis();
  struct timeval zero = { 0, 0 };
  settimeofday(&zero, nullptr);            // so a fresh answer is easy to recognise
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
  while (millis() - t0 < 6000) {
    time_t now = time(nullptr);
    if (now > 1600000000) return now;
    delay(100);
  }
  struct timeval back = { before + (time_t)((millis() - t0) / 1000), 0 };
  settimeofday(&back, nullptr);            // nobody answered: keep the old clock
  return 0;
}

// plan B when a network blocks time servers: read the date header of a web page
time_t httpUtcTime() {
  WiFiClient client;
  if (!client.connect("www.google.com", 80, 4000)) return 0;
  client.print("HEAD / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n");
  unsigned long start = millis();
  String line = "";
  time_t result = 0;
  bool done = false;
  while (!done && millis() - start < 5000) {
    while (client.available() && !done) {
      char c = client.read();
      if (c != '\n') { if (line.length() < 80) line += c; continue; }
      char wd[8], mon[8];
      int d, y, h, m, s;
      if (sscanf(line.c_str(), "Date: %3s, %d %3s %d %d:%d:%d", wd, &d, mon, &y, &h, &m, &s) == 7) {
        const char *names = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *p = strstr(names, mon);
        if (p) {
          struct tm t = {};
          t.tm_year = y - 1900; t.tm_mon = (p - names) / 3; t.tm_mday = d;
          t.tm_hour = h; t.tm_min = m; t.tm_sec = s;
          result = mktime(&t);
        }
        done = true;
      } else if (line.length() <= 1) done = true;   // end of the headers
      line = "";
    }
    if (!client.connected() && !client.available()) break;
    delay(10);
  }
  client.stop();
  return result;
}

// connect to WiFi, read the real time, then switch WiFi off again
bool syncTimeFromInternet() {
  if (WIFI_NAME[0] == 0) return false;
  bool ok = false;
  wifiProblem = "";
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000 && !pcTimeArrived) {
    pollPcTime();
    delay(100);
  }
  int st = WiFi.status();
  pcLog(String("WIFI status=") + st);
  if (pcTimeArrived) {
    // the computer already gave the time, nothing else to do
  } else if (st == WL_CONNECTED) {
    time_t utc = ntpUtcTime();
    pcLog(String("WIFI ntp=") + (long)utc);
    if (!utc) { utc = httpUtcTime(); pcLog(String("WIFI http=") + (long)utc); }
    if (utc) { setClockLocal(utc + (long)(TIMEZONE_HOURS * 3600)); ok = true; }
    else wifiProblem = "wifi ok, no internet";
  } else {
    // list what the board can see, to find out why it could not join
    WiFi.disconnect();
    delay(200);
    int n = WiFi.scanNetworks();
    bool seen = false;
    for (int i = 0; i < n; i++) {
      pcLog(String("WIFI sees: [") + WiFi.SSID(i) + "] ch" + WiFi.channel(i) + " " + WiFi.RSSI(i) + "dBm");
      if (WiFi.SSID(i) == WIFI_NAME) seen = true;
    }
    wifiProblem = seen ? "wifi password wrong?" : "wifi name not found";
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  lastSyncTry = millis();
  syncFails = ok ? 0 : syncFails + 1;
  pcLog(String("WIFI RESULT ") + (ok ? String("ok") : wifiProblem));
  return ok;
}

// "Wed 11 Nov" once the real date is known
String dateText() {
  static const char *DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static const char *MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (!timeSynced || t.tm_year < 120) return String("");
  return String(DAYS[t.tm_wday]) + " " + String(t.tm_mday) + " " + String(MONTHS[t.tm_mon]);
}

// ---------- sound / light / toast ----------
void chirp(int freq, int len) {
#if ENABLE_SOUND
  music.playTone(freq, len);
#endif
}
void soundMeow()  { chirp(988, 900); chirp(784, 1400); }
void soundClick() { chirp(1319, 500); }
void soundChime() { chirp(784, 1200); chirp(988, 1200); chirp(1175, 2000); }
void soundDizzy() { chirp(659, 900); chirp(523, 900); chirp(392, 1400); }

void toast(const char *text, unsigned long ms) { toastText = text; toastUntil = millis() + ms; topChanged = true; }
void toastTemp() {
  toastText = String("temp ") + String(tempC, 1) + "C  A- B+";
  toastUntil = millis() + 6500;
  topChanged = true;
}
void flashLed(uint32_t color, unsigned long ms) { ledFlashColor = color; ledFlashUntil = millis() + ms; }
void poke() { lastInteract = millis(); }

// ---------- drawing primitives ----------
void setLineW(int w) { lineW = w; CV->canvasSetLineWidth(w); }
void fillRect(int x, int y, int w, int h, uint32_t c) { if (w > 0 && h > 0) CV->canvasRectangle(x, y, w, h, c, c, true); }
// filled disc in a single draw call: a ring as thick as its radius
void fillCircle(int x, int y, int r, uint32_t c) {
  if (r < 2) { fillRect(x - r, y - r, 2 * r + 1, 2 * r + 1, c); return; }
  CV->canvasSetLineWidth(r);
  CV->canvasCircle(x, y, r, c, c, false);
  CV->canvasSetLineWidth(lineW);
}

// rounded bar between two points (horizontal or vertical)
void capsule(int x1, int y1, int x2, int y2, int r, uint32_t c) {
  if (y1 == y2) fillRect(min(x1, x2), y1 - r, abs(x2 - x1), 2 * r, c);
  else          fillRect(x1 - r, min(y1, y2), 2 * r, abs(y2 - y1), c);
  fillCircle(x1, y1, r, c);
  fillCircle(x2, y2, r, c);
}

void roundRect(int x, int y, int w, int h, int r, uint32_t c) {
  fillRect(x + r, y, w - 2 * r, h, c);
  fillRect(x, y + r, w, h - 2 * r, c);
  fillCircle(x + r, y + r, r, c);
  fillCircle(x + w - r, y + r, r, c);
  fillCircle(x + r, y + h - r, r, c);
  fillCircle(x + w - r, y + h - r, r, c);
}

float edgeX(int ya, int xa, int yb, int xb, int y) {
  return ya == yb ? xb : xa + (xb - xa) * (float)(y - ya) / (float)(yb - ya);
}
void fillTri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
  int t;
  if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  int oldW = lineW;
  setLineW(2);                               // 2px rows = half the draw calls
  for (int y = y0; y <= y2; y += 2) {
    float xa = edgeX(y0, x0, y2, x2, y);
    float xb = y < y1 ? edgeX(y0, x0, y1, x1, y) : edgeX(y1, x1, y2, x2, y);
    CV->canvasLine((int)xa, y, (int)xb, y, c);
  }
  setLineW(oldW);
}

void drawHeart(int x, int y, int s, uint32_t c) {
  if (s < 2) { fillCircle(x, y, 1, c); return; }
  int o = s * 4 / 5;
  fillCircle(x - o, y, s, c);
  fillCircle(x + o, y, s, c);
  fillTri(x - o - s + 1, y + 1, x + o + s - 1, y + 1, x, y + s * 2, c);
}
void drawSpark(int x, int y, int s, uint32_t c) {
  CV->canvasLine(x - s, y, x + s, y, c);
  CV->canvasLine(x, y - s, x, y + s, c);
  if (s > 2) fillCircle(x, y, 1, c);
}
void drawZ(int x, int y, int s, uint32_t c) {
  CV->canvasLine(x, y, x + s, y, c);
  CV->canvasLine(x + s, y, x, y + s, c);
  CV->canvasLine(x, y + s, x + s, y + s, c);
}
void drawPaw(int x, int y, uint32_t c) {
  fillCircle(x, y + 3, 4, c);
  fillCircle(x - 5, y - 2, 2, c);
  fillCircle(x, y - 4, 2, c);
  fillCircle(x + 5, y - 2, 2, c);
}

// ---------- particles ----------
enum PType : uint8_t { P_HEART, P_SPARK, P_ZZZ };
struct Particle { float x, y, vx, vy; int16_t life, maxLife; uint8_t type, size; uint32_t color; };
const int MAX_PARTICLES = 28;
Particle parts[MAX_PARTICLES];

void addParticle(uint8_t type, float x, float y, float vx, float vy, int life, int size, uint32_t color) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (parts[i].life <= 0) {
      parts[i] = { x, y, vx, vy, (int16_t)life, (int16_t)life, type, (uint8_t)size, color };
      return;
    }
  }
}
void burstHearts(float x, float y, int count) {
  const uint32_t cols[] = { PINK, LILAC, HOT };
  for (int i = 0; i < count; i++)
    addParticle(P_HEART, x + random(-22, 23), y + random(-8, 9), random(-12, 13) / 10.0f, -random(12, 28) / 10.0f,
          random(26, 44), random(3, 6), cols[random(0, 3)]);
}
void burstSparks(float x, float y, int count, uint32_t color) {
  for (int i = 0; i < count; i++)
    addParticle(P_SPARK, x, y, random(-30, 31) / 10.0f, -random(10, 40) / 10.0f, random(14, 26), random(2, 5), color);
}
void updateParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle &p = parts[i];
    if (p.life <= 0) continue;
    p.life--;
    if (p.type == P_HEART)      { p.x += p.vx + sinf((frameNo + i * 7) * 0.25f) * 0.6f; p.vy *= 0.98f; }
    else if (p.type == P_SPARK) { p.x += p.vx; p.vy += 0.18f; }
    else                        { p.x += sinf((frameNo + i * 5) * 0.12f) * 0.5f; }
    p.y += p.vy;
  }
}
void drawParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle &p = parts[i];
    if (p.life <= 0) continue;
    float f = (float)p.life / p.maxLife;
    uint32_t c = lerpColor(th.bg, p.color, f < 0.5f ? f * 2 : 1);
    int s = p.size;
    if (p.type == P_HEART)      drawHeart(p.x, p.y, max(1, (int)(s * (0.5f + f * 0.5f))), c);
    else if (p.type == P_SPARK) drawSpark(p.x, p.y, s, c);
    else                        drawZ(p.x, p.y, s + (int)((1 - f) * 4), c);
  }
}

bool particlesHigh() {
  for (int i = 0; i < MAX_PARTICLES; i++)
    if (parts[i].life > 0 && parts[i].y < 152) return true;
  return false;
}

// ---------- background ----------
void drawCloud(int x, int y, uint32_t c) {
  fillCircle(x, y, 9, c);
  fillCircle(x + 11, y - 4, 11, c);
  fillCircle(x + 23, y, 8, c);
  fillRect(x, y, 23, 9, c);
}
// top part (status bar + clock card): repainted only when something there changes
void drawTopBackground() {
  fillRect(0, 0, 320, 130, th.bg);
}

// bottom part (seconds bar + the cat's garden): repainted every frame
void drawBottomBackground() {
  fillRect(0, 130, 320, 110, th.bg);
  if (th.night > 0.5f) {
    uint32_t starCol = lerpColor(th.bg, SNOW, th.night);
    for (int i = 0; i < 12; i++) {
      int sx = (i * 73 + 17) % 316 + 2;
      int sy = 152 + (i * 29) % 62;
      float tw = sinf(frameNo * 0.12f + i * 1.7f);
      if (tw > 0.6f) drawSpark(sx, sy, 2, starCol);
      else if (tw > -0.3f) fillCircle(sx, sy, 1, starCol);
    }
    fillCircle(292, 166, 12, lerpColor(th.bg, 0xFFF3C4, th.night));  // moon
    fillCircle(298, 161, 10, th.bg);
  } else {
    uint32_t cloud = lerpColor(th.bg, luma(th.bg) > 250 ? PINK2 : SNOW, 0.75f);
    drawCloud((int)(frameNo * 0.35f) % 380 - 40, 162, cloud);
    drawCloud((int)(frameNo * 0.2f + 190) % 380 - 40, 188, lerpColor(th.bg, SKY, 0.9f));
    fillCircle(300 - (int)(frameNo * 0.5f) % 340, 205, 7, lerpColor(th.bg, MINT, 0.8f));
  }
  // grass floor with bumpy top and tiny flowers
  fillRect(0, FLOOR_Y, 320, 240 - FLOOR_Y, th.floor);
  for (int x = 8; x < 320; x += 24) fillCircle(x, FLOOR_Y + 1, 5, th.floor);
  for (int x = 30; x < 320; x += 86) {
    fillCircle(x, FLOOR_Y + 8, 2, th.accent);
    fillCircle(x + 40, FLOOR_Y + 11, 2, lerpColor(th.floor, SNOW, 0.8f));
  }
}

// ---------- status bar ----------
const char *moodLabel() {
  switch (mood) {
    case M_HAPPY: return "happy!";
    case M_SLEEP: return "zzz...";
    case M_DIZZY: return "@_@ dizzy";
    case M_EAT:   return "nom nom";
    default:      return "purr~";
  }
}
void drawStatusBar() {
  drawHeart(14, 9, beatOn ? 6 : 5, HOT);
  CV->canvasText(String(love), 28, 4, th.inkBg, CV->eCNAndENFont16, 50, false);
  String mid = dateText();                                  // real date when the time came from WiFi
  if (mid.length() == 0 || mood != M_NORMAL) mid = String(moodLabel());
  if (millis() < toastUntil) mid = toastText;
  CV->canvasText(mid, 160 - (int)mid.length() * 4, 4, th.inkBg, CV->eCNAndENFont16, 50, false);
#if ENABLE_SENSORS
  if (sensorsReady) {
    String s = String((int)roundf(tempC)) + "C " + String((int)roundf(humidity)) + "%";
    CV->canvasText(s, 312 - (int)s.length() * 8, 4, th.inkBg, CV->eCNAndENFont16, 50, false);
  }
#else
  CV->canvasText("Qatwti", 262, 4, th.inkBg, CV->eCNAndENFont16, 50, false);
#endif
}

// ---------- clock card with chunky bubble digits ----------
const uint8_t SEGMENTS[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };  // gfedcba

void drawDigit(int d, int x, int y, int w, int h, int r, uint32_t c) {
  uint8_t s = SEGMENTS[d % 10];
  int l = x + r, rt = x + w - r, t = y + r, m = y + h / 2, b = y + h - r;
  if (s & 0x01) capsule(l, t, rt, t, r, c);
  if (s & 0x02) capsule(rt, t, rt, m, r, c);
  if (s & 0x04) capsule(rt, m, rt, b, r, c);
  if (s & 0x08) capsule(l, b, rt, b, r, c);
  if (s & 0x10) capsule(l, m, l, b, r, c);
  if (s & 0x20) capsule(l, t, l, m, r, c);
  if (s & 0x40) capsule(l, m, rt, m, r, c);
}

void drawClockCard(int hour, int minute) {
  roundRect(12, 33, 296, 96, 14, th.shadow);
  roundRect(12, 28, 296, 96, 14, th.card);
  uint32_t soft = lerpColor(th.card, th.accent, 0.45f);
  drawPaw(292, 44, soft);
  drawPaw(28, 108, soft);

  int shown = hour;
#if USE_12H
  shown = hour % 12; if (shown == 0) shown = 12;
  CV->canvasText(hour < 12 ? "AM" : "PM", 22, 34, soft, CV->eCNAndENFont16, 50, false);
#endif
  const int xs[4] = { 34, 94, 178, 238 };
  const int ds[4] = { shown / 10, shown % 10, minute / 10, minute % 10 };
  for (int i = 0; i < 4; i++) {
    drawDigit(ds[i], xs[i] + 2, 43, 48, 68, 6, soft);   // soft drop shadow
    drawDigit(ds[i], xs[i], 40, 48, 68, 6, th.ink);
  }
  // heartbeat colon
  int hs = beatOn ? 6 : 5;
  drawHeart(160, 58, hs, HOT);
  drawHeart(160, 86, hs, th.accent);
}

// seconds bar with a tiny paw riding on it
void drawSecondsBar(int second) {
  float p = (second + secondFraction()) / 60.0f;
  int endX = 26 + (int)(236 * p);
  capsule(26, 138, 262, 138, 4, th.shadow);
  capsule(26, 138, endX, 138, 4, th.accent);
  fillCircle(endX, 138, 6, HOT);
  fillCircle(endX, 138, 2, SNOW);
  CV->canvasText(twoDigit(second), 278, 130, th.inkBg, CV->eCNAndENFont16, 50, false);
}

// ---------- fish ----------
void drawFish() {
  if (!fish.active) return;
  int x = fish.x, y = fish.y;
  fillTri(x + 8, y, x + 16, y - 6, x + 16, y + 6, FISH);
  capsule(x - 6, y, x + 5, y, 5, FISH);
  fillCircle(x - 6, y - 1, 1, INK_DARK);
  if ((frameNo / 6) % 2) drawSpark(x + 2, y - 12, 2, HONEY);
}
void dropFish() {
  poke();
  if (fish.active) return;
  fish.active = true;
  fish.x = cat.x < 160 ? random(200, 281) : random(40, 121);
  fish.y = 146;
  fish.vy = 0;
  toast("fish time!", 1600);
  flashLed(SKY, 350);
  soundClick();
}
void updateFish() {
  if (!fish.active) return;
  fish.vy += 0.6f;
  fish.y += fish.vy;
  if (fish.y > FLOOR_Y - 7) {
    fish.y = FLOOR_Y - 7;
    fish.vy = fabsf(fish.vy) > 1.5f ? -fish.vy * 0.4f : 0;
  }
  if (mood != M_DIZZY && fabsf(fish.x - cat.x) < 22 && fish.y > FLOOR_Y - 30) {
    fish.active = false;
    unsigned long now = millis();
    eatUntil = now + 1500;
    happyUntil = now + 3800;
    love += 3;
    burstHearts(cat.x, CAT_BASE_Y - 30, 7);
    burstSparks(fish.x, fish.y - 6, 6, HONEY);
    toast("yum! +3", 1600);
    flashLed(HOT, 500);
    soundMeow();
  }
}

// ---------- cat ----------
void catJump(float power) {
  if (!cat.grounded) return;
  cat.vy = -power * 25.0f;      // pixels per second
  cat.grounded = false;
}

// when nobody tilts the board the cat strolls around by itself
float wanderAccel(unsigned long now) {
  if (now > wanderNext) {
    wanderNext = now + random(2500, 7000);
    wanderTarget = random(0, 3) == 0 ? -1 : random(CAT_MIN_X + 16, CAT_MAX_X - 15);   // -1 = sit still
  }
  if (wanderTarget < 0 || fabsf(wanderTarget - cat.x) < 8) return 0;
  return wanderTarget > cat.x ? 170.0f : -170.0f;
}

void hitWall(unsigned long now) {
  if (fabsf(cat.vx) < 110) { cat.vx = 0; return; }
  cat.squash = 0.7f;                                  // bonk!
  burstSparks(cat.x + (cat.vx > 0 ? 24 : -24), CAT_BASE_Y + 6, 4, HONEY);
  cat.vx = -cat.vx * 0.35f;
  scaredUntil = now + 500;
  if (now - lastBonk > 1500) { lastBonk = now; toast("bonk!", 900); chirp(330, 350); }
}

// dt = seconds since the last frame, so the cat moves the same at any frame rate
void updateCat(float dt) {
  unsigned long now = millis();
  if (fabsf(tiltSide) > 200 && !faceDown) poke();     // being tilted keeps the cat awake

  float accel = 0;                                    // pixels per second, per second
  if (mood == M_DIZZY) accel = sinf(now / 90.0f) * 430.0f;            // wobble around
  else if (mood != M_SLEEP) {
    bool tilted = fabsf(tiltSide) > 60;
    if (tilted) accel += tiltSide * 1.6f;                             // slide downhill
    if (fish.active) {
      if (fabsf(fish.x - cat.x) > 10) accel += fish.x > cat.x ? 340.0f : -340.0f;   // chase the fish
    } else if (!tilted) accel = wanderAccel(now);
    if (fabsf(tiltSide) > 450 && fabsf(cat.vx) > 140) {               // steep: sliding fast
      scaredUntil = now + 350;
      if (now - lastWhee > 6000) { lastWhee = now; toast("wheee!", 1200); }
    }
  }
  cat.vx = (cat.vx + accel * dt) * expf(-3.2f * dt);
  cat.x += cat.vx * dt;
  if (cat.x < CAT_MIN_X) { cat.x = CAT_MIN_X; hitWall(now); }
  if (cat.x > CAT_MAX_X) { cat.x = CAT_MAX_X; hitWall(now); }

  if (!cat.grounded) {
    cat.vy += 560.0f * dt;
    cat.y += cat.vy * dt;
    if (cat.y >= 0) {
      cat.squash = min(1.0f, cat.vy / 200.0f);
      cat.y = 0; cat.vy = 0; cat.grounded = true;
      burstSparks(cat.x, FLOOR_Y - 2, 3, lerpColor(th.floor, SNOW, 0.6f));
    }
  }
  cat.squash *= expf(-5.0f * dt);
  float target = constrain(cat.vx / 100.0f + tiltSide / 700.0f, -1.0f, 1.0f);   // looks where it slides
  cat.look += (target - cat.look) * min(1.0f, 5.0f * dt);

  if (mood == M_SLEEP && now - lastZzz > 1300) {
    lastZzz = now;
    addParticle(P_ZZZ, cat.x + 22, CAT_BASE_Y - 30, 0, -0.55f, 55, 4, lerpColor(th.bg, LILAC, 0.9f));
  }
}

void drawEye(int ex, int ey) {
  bool blink = (frameNo % 70) < 3;
  switch (mood) {
    case M_HAPPY:
    case M_EAT:      // ^ ^
      CV->canvasLine(ex - 5, ey + 1, ex, ey - 4, INK_DARK);
      CV->canvasLine(ex, ey - 4, ex + 5, ey + 1, INK_DARK);
      break;
    case M_SLEEP:    // closed, curved down
      CV->canvasLine(ex - 5, ey - 1, ex, ey + 2, INK_DARK);
      CV->canvasLine(ex, ey + 2, ex + 5, ey - 1, INK_DARK);
      break;
    case M_DIZZY:    // x x
      CV->canvasLine(ex - 4, ey - 4, ex + 4, ey + 4, INK_DARK);
      CV->canvasLine(ex - 4, ey + 4, ex + 4, ey - 4, INK_DARK);
      break;
    default:
      if (millis() < scaredUntil) {            // wide eyes
        fillCircle(ex, ey, 5, INK_DARK);
        fillCircle(ex, ey, 4, SNOW);
        fillCircle(ex, ey, 2, INK_DARK);
      } else if (blink) CV->canvasLine(ex - 4, ey, ex + 4, ey, INK_DARK);
      else {
        fillCircle(ex, ey, 4, INK_DARK);
        fillCircle(ex + 1, ey - 2, 1, SNOW);
      }
  }
}

void drawCat() {
  unsigned long now = millis();
  bool moving = fabsf(cat.vx) > 15.0f && cat.grounded;
  float bob = mood == M_SLEEP ? sinf(now / 600.0f) * 1.5f + 3 : (moving ? (frameNo / 2) % 2 * 2 : (frameNo / 6) % 2);
  int x = cat.x;
  int y = CAT_BASE_Y + (int)cat.y + (int)bob + (int)(cat.squash * 5);
  int bodyR = 23 + (int)(cat.squash * 4);
  int lo = (int)(cat.look * 3);              // face shifts toward the walking direction
  int td = cat.look > 0.2f ? -1 : 1;         // tail trails behind

  // soft shadow on the grass
  int sw = max(6, 22 + (int)(cat.y * 0.25f));
  capsule(x - sw, FLOOR_Y + 2, x + sw, FLOOR_Y + 2, 2, lerpColor(th.floor, 0x000000, 0.18f));

  // tail
  int wag = mood == M_SLEEP ? 0 : (int)(sinf(frameNo * (mood == M_HAPPY ? 0.9f : 0.35f)) * 6);
  setLineW(5);
  if (mood == M_SLEEP) CV->canvasLine(x + td * 18, y + 40, x + td * 36, y + 44, PEACH);
  else                 CV->canvasLine(x + td * 20, y + 25, x + td * 34 + wag, y + 9, PEACH);
  setLineW(1);
  if (mood != M_SLEEP) fillCircle(x + td * 34 + wag, y + 9, 3, STRIPE);

  // body, belly, feet
  fillCircle(x, y + 24, bodyR, PEACH);
  fillCircle(x, y + 30, 12, CREAM);
  int step = moving ? (frameNo / 3) % 2 : -1;
  fillCircle(x - 12, y + 42 - (step == 0 ? 3 : 0), 8, CREAM);
  fillCircle(x + 12, y + 42 - (step == 1 ? 3 : 0), 8, CREAM);

  // the head leans downhill when the board is tilted
  if (mood != M_SLEEP) x += constrain((int)(tiltSide / 110.0f), -7, 7);

  // ears (wiggle when happy)
  int ew = (mood == M_HAPPY || mood == M_EAT) ? (int)(sinf(frameNo * 0.8f) * 2) : 0;
  int ed = mood == M_SLEEP ? 4 : 0;
  fillTri(x - 25, y - 6, x - 15 + ew, y - 27 + ed, x - 3, y - 12, PEACH);
  fillTri(x + 25, y - 6, x + 15 + ew, y - 27 + ed, x + 3, y - 12, PEACH);
  fillTri(x - 20, y - 10, x - 15 + ew, y - 21 + ed, x - 9, y - 13, PINK2);
  fillTri(x + 20, y - 10, x + 15 + ew, y - 21 + ed, x + 9, y - 13, PINK2);

  // head + tabby stripes
  fillCircle(x, y, 27, PEACH);
  setLineW(2);
  CV->canvasLine(x - 7, y - 25, x - 6, y - 18, STRIPE);
  CV->canvasLine(x, y - 26, x, y - 18, STRIPE);
  CV->canvasLine(x + 7, y - 25, x + 6, y - 18, STRIPE);

  // face
  drawEye(x - 10 + lo, y - 2);
  drawEye(x + 10 + lo, y - 2);
  setLineW(1);
  int blush = (mood == M_HAPPY || mood == M_EAT) ? 5 : 4;
  fillCircle(x - 18 + lo, y + 8, blush, PINK);
  fillCircle(x + 18 + lo, y + 8, blush, PINK);
  fillCircle(x + lo, y + 5, 2, HOT);
  if (mood == M_EAT && (frameNo / 3) % 2) fillCircle(x + lo, y + 11, 3, INK_DARK);       // chewing
  else if (mood == M_DIZZY || now < scaredUntil) fillCircle(x + lo, y + 11, 2, INK_DARK);   // "o"
  else {
    CV->canvasLine(x + lo, y + 7, x - 5 + lo, y + 11, INK_DARK);
    CV->canvasLine(x + lo, y + 7, x + 5 + lo, y + 11, INK_DARK);
  }
  uint32_t whisker = lerpColor(PEACH, INK_DARK, 0.45f);
  CV->canvasLine(x - 22, y + 3, x - 34, y + 1, whisker);
  CV->canvasLine(x - 22, y + 7, x - 34, y + 9, whisker);
  CV->canvasLine(x + 22, y + 3, x + 34, y + 1, whisker);
  CV->canvasLine(x + 22, y + 7, x + 34, y + 9, whisker);

  // "!" when something knocks the board
  if (now < surprisedUntil) {
    capsule(x + 30, y - 46, x + 30, y - 37, 2, HOT);
    fillCircle(x + 30, y - 30, 2, HOT);
  }

  // dizzy stars orbiting the head
  if (mood == M_DIZZY) {
    for (int i = 0; i < 3; i++) {
      float a = now / 160.0f + i * 2.094f;
      drawSpark(x + (int)(cosf(a) * 30), y - 32 + (int)(sinf(a) * 7), 3, HONEY);
    }
  }
}

// ---------- input ----------
void adjustClock(long step, const char *label, uint32_t color) {
  addClockSeconds(step);
  manualTime = true;                      // you set it by hand, so WiFi will not overwrite it
  lastHour = currentSeconds() / 3600UL;   // manual change must not ring the hourly chime
  toast(label, 900);
  flashLed(color, 250);
  soundClick();
}
void handleButton(Btn &s, bool pressed, unsigned long now, long step, const char *label, uint32_t color) {
  if (pressed && !s.down) {
    s.down = true; s.repeated = false; s.since = now; s.lastRepeat = now;
    poke();
  } else if (pressed && s.down) {
    if (!comboUsed && now - s.since > 600 && now - s.lastRepeat > 140) {   // hold = fast forward
      s.lastRepeat = now; s.repeated = true;
      adjustClock(step, label, color);
    }
  } else if (!pressed && s.down) {
    s.down = false;
    if (!comboUsed && !s.repeated) adjustClock(step, label, color);
  }
}

// temperature fix mode: A = -0.5 C, B = +0.5 C, saves itself after 6 quiet seconds
void handleCalibration(bool a, bool b, unsigned long now) {
  if (calWaitRelease) {                       // wait until the two buttons are let go
    if (!a && !b) calWaitRelease = false;
    btnA.down = a; btnB.down = b;
    return;
  }
  bool pressA = a && !btnA.down, pressB = b && !btnB.down;
  btnA.down = a; btnB.down = b;
  if (pressA) tempOffset -= 0.5f;
  if (pressB) tempOffset += 0.5f;
  if (pressA || pressB) {
    calLastPress = now;
    tempC = rawTemp + tempOffset;
    toastTemp();
    soundClick();
  }
  if (now - calLastPress > 6000 && !a && !b) {
    calMode = false;
    comboUsed = false;
#if ENABLE_SENSORS
    prefs.putFloat("tOff", tempOffset);
#endif
    toast("saved!", 1500);
  }
}

void handleControls() {
  unsigned long now = millis();
  bool a = k10.buttonA->isPressed(), b = k10.buttonB->isPressed();
  if (calMode) {
    handleCalibration(a, b, now);
  } else {
    if (a && b) {
      if (!comboUsed) { comboUsed = true; comboSince = now; dropFish(); }
      else if (sensorsReady && now - comboSince > 2000) {   // long hold = temperature fix mode
        calMode = true; calWaitRelease = true; calLastPress = now;
        toastTemp();
        flashLed(SKY, 400);
        return;
      }
    }
    handleButton(btnA, a, now, 3600, "+1 hour", PINK);
    handleButton(btnB, b, now, 60, "+1 min", MINT);
    if (!a && !b) comboUsed = false;
  }

  updateMotion();
}

// ---------- motion: the cat feels how the board is moved ----------
void onBump(float jerk, unsigned long now) {
  if (now - bumpWindowStart > 3000) { bumpWindowStart = now; bumpCount = 0; }
  bumpCount++;
  bumpStreak = now - lastBumpMs < 450 ? bumpStreak + 1 : 1;
  lastBumpMs = now;
  if (now < dizzyUntil || faceDown) return;
  if (bumpCount >= 12) {                                   // shaken for a long time
    bumpCount = 0;
    dizzyUntil = now + 3200;
    toast("too much shaking!", 2000);
    flashLed(HONEY, 500);
    soundDizzy();
  } else if (bumpStreak >= 3 && now - lastLove > 900) {    // a real shake
    lastLove = now;
    love++;
    happyUntil = now + 2600;
    burstHearts(cat.x, CAT_BASE_Y - 28 + cat.y, 6);
    toast("love you! +1", 1600);
    flashLed(HOT, 350);
    soundMeow();
  } else if (bumpStreak == 1 && now - lastHop > 500) {     // one knock: a startled hop
    lastHop = now;
    surprisedUntil = now + 700;
    scaredUntil = now + 700;
    catJump(min(9.0f, 4.0f + jerk / 300.0f));
    flashLed(SNOW, 120);
    chirp(1568, 300);
  }
}

void updateMotion() {
  unsigned long now = millis();
  if (now - lastMotionMs < 100) return;                    // the sensor gives 10 readings a second
  lastMotionMs = now;
  int x = k10.getAccelerometerX(), y = k10.getAccelerometerY(), z = k10.getAccelerometerZ();

  // the screen is used sideways, so "downhill to the right" is -Y on this board
  float side = TILT_USE_X ? x : -y;
  if (TILT_FLIP) side = -side;
  tiltSide += (side - tiltSide) * 0.5f;

  float jerk = 0;
  if (motionPrimed) {
    float dx = x - accPrevX, dy = y - accPrevY, dz = z - accPrevZ;
    jerk = sqrtf(dx * dx + dy * dy + dz * dz);             // how suddenly the board moved
    if (jerk > 150) poke();
    if (jerk > BUMP_SENSITIVITY) onBump(jerk, now);
  }
  accPrevX = x; accPrevY = y; accPrevZ = z;
  motionPrimed = true;

  // screen face down on the table = bed time, picking it up wakes the cat
  if (z < -600) {
    if (!faceDownSince) faceDownSince = now;
    if (!faceDown && now - faceDownSince > 600) { faceDown = true; toast("night night...", 2000); }
  } else if (z > -250) {
    faceDownSince = 0;
    if (faceDown) { faceDown = false; poke(); }
  }

  if (now < accDebugUntil)
    pcLog(String("ACC ") + x + " " + y + " " + z + " jerk" + (int)jerk + " fps" + fps + " side" + (int)tiltSide + " cat" + (int)cat.x);
}

// ---------- mood / sensors / LEDs ----------
void updateMood(int hour) {
  unsigned long now = millis();
  prevMood = mood;
  bool sleepy = hour >= 22 || hour < 6;
  unsigned long idle = now - lastInteract;
  if (faceDown)                                      mood = M_SLEEP;   // screen on the table = bed time
  else if (now < dizzyUntil)                         mood = M_DIZZY;
  else if (now < eatUntil)                           mood = M_EAT;
  else if (now < happyUntil)                         mood = M_HAPPY;
  else if (idle > (sleepy ? 15000UL : 120000UL))     mood = M_SLEEP;   // nobody moved me: nap time
  else                                               mood = M_NORMAL;
  if (prevMood == M_SLEEP && mood != M_SLEEP) { catJump(6); toast("meow?", 1600); }   // woke up
}

void updateSensors() {
#if ENABLE_SENSORS
  unsigned long now = millis();
  if (!aht20 || now - lastSensor < 2000) return;
  lastSensor = now;
  float t = aht20->getData(AHT20::eAHT20TempC);
  float h = aht20->getData(AHT20::eAHT20HumiRH);
  if (h < 1 || h > 100 || t < -30 || t > 80) return;       // no real reading yet
  rawTemp = sensorsReady ? rawTemp * 0.7f + t * 0.3f : t;   // smooth out the jitter
  humidity = sensorsReady ? humidity * 0.7f + h * 0.3f : h;
  tempC = rawTemp + tempOffset;
  sensorsReady = true;
  topChanged = true;
#endif
}

// keep the clock exact: ask the internet again twice a day (or retry after a failure)
void maybeResyncTime() {
  if (WIFI_NAME[0] == 0 || manualTime) return;
  if (syncFails >= 3) return;                               // WiFi keeps failing: stop trying
  unsigned long wait = syncFails == 0 ? 12UL * 3600UL * 1000UL : 3600UL * 1000UL;
  if (millis() - lastSyncTry < wait) return;
  if (!syncAnnounced) { syncAnnounced = true; toast("syncing time...", 20000); return; }  // show the message first
  syncAnnounced = false;
  bool ok = syncTimeFromInternet();
  lastHour = currentSeconds() / 3600UL;
  lastFrame = millis();
  toast(ok ? "time synced!" : wifiProblem.c_str(), 5000);
}

void updateLeds() {
  unsigned long now = millis();
  if (now - lastLed < 90) return;
  lastLed = now;
  if (now < ledFlashUntil) { k10.rgb->write(-1, ledFlashColor); return; }
  const uint32_t pal[4] = { PINK, LILAC, 0x9FE8C8, 0x9FD8FF };
  const uint32_t rainbow[4] = { 0xFF5A7A, HONEY, 0x6CE0A0, 0x7AA8FF };
  for (int i = 0; i < 3; i++) {
    uint32_t c;
    if (mood == M_SLEEP)      c = scaleColor(LILAC, 0.12f + 0.10f * sinf(now / 1200.0f));
    else if (mood == M_DIZZY) c = rainbow[(now / 120 + i) % 4];
    else if (mood == M_HAPPY || mood == M_EAT) c = scaleColor(HOT, 0.55f + 0.45f * sinf(now / 130.0f + i));
    else {
      float t = now / 1800.0f + i * 0.33f;
      int idx = (int)t % 4;
      c = scaleColor(lerpColor(pal[idx], pal[(idx + 1) % 4], t - (int)t), 0.6f + 0.4f * sinf(now / 700.0f));
    }
    k10.rgb->write(i, c);
  }
}

// ---------- main ----------
void setup() {
  k10.begin();
  Serial.begin(115200);
#if HAS_USB_SERIAL
  USBSerial.begin(115200);
  USBSerial.setTxTimeoutMs(0);     // never wait when no computer is listening
#endif
  setenv("TZ", "UTC0", 1);        // the clock itself is kept in local time
  tzset();
  k10.initScreen(screen_dir);
  k10.creatCanvas();
  setLineW(1);
  k10.rgb->brightness(3);
  k10.rgb->write(-1, PINK2);
#if ENABLE_SENSORS
  aht20 = new AHT20();
  prefs.begin("qatwti", false);
  tempOffset = prefs.getFloat("tOff", TEMP_OFFSET);
#endif
  randomSeed(micros());
  setClockFromCompileTime();
  if (WIFI_NAME[0] != 0) {
    // small waiting screen while the real time is fetched
    fillRect(0, 0, 320, 240, 0xFFF7FC);
    drawHeart(160, 92, 12, HOT);
    CV->canvasText("getting the time...", 84, 132, INK_DARK, CV->eCNAndENFont16, 50, false);
    CV->updateCanvas();
    bool ok = syncTimeFromInternet();
    if (ok) toast("time synced!", 3000);
    else if (!pcTimeArrived) toast(wifiProblem.c_str(), 6000);
  } else {
    toast("hi! I'm Qatwti", 2500);
  }
  lastSyncTry = millis();
  lastHour = currentSeconds() / 3600UL;
  lastInteract = millis();
  soundMeow();
}

void loop() {
  handleControls();
  updateLeds();

  // time sent from the computer (SET_TIME.bat)
  pollPcTime();
  if (pcTimeArrived) {
    pcTimeArrived = false;
    lastHour = currentSeconds() / 3600UL;
    toast("time from PC!", 3000);
    flashLed(MINT, 500);
    soundClick();
  }
  if (wifiTestRequested) {           // the computer asked for a WiFi test
    wifiTestRequested = false;
    bool ok = syncTimeFromInternet();
    lastHour = currentSeconds() / 3600UL;
    lastFrame = millis();
    toast(ok ? "time synced!" : wifiProblem.c_str(), 5000);
  }

  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  float dt = constrain((now - lastFrame) / 1000.0f, 0.005f, 0.12f);   // seconds since the last frame
  lastFrame = now;
  frameNo++;
  fpsFrames++;
  if (now - fpsStamp >= 1000) { fps = fpsFrames; fpsFrames = 0; fpsStamp = now; }

  unsigned long secs = currentSeconds();
  int hour = secs / 3600UL, minute = (secs % 3600UL) / 60UL, second = secs % 60UL;

  // hourly chime: the cat jumps for joy
  if (hour != lastHour) {
    lastHour = hour;
    poke();
    catJump(9);
    happyUntil = now + 3000;
    burstSparks(160, 70, 10, HONEY);
    toast("new hour!", 2500);
    soundChime();
  }

  updateSensors();
  updateTheme(secs / 3600.0f);
  updateMood(hour);
  updateFish();
  updateCat(dt);
  updateParticles();

  // the top (status bar + clock card) is repainted only when it changes or something flies over it
  beatOn = secondFraction() < 0.18f;
  bool toastOn = millis() < toastUntil;
  bool overlap = !cat.grounded || mood == M_DIZZY || particlesHigh() || millis() < surprisedUntil;
  bool redrawTop = overlap || prevOverlap || topChanged || beatOn != lastBeat || toastOn != lastToastOn
                   || minute != lastMinuteDrawn || mood != prevMood;
  prevOverlap = overlap; lastBeat = beatOn; lastToastOn = toastOn; lastMinuteDrawn = minute; topChanged = false;

  if (redrawTop) {
    drawTopBackground();
    drawClockCard(hour, minute);
    drawStatusBar();
  }
  drawBottomBackground();
  drawSecondsBar(second);
  drawFish();
  drawCat();
  drawParticles();
  CV->updateCanvas();
  maybeResyncTime();
}

// Gym logger, step 2b: mic screen + tap-to-log screen on the round display.
//
// Screen 1 (MIC):  tap the mic to start/stop the listening animation (UI only, no mic hardware yet).
// Screen 2 (LOG):  top = exercise (tap to cycle), weight - / +, reps - / +, LOG SET button.
// Switch screens: tap the very bottom of the screen (the two dots).
// Every logged set prints one line to the Serial Monitor:  LOG,exercise,set,weight,reps,millis
//
// Board: Waveshare ESP32-S3-Touch-LCD-1.28   Library: "GFX Library for Arduino" (moononournation)
// Arduino IDE: Board "ESP32S3 Dev Module", USB CDC On Boot = Enabled.
// Pins (Waveshare wiki): LCD DC 8, CS 9, SCLK 10, MOSI 11, MISO 12, RST 14, BL 2; Touch SDA 6, SCL 7, RST 13.

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define LCD_BL   2
#define TP_SDA   6
#define TP_SCL   7
#define TP_RST   13
#define TP_ADDR  0x15

Arduino_DataBus *bus = new Arduino_ESP32SPI(8, 9, 10, 11, 12);
Arduino_GFX *panel = new Arduino_GC9A01(bus, 14, 0, true);
Arduino_Canvas *gfx = new Arduino_Canvas(240, 240, panel);

const int CX = 120, CY = 120;
uint16_t C_BG, C_IDLE, C_ACTIVE, C_TEXT, C_DIM, C_BTN, C_LOG;

// ---- app state ----
enum Screen { SCREEN_MIC, SCREEN_LOG };
Screen screen = SCREEN_LOG;

// mic screen
bool listening = false;
unsigned long listenStart = 0;

// log screen
const char *EXERCISES[] = {"Pushdown", "Curl", "Bench", "Pulldown", "Row", "Press", "Squat", "Deadlift"};
const int NUM_EX = 8;
int exIdx = 0;
int weight = 80;                 // lb
int reps = 8;
int setCount[NUM_EX] = {0};
unsigned long flashUntil = 0;
int totalLogged = 0;

bool wasTouching = false;

// ---- touch ----
bool readTouch(int &x, int &y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)6) != 6) return false;
  Wire.read();                              // gesture
  uint8_t fingers = Wire.read();
  uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (fingers == 0) return false;
  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;
  return true;
}

void touchInit() {
  Wire.begin(TP_SDA, TP_SCL, 400000);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(10);
  digitalWrite(TP_RST, HIGH); delay(60);
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0xFE);
  Wire.write(0x01);
  Wire.endTransmission();
}

// ---- drawing helpers ----
uint16_t blend(uint16_t fg, uint16_t bg, float t) {
  int fr = (fg >> 11) & 31, fgc = (fg >> 5) & 63, fb = fg & 31;
  int br = (bg >> 11) & 31, bgc = (bg >> 5) & 63, bb = bg & 31;
  return ((br + (int)((fr - br) * t)) << 11) | ((bgc + (int)((fgc - bgc) * t)) << 5) | (bb + (int)((fb - bb) * t));
}

void textCentered(int cx, int y, const char *s, uint8_t size, uint16_t col) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int)w / 2, y);
  gfx->setTextColor(col);
  gfx->print(s);
}

void drawMic(uint16_t col) {
  gfx->fillCircle(CX, CY, 60, C_BG);
  for (int i = 0; i < 3; i++) gfx->drawCircle(CX, CY + 4, 26 - i, col);
  gfx->fillRect(CX - 30, CY - 26, 60, 30, C_BG);
  gfx->fillRoundRect(CX - 14, CY - 40, 28, 54, 14, col);
  gfx->fillRect(CX - 2, CY + 30, 4, 14, col);
  gfx->fillRoundRect(CX - 14, CY + 44, 28, 4, 2, col);
}

void pageDots() {                      // two dots at the bottom show which screen is on
  gfx->fillCircle(112, 226, 3, screen == SCREEN_MIC ? C_TEXT : C_DIM);
  gfx->fillCircle(128, 226, 3, screen == SCREEN_LOG ? C_TEXT : C_DIM);
}

void roundButton(int x, int y, int r, bool plus) {
  gfx->fillCircle(x, y, r, C_BTN);
  gfx->fillRect(x - 7, y - 1, 15, 3, C_TEXT);                 // the minus bar
  if (plus) gfx->fillRect(x - 1, y - 7, 3, 15, C_TEXT);       // add the vertical bar for plus
}

void drawMicScreen(unsigned long now) {
  if (listening) {
    float t = (now - listenStart) / 1000.0f;
    for (int k = 0; k < 3; k++) {
      float phase = fmodf(t * 0.9f + k / 3.0f, 1.0f);
      int r = 64 + (int)(phase * 52);
      uint16_t col = blend(C_ACTIVE, C_BG, 1.0f - phase);
      gfx->drawCircle(CX, CY, r, col);
      gfx->drawCircle(CX, CY, r - 1, col);
    }
    drawMic(blend(C_ACTIVE, C_BG, 0.85f + 0.15f * sinf(t * 7.0f)));
    for (int i = 0; i < 7; i++) {
      float h = 4 + 10 * fabsf(sinf(t * 6.0f + i * 0.9f));
      gfx->fillRoundRect(CX - 30 + i * 10, 182 - (int)h / 2, 4, (int)h, 2, C_ACTIVE);
    }
    textCentered(CX, 196, "Listening...", 2, C_TEXT);
  } else {
    drawMic(C_IDLE);
    textCentered(CX, 188, "Tap to log", 2, C_DIM);
  }
}

void drawLogScreen(unsigned long now) {
  char buf[24];

  // exercise name + set number
  textCentered(CX, 24, EXERCISES[exIdx], 2, C_ACTIVE);
  snprintf(buf, sizeof(buf), "Set %d", setCount[exIdx] + 1);
  textCentered(CX, 46, buf, 1, C_DIM);

  // weight row
  roundButton(42, 92, 18, false);
  roundButton(198, 92, 18, true);
  snprintf(buf, sizeof(buf), "%d", weight);
  textCentered(CX, 78, buf, 3, C_TEXT);
  textCentered(CX, 106, "lb", 1, C_DIM);

  // reps row
  roundButton(52, 144, 16, false);
  roundButton(188, 144, 16, true);
  snprintf(buf, sizeof(buf), "%d", reps);
  textCentered(CX, 130, buf, 3, C_TEXT);
  textCentered(CX, 158, "reps", 1, C_DIM);

  // LOG button
  bool flashing = now < flashUntil;
  gfx->fillRoundRect(65, 176, 110, 30, 15, flashing ? C_ACTIVE : C_LOG);
  textCentered(CX, 183, flashing ? "LOGGED" : "LOG SET", 2, C_BG);
}

void drawFrame(unsigned long now) {
  gfx->fillScreen(C_BG);
  if (screen == SCREEN_MIC) drawMicScreen(now); else drawLogScreen(now);
  pageDots();
  gfx->flush();
}

// ---- touch handling ----
bool inCircle(int x, int y, int cx, int cy, int r) {
  int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

void handleTap(int x, int y, unsigned long now) {
  if (y > 212 && x > 70 && x < 170) {              // bottom: switch screen
    screen = (screen == SCREEN_MIC) ? SCREEN_LOG : SCREEN_MIC;
    return;
  }
  if (screen == SCREEN_MIC) {
    if (inCircle(x, y, CX, CY, 90)) {
      listening = !listening;
      if (listening) listenStart = now;
    }
    return;
  }
  // LOG screen
  if (y < 62) { exIdx = (exIdx + 1) % NUM_EX; return; }                          // cycle exercise
  if (inCircle(x, y, 42, 92, 26))   { weight = max(0, weight - 5); return; }
  if (inCircle(x, y, 198, 92, 26))  { weight += 5; return; }
  if (inCircle(x, y, 52, 144, 24))  { reps = max(1, reps - 1); return; }
  if (inCircle(x, y, 188, 144, 24)) { reps += 1; return; }
  if (x > 60 && x < 180 && y > 170 && y < 212) {                                 // LOG SET
    setCount[exIdx]++;
    totalLogged++;
    flashUntil = now + 700;
    Serial.printf("LOG,%s,%d,%d,%d,%lu\n", EXERCISES[exIdx], setCount[exIdx], weight, reps, now);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  if (!gfx->begin()) Serial.println("Display init failed");
  C_BG     = gfx->color565(8, 10, 16);
  C_IDLE   = gfx->color565(150, 160, 175);
  C_ACTIVE = gfx->color565(0, 220, 190);
  C_TEXT   = gfx->color565(235, 240, 245);
  C_DIM    = gfx->color565(110, 118, 130);
  C_BTN    = gfx->color565(40, 48, 62);
  C_LOG    = gfx->color565(30, 160, 120);
  touchInit();
  Serial.println("Ready. Format: LOG,exercise,set,weight,reps,millis");
}

void loop() {
  unsigned long now = millis();
  int x, y;
  bool touching = readTouch(x, y);
  if (touching && !wasTouching) handleTap(x, y, now);
  wasTouching = touching;

  static unsigned long nextFrame = 0;
  if (now >= nextFrame) {
    nextFrame = now + 33;
    drawFrame(now);
  }
}

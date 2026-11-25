// Step 2: voice screen. A mic icon in the middle of the round screen.
// Tap it: the mic turns teal, rings pulse outward and the label says "Listening...".
// Tap again: back to idle. This is the UI only. The board has no microphone yet.
//
// Board: Waveshare ESP32-S3-Touch-LCD-1.28
// Library to install (Library Manager): "GFX Library for Arduino" by moononournation
// Arduino IDE: Board "ESP32S3 Dev Module", USB CDC On Boot = Enabled.
//
// Pins (from the Waveshare wiki):
//   LCD:   DC 8, CS 9, SCLK 10, MOSI 11, MISO 12, RST 14, backlight 2
//   Touch: SDA 6, SCL 7, RST 13, (INT 5 not used, we poll)

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define LCD_BL   2
#define TP_SDA   6
#define TP_SCL   7
#define TP_RST   13
#define TP_ADDR  0x15   // CST816S touch controller

Arduino_DataBus *bus = new Arduino_ESP32SPI(8 /*DC*/, 9 /*CS*/, 10 /*SCK*/, 11 /*MOSI*/, 12 /*MISO*/);
Arduino_GFX *panel = new Arduino_GC9A01(bus, 14 /*RST*/, 0 /*rotation*/, true /*IPS*/);
Arduino_Canvas *gfx = new Arduino_Canvas(240, 240, panel);   // draw off-screen, then flush: no flicker

const int CX = 120, CY = 120;

uint16_t C_BG, C_IDLE, C_ACTIVE, C_TEXT, C_DIM;
bool listening = false;
bool wasTouching = false;
unsigned long listenStart = 0;

// ---- touch ----
bool readTouch(int &x, int &y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)6) != 6) return false;
  uint8_t gesture = Wire.read();
  uint8_t fingers = Wire.read();
  uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  (void)gesture;
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
  Wire.beginTransmission(TP_ADDR);   // stop the chip from going to sleep
  Wire.write(0xFE);
  Wire.write(0x01);
  Wire.endTransmission();
}

// ---- drawing ----
uint16_t blend(uint16_t fg, uint16_t bg, float t) {   // t=1 -> fg, t=0 -> bg (RGB565)
  int fr = (fg >> 11) & 31, fgc = (fg >> 5) & 63, fb = fg & 31;
  int br = (bg >> 11) & 31, bgc = (bg >> 5) & 63, bb = bg & 31;
  int r = br + (int)((fr - br) * t);
  int g = bgc + (int)((fgc - bgc) * t);
  int b = bb + (int)((fb - bb) * t);
  return (r << 11) | (g << 5) | b;
}

void drawMic(uint16_t col) {
  gfx->fillCircle(CX, CY, 60, C_BG);                    // solid disk under the icon
  // U-shaped cup: a 3 px ring with its top half hidden
  for (int i = 0; i < 3; i++) gfx->drawCircle(CX, CY + 4, 26 - i, col);
  gfx->fillRect(CX - 30, CY - 26, 60, 30, C_BG);
  // capsule body
  gfx->fillRoundRect(CX - 14, CY - 40, 28, 54, 14, col);
  // stem and base
  gfx->fillRect(CX - 2, CY + 30, 4, 14, col);
  gfx->fillRoundRect(CX - 14, CY + 44, 28, 4, 2, col);
}

void drawCenteredText(const char *s, int y, uint8_t size, uint16_t col) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(CX - (int)w / 2, y);
  gfx->setTextColor(col);
  gfx->print(s);
}

void drawFrame(unsigned long now) {
  gfx->fillScreen(C_BG);

  if (listening) {
    float t = (now - listenStart) / 1000.0f;
    for (int k = 0; k < 3; k++) {                       // three pulsing rings
      float phase = fmodf(t * 0.9f + k / 3.0f, 1.0f);   // 0..1, loops
      int r = 64 + (int)(phase * 52);                   // 64 .. 116
      uint16_t col = blend(C_ACTIVE, C_BG, 1.0f - phase);
      gfx->drawCircle(CX, CY, r, col);
      gfx->drawCircle(CX, CY, r - 1, col);
    }
    float pulse = 0.85f + 0.15f * sinf(t * 7.0f);       // the mic breathes slightly
    drawMic(blend(C_ACTIVE, C_BG, pulse));

    // little level bars under the mic
    for (int i = 0; i < 7; i++) {
      float h = 4 + 10 * fabsf(sinf(t * 6.0f + i * 0.9f));
      int x = CX - 30 + i * 10;
      gfx->fillRoundRect(x, 182 - (int)h / 2, 4, (int)h, 2, C_ACTIVE);
    }
    drawCenteredText("Listening...", 198, 2, C_TEXT);
  } else {
    drawMic(C_IDLE);
    drawCenteredText("Tap to log", 190, 2, C_DIM);
  }
  gfx->flush();
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

  touchInit();
  drawFrame(millis());
  Serial.println("Ready. Tap the screen.");
}

void loop() {
  unsigned long now = millis();

  int x, y;
  bool touching = readTouch(x, y);
  if (touching && !wasTouching) {                       // new press
    int dx = x - CX, dy = y - CY;
    if (dx * dx + dy * dy < 90 * 90) {                  // inside the circle around the mic
      listening = !listening;
      if (listening) listenStart = now;
      Serial.printf("touch (%d,%d) -> %s\n", x, y, listening ? "listening" : "idle");
    }
  }
  wasTouching = touching;

  static unsigned long nextFrame = 0;
  if (now >= nextFrame) {
    nextFrame = now + 33;                               // about 30 fps
    drawFrame(now);
  }
}

// Step 1: read the QMI8658 motion sensor and count reps. Output goes to Serial Monitor.
// Board: Waveshare ESP32-S3-Touch-LCD-1.28. The sensor is on I2C: SDA = GPIO6, SCL = GPIO7.
//
// Arduino IDE: Board "ESP32S3 Dev Module", USB CDC On Boot = Enabled, Serial Monitor 115200.
//
// How it works: take the total acceleration (in g), subtract a slow average to remove gravity,
// and count one rep each time the signal swings low then high past a threshold.
// This is a first draft. Tune TH and MIN_REP_MS while watching the Serial Monitor.

#include <Wire.h>/Users/vikas/Desktop/projects/gym-logger/gym_logger

#define SDA_PIN 6
#define SCL_PIN 7

// QMI8658 registers (from the datasheet)
#define REG_WHO_AM_I 0x00   // should read 0x05
#define REG_CTRL1    0x02
#define REG_CTRL2    0x03   // accelerometer range + output rate
#define REG_CTRL7    0x08   // enable sensors
#define REG_AX_L     0x35   // accel X low byte, then X high, Y low, Y high, Z low, Z high

uint8_t imuAddr = 0;

// Tuning
const float FAST = 0.25f;       // smoothing for the quick signal
const float SLOW = 0.004f;      // slow average (about 2.5 s) that follows gravity/posture
const float TH = 0.10f;         // swing size (in g) that counts as movement
const unsigned long MIN_REP_MS = 500;   // ignore anything faster than this

float fastAvg = 1.0f, slowAvg = 1.0f;
bool initDone = false;
bool armed = true;              // true = ready to count the next upswing
unsigned long lastRepMs = 0;
int reps = 0;

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(imuAddr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

bool findImu() {
  const uint8_t addrs[2] = {0x6B, 0x6A};
  for (int i = 0; i < 2; i++) {
    imuAddr = addrs[i];
    if (readReg(REG_WHO_AM_I) == 0x05) return true;
  }
  return false;
}

bool readAccelG(float &x, float &y, float &z) {
  Wire.beginTransmission(imuAddr);
  Wire.write(REG_AX_L);
  Wire.endTransmission(false);
  if (Wire.requestFrom(imuAddr, (uint8_t)6) != 6) return false;
  int16_t rx = Wire.read() | (Wire.read() << 8);
  int16_t ry = Wire.read() | (Wire.read() << 8);
  int16_t rz = Wire.read() | (Wire.read() << 8);
  const float perG = 8192.0f;   // +/-4 g range
  x = rx / perG; y = ry / perG; z = rz / perG;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  if (!findImu()) {
    Serial.println("QMI8658 not found. Check the board and the I2C pins (SDA 6, SCL 7).");
    while (true) delay(1000);
  }
  Serial.printf("QMI8658 found at 0x%02X\n", imuAddr);

  writeReg(REG_CTRL1, 0x40);    // auto-increment address on reads
  writeReg(REG_CTRL2, 0x16);    // accel: +/-4 g, 125 Hz
  writeReg(REG_CTRL7, 0x01);    // enable accelerometer only
  delay(50);

  Serial.println("Hold the board still. At rest, mag should be about 1.0 (g).");
  Serial.println("If it is far from 1.0, tell me the numbers and I will fix the settings.");
}

void loop() {
  static unsigned long next = 0;
  unsigned long now = millis();
  if (now < next) return;
  next = now + 10;              // 100 samples per second

  float x, y, z;
  if (!readAccelG(x, y, z)) return;
  float mag = sqrtf(x * x + y * y + z * z);

  if (!initDone) { fastAvg = slowAvg = mag; initDone = true; }
  fastAvg += FAST * (mag - fastAvg);
  slowAvg += SLOW * (mag - slowAvg);
  float d = fastAvg - slowAvg;

  if (d < -TH) armed = true;
  if (armed && d > TH && (now - lastRepMs) > MIN_REP_MS) {
    reps++;
    lastRepMs = now;
    armed = false;
    Serial.printf("REP %d\n", reps);
  }

  static unsigned long lastPrint = 0;
  if (now - lastPrint > 200) {   // status line 5 times a second
    lastPrint = now;
    Serial.printf("mag=%.2f  d=%+.2f  reps=%d\n", mag, d, reps);
  }
}

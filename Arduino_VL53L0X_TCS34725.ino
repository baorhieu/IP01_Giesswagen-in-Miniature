#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_TCS34725.h>

// ── Wire (I2C0): ToF1 + ToF2 ─────────────────────────────────
#define TOF_SDA     8
#define TOF_SCL     9
#define XSHUT_PIN1  5
#define XSHUT_PIN2  6
#define TOF_ADDR1   0x30
#define TOF_ADDR2   0x31

// ── Wire1 (I2C1): RGB1 + RGB2 (pin switching) ─────────────────
#define RGB1_SDA    35
#define RGB1_SCL    36
#define RGB2_SDA    12
#define RGB2_SCL    13

// ── LED pins ──────────────────────────────────────────────────
#define LED_PIN1    40
#define LED_PIN2    41

// ── Sensor objects ────────────────────────────────────────────
VL53L0X tof1, tof2;
Adafruit_TCS34725 rgb1(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 rgb2(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

void scanBus(TwoWire& bus, const char* name) {
  Serial.print("[I2C] "); Serial.println(name);
  bool found = false;
  for (byte a = 8; a < 120; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.print("  0x"); Serial.println(a, HEX);
      found = true;
    }
  }
  if (!found) Serial.println("  (no devices)");
}

void resetSensor(int xshut, uint8_t addr, VL53L0X& sensor) {
  Serial.println("[ToF] Resetting...");
  sensor.stopContinuous();
  digitalWrite(xshut, LOW);
  delay(20);
  digitalWrite(xshut, HIGH);
  delay(20);
  sensor.init();
  sensor.setAddress(addr);
  sensor.setTimeout(500);
  sensor.startContinuous();
  Serial.println("[ToF] Reset done");
}

bool tof_sens(VL53L0X& sensor, const char* label) {
  uint16_t dist = sensor.readRangeContinuousMillimeters();
  Serial.print("["); Serial.print(label); Serial.print("] ");
  Serial.print(dist); Serial.println(" mm");
  return dist < 1000;
}

// Wire1 pins are switched before reading each RGB sensor.
// sensor.begin() is called each time because Wire1.end() can cause
// a bus glitch that leaves the TCS34725 in a non-responsive state.
bool is_green(Adafruit_TCS34725& sensor, int sda, int scl, const char* label) {
  Wire1.end();
  Wire1.begin(sda, scl);
  sensor.begin(TCS34725_ADDRESS, &Wire1);

  uint16_t r, g, b, c;
  sensor.getRawData(&r, &g, &b, &c);

  Serial.print("["); Serial.print(label); Serial.print("] ");
  Serial.print("R="); Serial.print(r);
  Serial.print(" G="); Serial.print(g);
  Serial.print(" B="); Serial.print(b);
  Serial.print(" C="); Serial.println(c);

  if (c == 0) {
    Serial.print("["); Serial.print(label); Serial.println("] No light");
    return false;
  }

  float rR = (float)r / c * 100.0f;
  float gR = (float)g / c * 100.0f;
  float bR = (float)b / c * 100.0f;

  bool result = (gR > rR * 1.2f) && (gR > bR * 1.1f) && (gR > 20.0f);
  Serial.print("["); Serial.print(label); Serial.print("] Green: ");
  Serial.println(result ? "YES" : "NO");
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[Boot] Starting...");

  pinMode(LED_PIN1,   OUTPUT); digitalWrite(LED_PIN1,   LOW);
  pinMode(LED_PIN2,   OUTPUT); digitalWrite(LED_PIN2,   LOW);
  pinMode(XSHUT_PIN1, OUTPUT); digitalWrite(XSHUT_PIN1, LOW);
  pinMode(XSHUT_PIN2, OUTPUT); digitalWrite(XSHUT_PIN2, LOW);
  delay(50);

  Wire.begin(TOF_SDA, TOF_SCL);

  // ── ToF1 ────────────────────────────────────────────────────
  tof1.setBus(&Wire);
  digitalWrite(XSHUT_PIN1, HIGH);
  delay(100);
  if (!tof1.init()) { Serial.println("[ToF1] FAILED"); while (1); }
  tof1.setAddress(TOF_ADDR1);
  tof1.setTimeout(500);
  tof1.startContinuous();
  Serial.println("[ToF1] OK at 0x30");

  // ── ToF2 ────────────────────────────────────────────────────
  tof2.setBus(&Wire);
  digitalWrite(XSHUT_PIN2, HIGH);
  delay(100);
  if (!tof2.init()) { Serial.println("[ToF2] FAILED"); while (1); }
  tof2.setAddress(TOF_ADDR2);
  tof2.setTimeout(500);
  tof2.startContinuous();
  Serial.println("[ToF2] OK at 0x31");

  // ── RGB1 ────────────────────────────────────────────────────
  Wire1.begin(RGB1_SDA, RGB1_SCL);
  if (!rgb1.begin(TCS34725_ADDRESS, &Wire1)) {
    Serial.println("[RGB1] FAILED"); while (1);
  }
  Serial.println("[RGB1] OK");

  // ── RGB2 ────────────────────────────────────────────────────
  Wire1.begin(RGB2_SDA, RGB2_SCL);
  if (!rgb2.begin(TCS34725_ADDRESS, &Wire1)) {
    Serial.println("[RGB2] FAILED"); while (1);
  }
  Serial.println("[RGB2] OK");

  Serial.println("[Boot] Final scan:");
  scanBus(Wire,  "Wire  (SDA:8,  SCL:9)");
  Wire1.begin(RGB1_SDA, RGB1_SCL); scanBus(Wire1, "Wire1 (SDA:35, SCL:36) RGB1");
  Wire1.begin(RGB2_SDA, RGB2_SCL); scanBus(Wire1, "Wire1 (SDA:12, SCL:13) RGB2");
}

void loop() {
  bool det1 = tof_sens(tof1, "ToF1");
  bool det2 = tof_sens(tof2, "ToF2");

  bool green1 = det1 ? is_green(rgb1, RGB1_SDA, RGB1_SCL, "RGB1") : false;
  bool green2 = det2 ? is_green(rgb2, RGB2_SDA, RGB2_SCL, "RGB2") : false;

  digitalWrite(LED_PIN1, (det1 && green1) ? HIGH : LOW);
  digitalWrite(LED_PIN2, (det2 && green2) ? HIGH : LOW);

  Serial.print("[LED1] "); Serial.println((det1 && green1) ? "ON" : "OFF");
  Serial.print("[LED2] "); Serial.println((det2 && green2) ? "ON" : "OFF");

  delay(100);
}

#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_TCS34725.h>

// ── Pin definitions ──────────────────────────────────────────
#define SDA_PIN     8
#define SCL_PIN     9
#define RELAY_PIN   4

// ── TCA9548A Multiplexer ──────────────────────────────────────
#define TCA_ADDR    0x70   // Default I2C address (A0=A1=A2=GND)
#define CH_TOF1     0      // Channel 0: VL53L0X #1
#define CH_TOF2     1      // Channel 1: VL53L0X #2
#define CH_RGB1     2      // Channel 2: TCS34725 #1
#define CH_RGB2     3      // Channel 3: TCS34725 #2

// ── Logic levels ─────────────────────────────────────────────
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ── Sensor objects ───────────────────────────────────────────
VL53L0X l0x1;
VL53L0X l0x2;
Adafruit_TCS34725 rgb1 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 rgb2 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// ─────────────────────────────────────────────────────────────
// Select a channel on the TCA9548A (0-7)
// Pass 255 to disable all channels
// ─────────────────────────────────────────────────────────────
void tcaSelect(uint8_t ch) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(ch < 8 ? (1 << ch) : 0);
  Wire.endTransmission();
}

// ─────────────────────────────────────────────────────────────
// Scan each active multiplexer channel for I2C devices
// ─────────────────────────────────────────────────────────────
void check() {
  Serial.println("[I2C] Scanning TCA9548A channels...");
  for (uint8_t ch = 0; ch < 4; ch++) {
    tcaSelect(ch);
    Serial.print("[I2C] Channel "); Serial.print(ch); Serial.println(":");
    for (byte addr = 8; addr < 120; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print("  Device at 0x");
        Serial.println(addr, HEX);
      }
    }
  }
  tcaSelect(255);
  Serial.println("[I2C] Scan complete");
}

// ─────────────────────────────────────────────────────────────
// Reinitialize a ToF sensor on its multiplexer channel
// Called when a timeout or communication failure is detected
// ─────────────────────────────────────────────────────────────
void resetSensor(uint8_t channel, VL53L0X& sensor) {
  Serial.println("[ToF] Resetting sensor...");
  sensor.stopContinuous();
  tcaSelect(channel);
  delay(50);
  sensor.init();
  sensor.setTimeout(500);
  sensor.startContinuous();
  Serial.println("[ToF] Reset complete");
}

// ─────────────────────────────────────────────────────────────
// Read distance from a ToF sensor on a given channel
// Returns true if an object is detected within 1000mm
// ─────────────────────────────────────────────────────────────
bool tof_sens(uint8_t channel, VL53L0X& sensor, const char* label) {
  tcaSelect(channel);
  uint16_t distance = sensor.readRangeContinuousMillimeters();

  Serial.print("["); Serial.print(label); Serial.print("] Distance: ");
  Serial.print(distance);
  Serial.println(" mm");

  return (distance < 1000);
}

// ─────────────────────────────────────────────────────────────
// Check whether the object on a given channel appears green
// Returns true if the color ratios indicate a plant
// ─────────────────────────────────────────────────────────────
bool is_green(uint8_t channel, Adafruit_TCS34725& sensor, const char* label) {
  tcaSelect(channel);
  uint16_t r, g, b, c;
  sensor.getRawData(&r, &g, &b, &c);

  Serial.print("["); Serial.print(label); Serial.print("] ");
  Serial.print("R="); Serial.print(r);
  Serial.print(" G="); Serial.print(g);
  Serial.print(" B="); Serial.print(b);
  Serial.print(" C="); Serial.println(c);

  if (c == 0) {
    Serial.print("["); Serial.print(label); Serial.println("] No light detected");
    return false;
  }

  float rRatio = (float)r / c * 100.0f;
  float gRatio = (float)g / c * 100.0f;
  float bRatio = (float)b / c * 100.0f;

  Serial.print("["); Serial.print(label); Serial.print("] ");
  Serial.print("rRatio="); Serial.print(rRatio);
  Serial.print(" gRatio="); Serial.print(gRatio);
  Serial.print(" bRatio="); Serial.println(bRatio);

  bool result = (gRatio > rRatio * 1.2f) &&
                (gRatio > bRatio * 1.1f) &&
                (gRatio > 20.0f);

  Serial.print("["); Serial.print(label); Serial.print("] Is green: ");
  Serial.println(result ? "YES" : "NO");

  return result;
}

void setRelay(bool shouldWater) {
  digitalWrite(RELAY_PIN, shouldWater ? RELAY_ON : RELAY_OFF);
  Serial.print("[Relay] ");
  Serial.println(shouldWater ? "ON — watering" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Init ToF1 on channel 0
  tcaSelect(CH_TOF1);
  delay(100);
  if (!l0x1.init()) { Serial.println("[ToF1] FAILED"); while (1); }
  l0x1.setTimeout(500);
  l0x1.startContinuous();
  Serial.println("[ToF1] OK");

  // Init ToF2 on channel 1
  tcaSelect(CH_TOF2);
  delay(100);
  if (!l0x2.init()) { Serial.println("[ToF2] FAILED"); while (1); }
  l0x2.setTimeout(500);
  l0x2.startContinuous();
  Serial.println("[ToF2] OK");

  // Init RGB1 on channel 2
  tcaSelect(CH_RGB1);
  if (!rgb1.begin()) { Serial.println("[RGB1] FAILED"); while (1); }
  Serial.println("[RGB1] OK");

  // Init RGB2 on channel 3
  tcaSelect(CH_RGB2);
  if (!rgb2.begin()) { Serial.println("[RGB2] FAILED"); while (1); }
  Serial.println("[RGB2] OK");

  check();
}

void loop() {
  bool detected1 = tof_sens(CH_TOF1, l0x1, "ToF1");
  bool detected2 = tof_sens(CH_TOF2, l0x2, "ToF2");

  // Read RGB only if the paired ToF detects an object
  bool green1 = detected1 ? is_green(CH_RGB1, rgb1, "RGB1") : false;
  bool green2 = detected2 ? is_green(CH_RGB2, rgb2, "RGB2") : false;

  // Water if either pair confirms a green object
  setRelay((detected1 && green1) || (detected2 && green2));
  delay(200);
}

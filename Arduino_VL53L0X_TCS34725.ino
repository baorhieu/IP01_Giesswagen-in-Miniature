#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_TCS34725.h>

// ── Pin definitions ──────────────────────────────────────────
#define SDA_PIN     8
#define SCL_PIN     9
#define RELAY_PIN   4   // GPIO4: safe on ESP32-S3
#define XSHUT_PIN1   5   // GPIO5: safe on ESP32-S3
#define XSHUT_PIN2   6   // GPIO6: safe on ESP32-S3

// ── I2C addresses ────────────────────────────────────────────
#define TOF_ADDRESS1 0x30  // Custom address 
#define TOF_ADDRESS2 0x31  // Custom address 

// ── Logic levels ─────────────────────────────────────────────
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ── Sensor objects ───────────────────────────────────────────
VL53L0X l0x1;
VL53L0X l0x2;
Adafruit_TCS34725 rgb_sensor = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X
);

// ─────────────────────────────────────────────────────────────
// Reset the ToF sensor via XSHUT pin
// Used when a timeout or communication failure is detected
// ─────────────────────────────────────────────────────────────
void resetSensor(int XSHUT_PIN, TOF_ADDRESS, tof_sensor) {
  Serial.println("[ToF] Resetting sensor...");
  tof_sensor.stopContinuous();
  digitalWrite(XSHUT_PIN, LOW);
  delay(20);
  digitalWrite(XSHUT_PIN, HIGH);
  delay(20);
  tof_sensor.init();
  tof_sensor.setAddress(TOF_ADDRESS);
  tof_sensor.setTimeout(500);
  tof_sensor.startContinuous();
  Serial.println("[ToF] Reset complete");
}

// ─────────────────────────────────────────────────────────────
// Detect an object using the ToF sensor
// Returns true if an object is detected within 1000mm
// ─────────────────────────────────────────────────────────────
bool tof_sens(tof_sensor) {
  uint16_t distance = tof_sensor.readRangeContinuousMillimeters();

  if (tof_sensor.timeoutOccurred()) {
    Serial.println("[ToF] Timeout! Resetting...");
    resetSensor();
    return false;
  }

  Serial.print("[ToF] Distance: ");
  Serial.print(distance);
  Serial.println(" mm");

  return (distance < 1000);

}

// ─────────────────────────────────────────────────────────────
// Check whether the detected object is green (likely a plant)
// Uses ratio of G channel vs R and B to determine greenness
// Returns true if the object appears green
// ─────────────────────────────────────────────────────────────
bool is_green() {
  uint16_t r, g, b, c;
  rgb_sensor.getRawData(&r, &g, &b, &c);

  Serial.print("[RGB] R="); Serial.print(r);
  Serial.print(" G=");      Serial.print(g);
  Serial.print(" B=");      Serial.print(b);
  Serial.print(" C=");      Serial.println(c);

  // No light detected — cannot determine color
  if (c == 0) {
    Serial.println("[RGB] No light detected");
    return false;
  }

  // Calculate each channel as a percentage of total light (clear channel)
  float rRatio = (float)r / c * 100.0f;
  float gRatio = (float)g / c * 100.0f;
  float bRatio = (float)b / c * 100.0f;

  Serial.print("[RGB] rRatio="); Serial.print(rRatio);
  Serial.print(" gRatio=");      Serial.print(gRatio);
  Serial.print(" bRatio=");      Serial.println(bRatio);

  // Green detection thresholds:
  // - Green must be 20% stronger than red
  // - Green must be 10% stronger than blue
  // - Green ratio must exceed 20% of total light
  bool isGreen = (gRatio > rRatio * 1.2f) &&
                 (gRatio > bRatio * 1.1f) &&
                 (gRatio > 20.0f);

  Serial.print("[RGB] Is green: ");
  Serial.println(isGreen ? "YES" : "NO");

  return isGreen;
}

// ─────────────────────────────────────────────────────────────
// Scan the I2C bus and print all found device addresses
// Useful for debugging sensor connections
// ─────────────────────────────────────────────────────────────
void check() {
  Serial.println("[I2C] Scanning bus...");
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("[I2C] Device found at 0x");
      Serial.println(i, HEX);
    }
  }
  Serial.println("[I2C] Scan complete");
}

// ─────────────────────────────────────────────────────────────
// Reassign the ToF sensor I2C address from default (0x29)
// to a custom address (TOF_ADDRESS) using the XSHUT pin
// ─────────────────────────────────────────────────────────────
void address_change(int XSHUT_PIN, float ADDRESS, tof_sensor) {
  digitalWrite(XSHUT_PIN, HIGH);
  delay(50); // Wait for sensor to boot before initializing
  if (!tof_sensor.init()) {
    Serial.println("[ToF] Init failed — check wiring.");
    resetSensor(XSHUT_PIN, ADDRESS, tof_sensor);
  }
  tof_sensor.init();
  tof_sensor.setAddress(ADDRESS);
  tof_sensor.setTimeout(500);
  Serial.println("[ToF] I2C address set to " + ADDRESS);
  
  tof_sensor.startContinuous();
  Serial.println("[ToF] Ready");
}

void setRelay(bool detected, bool green) {
  bool shouldWater = detected && green;
  digitalWrite(RELAY_PIN, shouldWater ? RELAY_ON : RELAY_OFF);
  Serial.print("[Relay] ");
  Serial.println(shouldWater ? "ON — watering" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== System Booting ===");

  // Initialize I2C with explicit SDA/SCL pins for ESP32-S3
  Wire.begin(SDA_PIN, SCL_PIN);

  // Configure output pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(XSHUT_PIN1, OUTPUT);
  pinMode(XSHUT_PIN2, OUTPUT);  

  // Ensure relay is off at startup
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Initialize ToF sensor with custom I2C address
  digitalWrite(XSHUT_PIN1, LOW);
  digitalWrite(XSHUT_PIN2, LOW);
  delay(20);

  // Change ToF address
  address_change(XSHUT_PIN1, TOF_ADDRESS1, l0x1);
  address_change(XSHUT_PIN2, TOF_ADDRESS2, l0x2); 

  // Initialize RGB color sensor
 
  if (!rgb_sensor.begin()) {
    Serial.println("[RGB] Init failed — check wiring.");
    while (1);
  }
  Serial.println("[RGB] Ready");
  

  // Run I2C scan once at startup for debugging
  check();

  Serial.println("=== System Ready ===");
}

void loop() {  
  bool detected_1 = tof_sens(l0x1);
  bool detected_2 = tof_sens(l0x2);

  
  // Only read RGB sensor if an object is detected — saves processing time
  bool green = detected_1 ? is_green() : false;

  setRelay(detected_1, green);
  delay(200);
}
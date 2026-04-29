#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_TCS34725.h>

VL53L0X tof_sensor;
Adafruit_TCS34725 rgb_sensor;

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define LIGHT_ON HIGH
#define LIGHT_OFF LOW

#define RELAY_PIN 7
#define XSHUT_PIN 3

#define SENSOR_ADDRESS 0x30


// Return true if the object is detected
bool tof_sens() {
  uint16_t distance = tof_sensor.readRangeContinuousMillimeters();  
  if (distance < 1000) {
    Serial.println("Detected");
    return true;
  } else {
    Serial.println("Nothing");
    return false;
  }
}

// Return true if the object is green
bool is_green(){
  uint16_t r, g, b, c;
  rgb_sensor.getRawData(&r, &g, &b, &c);

  Serial.print("R="); Serial.print(r);
  Serial.print(" G="); Serial.print(g);
  Serial.print(" B="); Serial.println(b);

  if (c == 0) {
    Serial.println("No light detected");
    delay(500);
  return;
  }

  // Calculate % of each received color wavelength
  float rRatio = (float)r / c * 100;
  float gRatio = (float)g / c * 100;
  float bRatio = (float)b / c * 100;

  // Green detection condition
  // Must be more than 20%
  bool isGreen = (gRatio > rRatio * 1.2f) && 
                (gRatio > bRatio * 1.1f) && 
                (gRatio > 20.0f);
  Serial.println(isGreen);

  return isGreen;

delay(500);
}

// Reset ToF if is not working
void resetSensor() {
  tof_sensor.stopContinuous();
  digitalWrite(XSHUT_PIN, LOW);
  delay(10);
  digitalWrite(XSHUT_PIN, HIGH);
  delay(10);
  tof_sensor.init();
  tof_sensor.setAddress(SENSOR_ADDRESS);
  tof_sensor.setTimeout(500);
  tof_sensor.startContinuous();
}

// Turn on the relay if the object is detected and green
void relays(bool state, bool green) {
  digitalWrite(RELAY_PIN, state && green ? LIGHT_ON : LIGHT_OFF);
}


// Just checking address of sensors
void check(){
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(i, HEX);
    }
  }
}

// Change address of the ToF
void address_change(){
  digitalWrite(XSHUT_PIN, HIGH);
  delay(50);
  tof_sensor.init();
  tof_sensor.setAddress(0x30);
  tof_sensor.setTimeout(500);
}

// Check if RGB sensor working
void rgb_run(){  
  if (rgb_sensor.begin()) {
    Serial.println("RGB OK");
  } else {
    Serial.println("No RGB");
    while(1);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Assign OUTPUT ports
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(XSHUT_PIN, OUTPUT);

  // Turn off the relay
  relays(false, false);

  // Shut the ToF down
  digitalWrite(XSHUT_PIN, LOW);
  delay(10);

  address_change();
  rgb_run();
  
  // Check ToF
  if (!tof_sensor.init()) {
    Serial.println("No ToF");
    resetSensor();
  }
  tof_sensor.startContinuous();
  Serial.println("ToF OK");
}

void loop() {  
  check();
  relays(tof_sens(),is_green());
  delay(100);
}
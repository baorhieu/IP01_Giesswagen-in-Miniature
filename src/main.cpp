// ─────────────────────────────────────────────────────────────
// Gießwagen Sensors Board — main.cpp
// Converted from Arduino_VL53L0X_TCS34725.ino for PlatformIO
// ─────────────────────────────────────────────────────────────
#include <Arduino.h>              // ← Required in PlatformIO (auto-included in Arduino IDE)
#include "config.h"               // ← Pin definitions and constants

#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_TCS34725.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ── WebServer + WebSocket ─────────────────────────────────────
WebServer        server(80);
WebSocketsServer webSocket(81);

// ── Sensor objects ────────────────────────────────────────────
VL53L0X          tof1, tof2;
Adafruit_TCS34725 rgb1(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 rgb2(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// ── HTML page ─────────────────────────────────────────────────
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-S3 Sensor Terminal</title>
  <style>
    body { background:#000; color:#0f0; font-family:Menlo,monospace;
           margin:0; padding:10px; font-size:13px; }
    #log { white-space:pre-wrap; word-break:break-all; padding-bottom:60px; }
    #input { position:fixed; bottom:0; left:0; right:0;
             background:#111; color:#0f0; border:1px solid #0f0;
             padding:12px; font-family:inherit; font-size:14px;
             box-sizing:border-box; }
    #status { color:#888; font-size:12px; margin-bottom:8px; }
  </style>
</head>
<body>
  <div id="status">Connecting...</div>
  <div id="log"></div>
  <input id="input" placeholder="Enter command..." autocomplete="off" autocapitalize="off">
  <script>
    const log    = document.getElementById('log');
    const input  = document.getElementById('input');
    const status = document.getElementById('status');
    const MAX_LINES = 300;
    let ws;
    function connect() {
      ws = new WebSocket('ws://' + location.hostname + ':81/');
      ws.onopen    = () => status.textContent = 'Connected to ESP32-S3';
      ws.onmessage = (e) => {
        log.textContent += e.data;
        const lines = log.textContent.split('\n');
        if (lines.length > MAX_LINES) log.textContent = lines.slice(-MAX_LINES).join('\n');
        window.scrollTo(0, document.body.scrollHeight);
      };
      ws.onclose = () => {
        status.textContent = 'Disconnected — retrying...';
        setTimeout(connect, 1500);
      };
    }
    connect();
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && input.value.trim()) {
        ws.send(input.value.trim());
        input.value = '';
      }
    });
  </script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────
// Send a line to Serial and all WebSocket clients
// ─────────────────────────────────────────────────────────────
void wsPrintln(const String& msg) {
  Serial.println(msg);
  webSocket.broadcastTXT(msg + "\n");
}

// ─────────────────────────────────────────────────────────────
// WebSocket: handle commands sent from browser
// ─────────────────────────────────────────────────────────────
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload).substring(0, length);
    wsPrintln("[CMD] " + msg);
  }
}

// ─────────────────────────────────────────────────────────────
// Scan I2C bus and print found device addresses
// ─────────────────────────────────────────────────────────────
void scanBus(TwoWire& bus, const char* name) {
  wsPrintln(String("[I2C] ") + name);
  bool found = false;
  for (byte a = 8; a < 120; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      wsPrintln("  0x" + String(a, HEX));
      found = true;
    }
  }
  if (!found) wsPrintln("  (no devices)");
}

// ─────────────────────────────────────────────────────────────
// Reset a ToF sensor via XSHUT pin
// ─────────────────────────────────────────────────────────────
void resetSensor(int xshut, uint8_t addr, VL53L0X& sensor) {
  wsPrintln("[ToF] Resetting...");
  sensor.stopContinuous();
  digitalWrite(xshut, LOW);  delay(20);
  digitalWrite(xshut, HIGH); delay(20);
  sensor.init();
  sensor.setAddress(addr);
  sensor.setTimeout(500);
  sensor.startContinuous();
  wsPrintln("[ToF] Reset done");
}

// ─────────────────────────────────────────────────────────────
// Read ToF distance — returns true if object detected < 500mm
// ─────────────────────────────────────────────────────────────
bool tof_sens(VL53L0X& sensor, const char* label) {
  uint16_t dist = sensor.readRangeContinuousMillimeters();
  wsPrintln(String("[") + label + "] " + dist + " mm");
  return dist < DETECT_DISTANCE_MM;
}

// ─────────────────────────────────────────────────────────────
// Read RGB sensor and return true if object is green
// Wire1 pins are switched before each read (pin-switching method)
// ─────────────────────────────────────────────────────────────
bool is_green(Adafruit_TCS34725& sensor, int sda, int scl, const char* label) {
  Wire1.end();
  Wire1.begin(sda, scl);
  sensor.begin(TCS34725_ADDRESS, &Wire1);

  uint16_t r, g, b, c;
  sensor.getRawData(&r, &g, &b, &c);

  wsPrintln(String("[") + label + "] R=" + r + " G=" + g + " B=" + b + " C=" + c);

  if (c == 0) {
    wsPrintln(String("[") + label + "] No light");
    return false;
  }

  bool result = (r > 100) && (b > 70) && (g > 90);
  wsPrintln(String("[") + label + "] Green: " + (result ? "YES" : "NO"));
  return result;
}

// ─────────────────────────────────────────────────────────────
// Setup — runs once on boot
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // ── WiFi Access Point ─────────────────────────────────────
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("=== Access Point ready ===");
  Serial.print("SSID: ");     Serial.println(AP_SSID);
  Serial.print("Password: "); Serial.println(AP_PASS);
  Serial.print("URL:  http://"); Serial.println(ip);
  Serial.println("==========================");

  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // ── GPIO setup ────────────────────────────────────────────
  pinMode(LED_PIN1,   OUTPUT); digitalWrite(LED_PIN1,   LOW);
  pinMode(LED_PIN2,   OUTPUT); digitalWrite(LED_PIN2,   LOW);
  pinMode(XSHUT_PIN1, OUTPUT); digitalWrite(XSHUT_PIN1, LOW);
  pinMode(XSHUT_PIN2, OUTPUT); digitalWrite(XSHUT_PIN2, LOW);
  delay(50);

  // ── I2C Bus 0 (Wire) — ToF sensors ───────────────────────
  Wire.begin(TOF_SDA, TOF_SCL);

  // ── ToF1 — wake up, init, assign address 0x30 ────────────
  tof1.setBus(&Wire);
  digitalWrite(XSHUT_PIN1, HIGH); delay(100);
  if (!tof1.init()) { wsPrintln("[ToF1] FAILED"); while (1); }
  tof1.setAddress(TOF_ADDR1);
  tof1.setTimeout(500);
  tof1.startContinuous();
  wsPrintln("[ToF1] OK at 0x30");

  // ── ToF2 — wake up, init, assign address 0x31 ────────────
  tof2.setBus(&Wire);
  digitalWrite(XSHUT_PIN2, HIGH); delay(100);
  if (!tof2.init()) { wsPrintln("[ToF2] FAILED"); while (1); }
  tof2.setAddress(TOF_ADDR2);
  tof2.setTimeout(500);
  tof2.startContinuous();
  wsPrintln("[ToF2] OK at 0x31");

  // ── RGB1 — init on Wire1 with RGB1 pins ──────────────────
  Wire1.begin(RGB1_SDA, RGB1_SCL);
  if (!rgb1.begin(TCS34725_ADDRESS, &Wire1)) {
    wsPrintln("[RGB1] FAILED"); while (1);
  }
  wsPrintln("[RGB1] OK");

  // ── RGB2 — switch Wire1 to RGB2 pins and init ────────────
  Wire1.begin(RGB2_SDA, RGB2_SCL);
  if (!rgb2.begin(TCS34725_ADDRESS, &Wire1)) {
    wsPrintln("[RGB2] FAILED"); while (1);
  }
  wsPrintln("[RGB2] OK");

  // ── Final I2C scan for debugging ─────────────────────────
  wsPrintln("[Boot] Final I2C scan:");
  scanBus(Wire, "Wire (SDA:8, SCL:9)");
  Wire1.begin(RGB1_SDA, RGB1_SCL); scanBus(Wire1, "Wire1 RGB1 (SDA:35, SCL:36)");
  Wire1.begin(RGB2_SDA, RGB2_SCL); scanBus(Wire1, "Wire1 RGB2 (SDA:12, SCL:13)");

  wsPrintln("[Boot] Ready");
}

// ─────────────────────────────────────────────────────────────
// Main loop
// ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  webSocket.loop();

  // Read sensors every 1000ms — no blocking delay()
  static unsigned long lastRead = 0;
  if (millis() - lastRead < SENSOR_READ_INTERVAL_MS) return;
  lastRead = millis();

  // Read ToF distance sensors
  bool det1 = tof_sens(tof1, "ToF1");
  bool det2 = tof_sens(tof2, "ToF2");

  // Read RGB color sensors — only if object detected by ToF
  bool green1 = false;
  bool green2 = false;

  if (det1) {
    green1 = is_green(rgb1, RGB1_SDA, RGB1_SCL, "RGB1");
  } else {
    wsPrintln("[RGB1] Skipped");
  }

  if (det2) {
    green2 = is_green(rgb2, RGB2_SDA, RGB2_SCL, "RGB2");
  } else {
    wsPrintln("[RGB2] Skipped");
  }

  // LED ON only if object detected AND confirmed green
  bool led1 = det1 && green1;
  bool led2 = det2 && green2;

  digitalWrite(LED_PIN1, led1 ? HIGH : LOW);
  digitalWrite(LED_PIN2, led2 ? HIGH : LOW);

  wsPrintln(String("[LED1] ") + (led1 ? "ON" : "OFF") +
            "  [LED2] "       + (led2 ? "ON" : "OFF"));
  wsPrintln("─────────────────────────────────────────────────────────────");
}
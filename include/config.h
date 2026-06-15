#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi Access Point ─────────────────────────────────────────
#define AP_SSID "ESP32-Sensor"
#define AP_PASS "12345678"

// ── I2C Bus 0 (Wire) — ToF sensors ───────────────────────────
#define TOF_SDA    8
#define TOF_SCL    9
#define XSHUT_PIN1 5
#define XSHUT_PIN2 6
#define TOF_ADDR1  0x30
#define TOF_ADDR2  0x31

// ── I2C Bus 1 (Wire1) — RGB sensors (pin switching) ──────────
#define RGB1_SDA   35
#define RGB1_SCL   36
#define RGB2_SDA   12
#define RGB2_SCL   13

// ── LED output pins ───────────────────────────────────────────
#define LED_PIN1   40
#define LED_PIN2   41

// ── Detection threshold ───────────────────────────────────────
#define DETECT_DISTANCE_MM 500
#define SENSOR_READ_INTERVAL_MS 1000

#endif // CONFIG_H
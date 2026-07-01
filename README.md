# 🌱 IP01 – Gießwagen in Miniature

**ESP32-S3 Plant Detection & Watering Cart — Hochschule Rhein-Waal**

A miniature "watering cart" (Gießwagen) prototype that autonomously detects plants using dual **Time-of-Flight (ToF)** distance sensors and **RGB color** sensors, then signals detection via onboard LEDs. Live sensor data streams to any browser over WiFi — no router or app install required.

Built with **PlatformIO** on the ESP32-S3.

---

## Table of Contents

- [📖 Overview](#overview)
- [✨ Features](#features)
- [🔧 Hardware](#hardware)
- [⚙️ How It Works](#how-it-works)
- [🚀 Getting Started](#getting-started)
- [🖥️ WiFi Debug Terminal](#wifi-debug-terminal)
- [📁 Project Structure](#project-structure)
- [👥 Team](#team)
- [📄 License](#license)

---

## Overview

This project combines distance sensing and color recognition to identify plants (green objects) within range of a moving cart. It was built as part of the **IP01** module at Hochschule Rhein-Waal, using an **ESP32-S3** as the central controller for two independent sensor pairs.

Each sensor pair works together:
1. A **ToF sensor** checks if *any* object is within 500 mm.
2. If so, a **color sensor** confirms whether that object is green.
3. If both conditions are true, the paired **LED** lights up.

All readings are also broadcast live to a browser-based terminal for debugging and demonstration.

---

## Features

| Feature | Description |
|---|---|
| 🎯 **2× VL53L0X** | Time-of-Flight distance sensors, 500 mm detection range |
| 🎨 **2× TCS34725** | RGB color sensors for green-object confirmation |
| 📡 **WiFi Access Point** | ESP32-S3 hosts its own network — connect directly, no router needed |
| 🖥️ **WebSocket Terminal** | Live sensor log in-browser at `http://192.168.4.1` |
| 💡 **2× Status LEDs** | Instant visual feedback per sensor pair |
| ⚡ **~300 ms refresh** | Near real-time sensor updates |

---

## Hardware

### Wiring

| Component | Pin | ESP32-S3 GPIO |
|---|---|---|
| ToF1 + ToF2 | SDA | 8 |
| ToF1 + ToF2 | SCL | 9 |
| ToF1 | XSHUT | 5 |
| ToF2 | XSHUT | 6 |
| RGB1 | SDA | 35 |
| RGB1 | SCL | 36 |
| RGB2 | SDA | 12 |
| RGB2 | SCL | 13 |
| LED 1 | — | 40 |
| LED 2 | — | 41 |

> Pin assignments are defined in [`include/config.h`](include/config.h).

### I²C Bus Layout

```
Wire  (I2C0, GPIO 8/9)   : ToF1 (0x30) + ToF2 (0x31)
Wire1 (I2C1, GPIO 35/36) : RGB1 (0x29) ┐ address is fixed,
Wire1 (I2C1, GPIO 12/13) : RGB2 (0x29) ┘ so pins are switched before each read
```

> **Why two I²C buses?** The VL53L0X sensors share a bus (I2C0) and use their `XSHUT` pins at boot to get unique addresses (`0x30` / `0x31`). The TCS34725 sensors have a fixed, non-configurable address (`0x29`), so instead they're each wired to a dedicated SDA/SCL pair on I2C1, and the firmware switches between them at runtime.

### Dependencies

Libraries are managed automatically by PlatformIO via `lib_deps` in [`platformio.ini`](platformio.ini):

| Library | Author |
|---|---|
| `VL53L0X` | Pololu |
| `Adafruit TCS34725` | Adafruit |
| `Adafruit BusIO` | Adafruit |
| `WebSockets` | Markus Sattler |

---

## How It Works

```
ToF1 detects object  AND  RGB1 confirms green  →  LED1 ON
ToF2 detects object  AND  RGB2 confirms green  →  LED2 ON
```

Each sensor pair operates independently, allowing the cart to monitor two zones (e.g. left/right side) simultaneously. Detection results and raw sensor values are pushed to the WiFi terminal roughly every 300 ms.

---

## Getting Started

This is a **PlatformIO** project. You'll need [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) installed.

1. Clone this repository:
   ```bash
   git clone https://github.com/baorhieu/IP01_Giesswagen-in-Miniature.git
   cd IP01_Giesswagen-in-Miniature
   ```
2. Open the folder in **VS Code** — PlatformIO will auto-detect the project and install the dependencies listed in `platformio.ini`.
3. Connect the ESP32-S3 via USB.
4. Build and upload:
   ```bash
   pio run --target upload
   ```
   (or use the PlatformIO **Upload** button in the status bar).
5. Open the serial monitor to confirm the Access Point's IP address:
   ```bash
   pio device monitor
   ```

---

## WiFi Debug Terminal

1. Power on the ESP32-S3.
2. Connect to the WiFi network:
   - **SSID:** `ESP32-Sensor`
   - **Password:** `12345678`
3. Open a browser and navigate to **`http://192.168.4.1`**.
4. Watch live sensor readings and detection events stream in.

![WiFi terminal screenshot](docs/terminal.png)

---

## Project Structure

```
IP01_Giesswagen-in-Miniature/
├── .vscode/
│   └── extensions.json        # Recommended VS Code extensions
├── docs/
│   └── terminal.png           # WiFi terminal screenshot
├── include/
│   ├── README
│   └── config.h               # Pin definitions & configuration
├── lib/
│   └── README                 # Local/private libraries (optional)
├── src/
│   └── main.cpp               # Main firmware source
├── test/
│   └── README                 # Unit tests (optional)
├── .gitignore
├── platformio.ini             # PlatformIO project configuration
└── README.md
```

---

## Team

Project developed for the **IP01** module at **Hochschule Rhein-Waal**.

- Truong, Lam Bao Hieu
- Abu HURAYRA
- SAHIDUL-ISLAM SHANTO

---

## License

This project is provided for academic purposes as part of coursework at Hochschule Rhein-Waal. Feel free to fork and adapt for educational use.

# M5Sharkino — Open Source Flipper Zero Alternative | ESP32-S3 Multi-Tool

**M5Sharkino** is a powerful open-source wireless research and multi-tool device built on the **ESP32-S3 N16R8**, inspired by the Flipper Zero architecture. It combines Sub-GHz RF, RFID, BLE, Wi-Fi, IR, and sensor analysis into a single handheld device with a joystick-navigated OLED menu system.

> This is the **Master Device** firmware. For the companion slave co-processor (4x nRF24L01 controller), see [superminiM5SharkinoCC1101mSD](../superminiM5SharkinoCC1101mSD).

---

## Key Features

### Sub-GHz RF (CC1101 — 433MHz)
- Raw RF signal **scan, capture, and replay**
- **Flipper Zero `.sub` file** read & replay support
- **Rolling Code analysis** (garage doors, car remotes)
- Modulation support: **ASK, OOK, FSK, GFSK**
- **Smart Meter** packet reading
- **POCSAG Pager** message decode
- Long-range LoRa-like mode

### RFID (MFRC522)
- **Mifare Classic 1K/4K** read, write, clone
- **UID spoof** (writable UID card support)
- **Flipper Zero `.nfc` file** import/export
- **Flipper Zero `.rfid` iButton file** support
- Raw block dump to SD card

### Wi-Fi & BLE
- BLE device passive scan with RSSI
- **BLE advertisement** type classification
- Wi-Fi AP scanner with security type detection

### Sensor Modules (Hot-Swap Expansion Port)
- **DHT11 / DHT22** — Temperature & Humidity (library-free raw protocol)
- **DS18B20** — 1-Wire temperature sensor
- **BMP280 / BME280** — Barometric pressure, temperature, altitude
- **APDS9960** — Gesture, proximity, RGB light sensor
- **MPU6050** — Accelerometer & gyroscope (6-axis IMU)
- **HC-SR04** — Ultrasonic distance measurement
- **MQ-2/MQ-135** — Gas / air quality sensor

### UI & Navigation
- **Flipper Zero-inspired menu architecture** (MenuItem, loopOptions, OptionList)
- **128x64 SSD1306 OLED** display @ I2C 400kHz
- **Analog joystick** (X/Y axis + button) navigation
- **Active buzzer** — boot, menu, success, error, RF/RFID audio feedback
- **Potentiometer** — analog parameter adjustment

### Storage
- **Micro SD card** — RF captures, PCAP logs, Flipper file library
- **SPIFFS** — Internal flash RF capture storage

### Slave Co-Processor Link
- **UART bridge** to ESP32-S3 SuperMini slave (TX→GPIO43, RX→GPIO44)
- Commands: `JAM`, `SCAN`, `SPEC`, `MITM_P`, `MITM_A`, `PING`, `STOP`, `RESET`
- Real-time spectrum data streamed to OLED

---

## Hardware & Pin Configuration

### Master Device Components

| Component | Description |
|:---|:---|
| **ESP32-S3 N16R8** | Main MCU (16MB Flash, 8MB Octal PSRAM, 240MHz) |
| **SSD1306 128x64** | I2C OLED Display |
| **CC1101** | 433MHz Sub-GHz RF transceiver (SPI) |
| **MFRC522** | RFID reader/writer (SPI) |
| **Analog Joystick** | X/Y navigation + push button |
| **Active Buzzer** | Audio feedback |
| **Potentiometer** | Analog parameter input |
| **Micro SD Module** | SPI storage |
| **Expansion Port** | 8-pin hot-swap sensor module connector |

### Master Pin Table

| Function | GPIO | Notes |
|:---|:---|:---|
| OLED SDA | GPIO 8 | I2C |
| OLED SCL | GPIO 9 | I2C 400kHz |
| CC1101 CS | GPIO 5 | SPI |
| CC1101 GDO0 | GPIO 6 | Interrupt |
| MFRC522 CS | GPIO 10 | SPI |
| MFRC522 RST | GPIO 11 | Reset |
| SD CS | GPIO 14 | SPI |
| SPI SCK | GPIO 12 | Shared SPI |
| SPI MOSI | GPIO 13 | Shared SPI |
| SPI MISO | GPIO 15 | Shared SPI |
| Joystick X | GPIO 3 | ADC |
| Joystick Y | GPIO 4 | ADC |
| Joystick BTN | GPIO 7 | INPUT_PULLUP |
| Buzzer | GPIO 7 | Active buzzer |
| Potentiometer | GPIO 1 | ADC |
| UART TX (Slave) | GPIO 43 | To SuperMini RX |
| UART RX (Slave) | GPIO 44 | From SuperMini TX |
| Expansion DATA | GPIO 40 | Sensor modules |
| Expansion SDA2 | GPIO 41 | Sensor I2C |
| Expansion SCL2 | GPIO 42 | Sensor I2C |

---

## Required Libraries

Install via Arduino IDE Library Manager (`Ctrl+Shift+I`):

- `Adafruit SSD1306` by Adafruit
- `Adafruit GFX Library` by Adafruit
- `MFRC522` by GithubCommunity
- `ESP32 Board Package` (includes WiFi, BLEDevice, SPIFFS, Preferences)

---

## How to Flash (Master)

1. Open `M5SharkinoCC1101mSD.ino` in **Arduino IDE 2.x**
2. Select board settings:
   - **Board:** `ESP32S3 Dev Module`
   - **Flash Size:** `16MB`
   - **PSRAM:** `OPI PSRAM`
   - **Partition Scheme:** `16M Flash (3MB APP/9.9MB FATFS)`
   - **CPU Frequency:** `240MHz`
   - **USB CDC On Boot:** `Enabled`
3. Select your active COM port
4. Click **Upload** (`Ctrl+U`)

---

## Slave Co-Processor

The **ESP32-S3 SuperMini** acts as a dedicated RF co-processor managing 4x nRF24L01 modules simultaneously. It communicates with the master via UART and handles:

- 2.4GHz band sweep jamming (5 modes)
- Drone signal scanning (DJI focus)
- Spectrum analysis (channels 0-125)
- Passive & Active MITM
- MouseJack wireless keyboard/mouse exploit testing

**Slave firmware:** [superminiM5SharkinoCC1101mSD](../superminiM5SharkinoCC1101mSD)

---

## Legal & Ethical Disclaimer

This project is designed exclusively for **educational research, authorized penetration testing, and RF spectrum analysis** in controlled environments where you have explicit permission.

- Always comply with local RF transmission laws and regulations.
- Never use RF replay or jamming capabilities on unauthorized systems.
- The author assumes no responsibility for any misuse.

---

## 📄 License

MIT License — See [LICENSE](LICENSE) for details.

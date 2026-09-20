# M5Sharkino — Open Source Flipper Zero Alternative with ESP32-S3

**M5Sharkino** is a dual-core, multi-processor open-source wireless research and security tool built using **ESP32-S3**. Inspired by the Flipper Zero architecture, it integrates 433MHz Sub-GHz RF transceiver capabilities, RFID reading/cloning, BLE scanning, Wi-Fi analysis, IR controls, and custom expansion sensor modules into a single handheld ecosystem with an OLED interface and joystick navigation.

---

## Hardware Architecture & Project Structure

The project consists of two interconnected ESP32-S3 boards communicating via high-speed UART:

```
                  +-----------------------------------+
                  |   ESP32-S3 N16R8 (Master Unit)    |
                  |  OLED / Joystick / SD / CC1101    |
                  |  RFID MFRC522 / Sensor Expansion  |
                  +-----------------+-----------------+
                                    |
                               UART Bridge
                             (TX:43 / RX:44)
                                    |
                  +-----------------+-----------------+
                  |   ESP32-S3 SuperMini (Slave Unit) |
                  |   4x nRF24L01+PA+LNA Transceivers |
                  +-----------------------------------+
```

### Repositories & Modules in this Project:

- **Master Unit Firmware:** Controls the main OLED menu, joystick, CC1101 Sub-GHz, MFRC522 RFID, SD Card, and sensor expansion port.
- **Slave Unit Firmware:** Manages 4x nRF24L01+ modules for spectrum analysis, drone signal scanning, packet injection, and RF jamming modes.

---

## Core Capabilities

### Sub-GHz RF (CC1101 @ 433MHz)
- Raw RF signal **scan, capture, and replay**
- **Flipper Zero `.sub` file** import, parsing, and replay support
- **Rolling Code analysis** for security auditing (garage doors, car remotes)
- Modulation support: **ASK, OOK, FSK, GFSK**
- **Smart Meter** packet reading and **POCSAG Pager** message decoding

### RFID (MFRC522)
- **Mifare Classic 1K/4K** read, write, and clone
- **UID spoofing** (writable UID card support)
- **Flipper Zero `.nfc` and `.rfid` (iButton)** file compatibility
- Raw block dump directly to Micro SD card

### 2.4GHz RF Co-Processing (4x nRF24L01+)
- **Spectrum Analyzer:** Real-time 2.4GHz RSSI heat-map streamed to Master OLED
- **Drone Scanner:** Detection of commercial 2.4GHz drone control links
- **Targeted Jamming Modes:** Band Sweep, BLE focus, Wi-Fi focus, Drone focus, Rolling Chaos
- **HID Exploitation:** MouseJack vulnerability testing and key injection

### Multi-Sensor Support (Hot-Swap Port)
- **DHT11 / DHT22** — Temperature & Humidity
- **DS18B20** — 1-Wire temperature probing
- **BMP280 / BME280** — Atmospheric pressure & altitude
- **APDS9960** — Gesture and RGB light sensing
- **MPU6050** — 6-Axis IMU (Accelerometer & Gyroscope)
- **HC-SR04** — Ultrasonic distance sensing
- **MQ-2 / MQ-135** — Air quality & gas detection

---

## Hardware Pinout Quick Reference

### Master Unit (ESP32-S3 N16R8)

| Component | Pin / GPIO | Notes |
|:---|:---|:---|
| **OLED Display** | SDA: GPIO 8, SCL: GPIO 9 | I2C @ 400kHz |
| **CC1101 RF** | CS: GPIO 5, GDO0: GPIO 6 | SPI |
| **MFRC522 RFID** | CS: GPIO 10, RST: GPIO 11 | SPI |
| **SD Card Module** | CS: GPIO 14 | SPI |
| **Shared SPI** | SCK: GPIO 12, MOSI: GPIO 13, MISO: GPIO 15 | Master SPI Bus |
| **Analog Joystick** | X: GPIO 3, Y: GPIO 4, BTN: GPIO 7 | ADC + Input Pullup |
| **Buzzer** | GPIO 7 | Active Audio Feedback |
| **UART Link** | TX: GPIO 43, RX: GPIO 44 | Connects to Slave |

### Slave Unit (ESP32-S3 SuperMini)

| Component | Pin / GPIO | Notes |
|:---|:---|:---|
| **Shared SPI Bus** | SCK: GPIO 4, MOSI: GPIO 5, MISO: GPIO 6 | Shared 4x nRF24 Bus |
| **nRF24 #1** | CSN: GPIO 7, CE: GPIO 8 | Channels 0 - 31 |
| **nRF24 #2** | CSN: GPIO 9, CE: GPIO 10 | Channels 32 - 63 |
| **nRF24 #3** | CSN: GPIO 11, CE: GPIO 12 | Channels 64 - 94 |
| **nRF24 #4** | CSN: GPIO 13, CE: GPIO 1 | Channels 95 - 125 |
| **UART Link** | TX: GPIO 21, RX: GPIO 20 | Connects to Master |

---

## Flashing & Setup

1. Open `M5SharkinoCC1101mSD.ino` in **Arduino IDE 2.x**.
2. Install required libraries: `Adafruit SSD1306`, `Adafruit GFX`, `MFRC522`, `RF24`.
3. Board Settings for Master Unit:
   - Board: `ESP32S3 Dev Module`
   - Flash Size: `16MB`
   - PSRAM: `OPI PSRAM`
   - Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`
   - USB CDC On Boot: `Enabled`
4. Upload Master firmware to ESP32-S3 DevKitC.
5. Upload Slave firmware to ESP32-S3 SuperMini.

---

## Legal & Ethical Disclaimer

This project is created strictly for **educational research, scientific analysis, and authorized security auditing** in controlled environments with explicit consent. Always adhere to local telecommunication laws and regulations. The author assumes no liability for misuse.

---

## License

MIT License — See [LICENSE](LICENSE) for details.

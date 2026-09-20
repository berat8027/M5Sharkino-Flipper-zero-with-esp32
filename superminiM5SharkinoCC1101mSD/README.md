# ESP32-S3 SuperMini — 4x nRF24L01+ RF Co-Processor (M5Sharkino Slave)

Dedicated RF slave co-processor firmware for **M5Sharkino**, built on **ESP32-S3 SuperMini** managing **4x nRF24L01+PA+LNA** modules concurrently over SPI. Communicates with the Master device (ESP32-S3 DevKitC) via high-speed UART.

---

## Features & Modes

- **Multi-RF Architecture:** Drives 4x nRF24L01+ modules simultaneously across 2.4GHz channels (0-125).
- **UART Command Protocol:** Inter-chip communication with Master at 115200 baud (`JAM`, `SCAN`, `SPEC`, `MITM_P`, `MITM_A`, `KEYS`, `REACT`).
- **Targeted Jamming Modes:**
  - `JAM:1` — Band Sweep (Full 2.4GHz spectrum)
  - `JAM:2` — BLE Focused (Advertising channels 37, 38, 39)
  - `JAM:3` — Wi-Fi Focused (Channels 1, 6, 11, 13)
  - `JAM:4` — Drone Control Frequency Sweep (DJI/Syma focus)
  - `JAM:5` — Rolling Chaos (Randomized hopping)
- **Drone Scanner:** Detection and signal analysis for commercial 2.4GHz drone transmitters.
- **Spectrum Analyzer:** High-speed channel RSSI/carrier scanning streamed to Master display.
- **MITM & MouseJack Engine:** Passive/Active packet relay and wireless HID key injection.
- **Reactive Mode:** Heat-map based dwell adjustments (focuses output on active channels).

---

## Pin Configuration (ESP32-S3 SuperMini)

| Hardware | Pin | Function |
|:---|:---|:---|
| **SPI SCK** | GPIO 4 | Shared SPI Clock |
| **SPI MOSI** | GPIO 5 | Shared SPI Master Out Slave In |
| **SPI MISO** | GPIO 6 | Shared SPI Master In Slave Out |
| **nRF24 #1 CSN / CE** | GPIO 7 / GPIO 8 | Channels 0 - 31 |
| **nRF24 #2 CSN / CE** | GPIO 9 / GPIO 10 | Channels 32 - 63 |
| **nRF24 #3 CSN / CE** | GPIO 11 / GPIO 12 | Channels 64 - 94 |
| **nRF24 #4 CSN / CE** | GPIO 13 / GPIO 1 | Channels 95 - 125 |
| **UART TX** | GPIO 21 | Connects to Master RX (GPIO 44) |
| **UART RX** | GPIO 20 | Connects to Master TX (GPIO 43) |

---

## Required Libraries

- `RF24` by TMRh20 (v1.4.x)
- `Adafruit NeoPixel`

---

## License

MIT License — See [LICENSE](LICENSE) for details.

#pragma once
// ═══════════════════════════════════════════════════════════
//  pin_config.h — ESP32-S3 SuperMini Pin Tanımları
//
//  ESP32-S3 SuperMini güvenli pinler:
//  1, 2, 4, 5, 6, 7, 8, 15, 16, 17, 18, 21
//
//  SPI paylaşımlı (4x nRF24 ortak):
//    SCK  → GPIO 4
//    MOSI → GPIO 5
//    MISO → GPIO 6
//
//  nRF24 #1 (CH 0-31):
//    CSN  → GPIO 7
//    CE   → GPIO 8
//
//  nRF24 #2 (CH 32-63):
//    CSN  → GPIO 15
//    CE   → GPIO 16
//
//  nRF24 #3 (CH 64-94):
//    CSN  → GPIO 17
//    CE   → GPIO 18
//
//  nRF24 #4 (CH 95-125):
//    CSN  → GPIO 21
//    CE   → GPIO 1
//
//  Güç:
//    VCC  → ESP32-S3 SuperMini 3.3V
//    GND  → GND
// ═══════════════════════════════════════════════════════════

// ── SPI (paylaşımlı) ──────────────────────────────────────
#define PIN_SCK     4
#define PIN_MOSI    5
#define PIN_MISO    6

// ── nRF24 #1 ──────────────────────────────────────────────
#define CSN1        7
#define CE1         8

// ── nRF24 #2 ──────────────────────────────────────────────
#define CSN2        9
#define CE2         10

// ── nRF24 #3 ──────────────────────────────────────────────
#define CSN3        11
#define CE3         12

// ── nRF24 #4 ──────────────────────────────────────────────
#define CSN4        13
#define CE4         1

// ── Durum LED ─────────────────────────────────────────────
// SuperMini'de onboard WS2812 GPIO48'de YOK — LED devre dışı
#define PIN_LED     -1  // LED yok

// ── Güç parametreleri ─────────────────────────────────────
// RF24_PA_HIGH = +7 dBm = %85 güç
// RF24_PA_MAX  = +20 dBm = %100 güç (ısınma riski)
// RF24_PA_LOW  = -6 dBm = %40 güç
#define NRF_PA_LEVEL  RF24_PA_HIGH  // +7 dBm — Slave USB'den beslendiğinde güvenli

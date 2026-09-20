// ═══════════════════════════════════════════════════════════
//  nrf24_slave.ino — ESP32-S3 SuperMini v2.0
//  4x nRF24L01+ kontrol + UART → DevKitC haberleşme
//
//  Kütüphaneler:
//    - RF24 by TMRh20 (v1.4.x)
//    - Adafruit NeoPixel
//
//  Board: ESP32S3 Dev Module
//  PSRAM: QSPI PSRAM
//  Flash: 4MB / Huge APP (3MB)
//  USB CDC On Boot: Enabled
//
//  UART Bağlantısı:
//    SuperMini TX (GPIO21) → DevKitC GPIO44 (RX)
//    SuperMini RX (GPIO20) ← DevKitC GPIO43 (TX)
//    GND → GND
//
//  Komutlar (DevKitC → SuperMini):
//    JAM          → Band Sweep jammer (mod 1)
//    JAM:1        → Band Sweep
//    JAM:2        → BLE Focused
//    JAM:3        → WiFi Focused
//    JAM:4        → Drone Focused
//    JAM:5        → Rolling Chaos
//    SCAN         → Drone scanner
//    FOCUS:DJI    → Scanner DJI odak
//    SPEC         → Spectrum analyzer
//    PEAK_RST     → Spectrum peak sıfırla
//    MITM_P       → Pasif MITM
//    MITM_A       → Aktif MITM (relay)
//    INJ:AABB...  → Hex paket enjeksiyonu (MITM aktifken)
//    KEYS:text    → Tuş enjeksiyonu (MouseJack)
//    DROP         → Bir sonraki paketi düşür
//    STAT         → İstatistik raporu
//    PING         → Bağlantı testi
//    STOP         → Mevcut modu durdur
//    RESET        → Tüm modülleri sıfırla
//    DWELL:n:us   → Mod n için dwell time µs olarak ayarla (örn. DWELL:2:300)
//    REACT        → Reaktif jamming (sıcak kanallar 20x, boş 2x burst)
// ═══════════════════════════════════════════════════════════

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>          // WiFi.mode(WIFI_OFF) için
#include "esp_wifi.h"       // WiFi donanımını derin de-init etmek için
#include "esp_bt.h"         // Bluetooth donanımını derin de-init etmek için

#include "pin_config.h"
#include "mod_ble_uart.h"   // UART haberleşme (ble_uart.h adı kaldı uyumluluk için)
#include "mod_scanner.h"
#include "mod_jammer.h"
#include "mod_mitm.h"

// ── Mod tanımları ─────────────────────────────────────────
enum class SlaveMode : uint8_t {
    IDLE     = 0,
    JAM      = 1,
    SCAN     = 2,
    SPEC     = 3,
    MITM_P   = 4,
    MITM_A   = 5,
};

static SlaveMode g_mode    = SlaveMode::IDLE;
static bool      g_running = false;

// ── LED ──────────────────────────────────────────────────
#if PIN_LED >= 0
static Adafruit_NeoPixel g_led(1, PIN_LED, NEO_GRB + NEO_KHZ800);
static void setLED(uint8_t r, uint8_t g, uint8_t b) {
    g_led.setPixelColor(0, g_led.Color(r, g, b));
    g_led.show();
}
#else
static void setLED(uint8_t, uint8_t, uint8_t) {} // LED yok — boş fonksiyon
#endif

static void updateLED() {
#if PIN_LED >= 0
    switch (g_mode) {
        case SlaveMode::IDLE:   setLED(0,   0,  30); break;
        case SlaveMode::JAM:    setLED(50,  0,   0); break;
        case SlaveMode::SCAN:   setLED(0,  50,   0); break;
        case SlaveMode::SPEC:   setLED(50, 50,   0); break;
        case SlaveMode::MITM_P: setLED(30,  0,  50); break;
        case SlaveMode::MITM_A: setLED(50, 25,   0); break;
    }
#endif
}

// ── LED nefes efekti (IDLE) ───────────────────────────────
static void breathLED() {
#if PIN_LED >= 0
    static uint32_t lastBreath = 0;
    static uint8_t  val = 0;
    static int8_t   dir = 1;
    if (millis() - lastBreath < 15) return;
    lastBreath = millis();
    val += dir * 2;
    if (val >= 40) dir = -1;
    if (val == 0)  dir =  1;
    setLED(0, 0, val);
#endif
}

// ── Sistem durumu ─────────────────────────────────────────
static void sendStatus() {
    const char* modeStr[] = {
        "IDLE", "JAM", "SCAN", "SPEC", "MITM_P", "MITM_A"
    };
    String s = "STATUS:";
    s += modeStr[(uint8_t)g_mode];
    s += ":nRF24=";

    // Hızlı modül kontrolü (2 Modüle düşürüldü)
    uint8_t okCount = 0;
    RF24* radios[2] = { &jam1, &jam2 };
    for (uint8_t i = 0; i < 2; i++) {
        if (radios[i]->isChipConnected()) okCount++;
    }
    s += String(okCount) + "/2\n";
    bleUartSend(s);
}

// ── Modülleri sıfırla ─────────────────────────────────────
static void resetAllRadios() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
    RF24* radios[2] = { &jam1, &jam2 };
    uint8_t okCount = 0;
    for (uint8_t i = 0; i < 2; i++) {
        radios[i]->powerDown();
        delay(10);
        if (radios[i]->begin(&SPI)) {
            radios[i]->setPALevel(NRF_PA_LEVEL);
            radios[i]->powerDown();
            okCount++;
        }
    }
    bleUartSend("RESET:OK:" + String(okCount) + "/2\n");
    Serial.println("[RESET] " + String(okCount) + "/2 modül hazır");
}

// ── Komut işleyici ────────────────────────────────────────
static void handleCmd(const String& cmd) {
    Serial.println("[CMD] '" + cmd + "'");

    // ── PING ──────────────────────────────────────────────
    if (cmd == "PING") {
        bleUartSend("PONG:nRF24-Slave:v2.0\n");
        return;
    }

    // ── STATUS ────────────────────────────────────────────
    if (cmd == "STATUS" || cmd == "STAT") {
        sendStatus();
        return;
    }

    // ── RESET ─────────────────────────────────────────────
    if (cmd == "RESET") {
        resetAllRadios();
        return;
    }

    // ── STOP ─── zaten çalışmıyorsa ignore ───────────────
    if (cmd == "STOP") {
        bleUartSend("STATUS:IDLE\n");
        return;
    }

    // ── JAM ───────────────────────────────────────────────
    if (cmd == "JAM" || cmd.startsWith("JAM:")) {
        uint8_t mode = 1;
        if (cmd.length() > 4) {
            mode = cmd.substring(4).toInt();
            if (mode < 1 || mode > 7) mode = 1;
        }
        g_mode = SlaveMode::JAM;
        g_running = true;
        updateLED();
        jammerRun(mode);
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    // ── SCAN ──────────────────────────────────────────────
    else if (cmd == "SCAN") {
        g_mode = SlaveMode::SCAN;
        g_running = true;
        updateLED();
        droneScanner();
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    // ── SPEC ──────────────────────────────────────────────
    else if (cmd == "SPEC") {
        g_mode = SlaveMode::SPEC;
        g_running = true;
        updateLED();
        spectrumAnalyzer();
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    // ── MITM Pasif ────────────────────────────────────────
    else if (cmd == "MITM_P") {
        g_mode = SlaveMode::MITM_P;
        g_running = true;
        updateLED();
        mitmPassive();
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    // ── MITM Aktif ────────────────────────────────────────
    else if (cmd == "MITM_A") {
        g_mode = SlaveMode::MITM_A;
        g_running = true;
        updateLED();
        mitmActive();
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    // ── DWELL — runtime dwell time değiştir ──────────────────
    else if (cmd.startsWith("DWELL:")) {
        // Jammer çalışmıyorsa doğrudan g_dwellTimeUs güncelle
        int c1 = cmd.indexOf(':', 6);
        if (c1 > 0) {
            uint8_t  m  = cmd.substring(6, c1).toInt();
            uint16_t us = cmd.substring(c1 + 1).toInt();
            if (m >= 1 && m <= 7) {
                g_dwellTimeUs[m - 1] = us;
                bleUartSend("DWELL:OK:" + String(m) + ":" + String(us) + "\n");
            } else {
                bleUartSend("ERR:DWELL:INVALID_MOD\n");
            }
        }
    }

    // ── REACT — reaktif jamming ───────────────────────────────
    else if (cmd == "REACT") {
        g_mode = SlaveMode::JAM;
        g_running = true;
        updateLED();
        scanHotChannels();
        reactiveJam();
        g_mode = SlaveMode::IDLE;
        g_running = false;
    }

    else {
        bleUartSend("ERR:UNKNOWN_CMD:" + cmd + "\n");
    }

    updateLED();
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    // Serial Monitor açılana kadar bekle (max 4 saniye)
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 4000) { delay(10); }
    delay(200);

    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║  nRF24 Slave v2.0            ║");
    Serial.println("║  ESP32-S3 SuperMini          ║");
    Serial.println("╚══════════════════════════════╝");

    // WiFi'yi kapat — bu slave sadece nRF24 + UART kullanıyor
    // ESP32-S3'te btStop() veya derin deinit SPI clocklarını kapatabileceği için
    // sadece standart WiFi.mode(WIFI_OFF) kullanıyoruz.
    WiFi.mode(WIFI_OFF);
    Serial.println("[POWER] WiFi kapatıldı");

    // LED init
#if PIN_LED >= 0
    g_led.begin();
    g_led.setBrightness(25);
    setLED(50, 0, 50);  // mor — başlatılıyor
    delay(200);
#endif

    // UART init
    bleUartInit();



    // nRF24 modül kontrolü (2 Modüle düşürüldü)
    RF24* radios[2] = { &jam1, &jam2 };
    uint8_t csnPins[4] = { CSN1, CSN2, CSN3, CSN4 }; // 3 ve 4 hala tanımlı, güvenliğe alacağız
    uint8_t cePins[2]  = { CE1,  CE2  };
    uint8_t okCount = 0;

    // Tüm CSN pinlerini (sökülmüş olanlar dahil) HIGH yap —
    // açıkta kalan hatların bus çakışmasını engeller
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(csnPins[i], OUTPUT);
        digitalWrite(csnPins[i], HIGH);
    }
    // MISO float'ta kalmasın — pull-up ekle
    pinMode(PIN_MISO, INPUT_PULLUP);
    delay(10);

    // SPI init
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
    Serial.printf("[SPI] SCK=%d MOSI=%d MISO=%d\n",
                  PIN_SCK, PIN_MOSI, PIN_MISO);

    for (uint8_t i = 0; i < 2; i++) {
        yield(); // watchdog besle
        bool ok = radios[i]->begin(&SPI);
        if (ok) {
            radios[i]->setPALevel(NRF_PA_LEVEL);
            radios[i]->setDataRate(RF24_2MBPS);
            radios[i]->powerDown();
            okCount++;
            Serial.printf("[nRF24 #%d] OK CSN=%d CE=%d\n",
                          i+1, csnPins[i], cePins[i]);
        } else {
            Serial.printf("[nRF24 #%d] -- yok/bagli degil CSN=%d CE=%d\n",
                          i+1, csnPins[i], cePins[i]);
        }
    }

    Serial.printf("[INIT] %d/2 modül hazır\n", okCount);

    // Başlangıç mesajı gönder
    delay(500);
    bleUartSend("HELLO:nRF24-Slave:v2.0:" + String(okCount) + "/2\n");

    g_mode = SlaveMode::IDLE;
    updateLED();

    Serial.println("[READY] Komut bekleniyor...");
    Serial.println("────────────────────────────────");
    Serial.println("Komutlar: JAM / JAM:1-5 / SCAN");
    Serial.println("          SPEC / MITM_P / MITM_A");
    Serial.println("          PING / STATUS / RESET");
    Serial.println("────────────────────────────────");
}

// ── Ana döngü ─────────────────────────────────────────────
void loop() {
    // IDLE'dayken LED nefes efekti
    if (g_mode == SlaveMode::IDLE) {
        breathLED();
    }

    // Her 10 saniyede bir HELLO gönder — DevKitC bağlantı teyidi
    static uint32_t lastHello = 0;
    if (millis() - lastHello > 10000) {
        lastHello = millis();
        bleUartSend("ALIVE:" + String(millis()/1000) + "s\n");
    }

    // Komut kontrolü
    String cmd = bleGetCmd();
    if (cmd.length() > 0) {
        handleCmd(cmd);
    }

    delay(10);
}

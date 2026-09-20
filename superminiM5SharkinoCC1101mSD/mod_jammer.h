#pragma once
// ═══════════════════════════════════════════════════════════
//  mod_jammer.h — 4x nRF24L01+ Çok-Bus Koordineli Jammer v4.0
//
//  v4.0 Geliştirmeleri:
//  ✓ SPI 10 MHz — nRF24L01+ donanımsal üst limit, kanal değişim hızı max
//  ✓ Interleaved (Zaman Bölmeli) Jamming — çift anlık akım ~230mA'ya düştü
//    (PA+LNA modüller için SuperMini 500mA regülatörünü korur)
//  ✓ Spektrum üst sınırı CH 83 (2483 MHz) — gereksiz 84-125 kanalları elendi
//    Band Sweep, Chaos, CW modlarında %35 daha hızlı tarama
//  ✓ %80 Güç Kısıtlaması (RF24_PA_HIGH) korundu
//
//  Mod 1 — Band Sweep   : CH 0-83 interleaved paket burst
//  Mod 2 — BLE Pinpoint : CH37/38/39 cluster odak
//  Mod 3 — WiFi Target  : CH1/6/11/13 ±kenar bombardman
//  Mod 4 — Drone Hunt   : DJI/Syma/FrSky hop simulation
//  Mod 5 — Chaos        : Rastgele CH(0-83) + rastgele burst
//  Mod 6 — CW Sweep     : Donanımsal taşıyıcı dalga (0-83)
// ═══════════════════════════════════════════════════════════

#include <RF24.h>
#include <SPI.h>
#include "pin_config.h"

// SPI hızını 8 MHz'e düşürdük. Uzun kablolarda ve breadboard kurulumlarında 
// sinyal bozulmasını (data corruption) engelleyerek kararlı çalışmayı sağlar.
static SPISettings g_jamSpiSettings(8000000, MSBFIRST, SPI_MODE0);

// ── Modüller (Ortak SPI Bus: Pin 4,5,6) ───────────────────
RF24 jam1(CE1, CSN1);
RF24 jam2(CE2, CSN2);
RF24 jam3(CE3, CSN3);
RF24 jam4(CE4, CSN4);

// ── Spektrum Sınırı ───────────────────────────────────────
// Classic BT + WiFi + BLE toplam bant: 2400-2483.5 MHz = CH 0-83
// CH 84-125 arası ticari herhangi bir cihaz kullanmaz → gereksiz
// Bu sınırla tarama alanı ~%35 daralır = tarama hızı %35 artar
static constexpr uint8_t JAM_CH_MAX = 83;

struct JamRadio {
    RF24*    radio;
    uint8_t  chStart;
    uint8_t  chEnd;
    uint8_t  curCh;
    bool     ok;
    uint32_t txCount;
    uint8_t  ccaFail;
    uint8_t  burstSize;
    bool     isCW;
    bool     isActive;   // Interleaved: şu an aktif mi?
};

// Kanal dağılımı: 2 modüle göre spektrumu ikiye bölüyoruz (CH 0-83)
static JamRadio g_jammers[2] = {
    { &jam1,   0,  41,   0, false, 0, 0, 10, false, true  },
    { &jam2,  42,  83,  42, false, 0, 0, 10, false, false }
};

// ── Interleaved (Zaman Bölmeli) kontrol ───────────────────
// 0 veya 1: sırayla 1 modül aktif
static uint8_t g_interleavedPhase = 0;

static uint16_t g_dwellTimeUs[7] = {200, 500, 300, 150, 0, 130, 130};
static uint8_t g_hotChannels[126] = {0};
static uint8_t g_hotChCount = 0;

static void interleavedSwitch() {
    g_interleavedPhase = (g_interleavedPhase + 1) % 2;

    for (uint8_t i = 0; i < 2; i++) {
        if (!g_jammers[i].ok) continue;
        bool shouldBeActive = (i == g_interleavedPhase);
        g_jammers[i].isActive = shouldBeActive;

        if (!shouldBeActive) {
            if (g_jammers[i].isCW) {
                g_jammers[i].radio->stopConstCarrier();
                g_jammers[i].isCW = false;
            }
            // NOT: powerDown() çağrısını inrush current sıçramalarını önlemek için kaldırdık.
            // Modül Standby modunda bekleyecek, bu da çok düşük akım çeker ve ani voltaj düşüşü yapmaz.
        }
    }
}

// ── BLE Kesin Kanal Tablosu ───────────────────────────────
static const uint8_t BLE_FOCUS_CH[] = {
    1, 2, 3,      // CH37 cluster (2402 MHz)
    25, 26, 27,   // CH38 cluster (2426 MHz)
    79, 80, 81,   // CH39 cluster (2480 MHz)
};
static constexpr uint8_t BLE_FOCUS_CNT = 9;

// ── WiFi Kanal Tablosu ────────────────────────────────────
static const uint8_t WIFI_FOCUS_CH[] = {
    10,11,12,13,14,15,    // WiFi CH1  (2412 MHz) ±kenar
    35,36,37,38,39,40,    // WiFi CH6  (2437 MHz) ±kenar
    60,61,62,63,64,65,    // WiFi CH11 (2462 MHz) ±kenar
    70,71,72,73,74,75,    // WiFi CH13 (2472 MHz) ±kenar
};
static constexpr uint8_t WIFI_FOCUS_CNT = 24;

// ── Drone Kanal Tablosu ───────────────────────────────────
// Tüm kanallar JAM_CH_MAX(83) altında — filtreye gerek yok
static const uint8_t DRONE_FOCUS_CH[] = {
    36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,
    40,43,46,49,52,55,58,61,
    0,10,20,30,40,50,60,70,80,
};
static constexpr uint8_t DRONE_FOCUS_CNT = 36;

// ── Payload Üretimi ───────────────────────────────────────
static uint8_t g_jamPayload[32];

enum class PayloadType : uint8_t {
    RANDOM      = 0,
    ALL_HIGH    = 1,
    ALTERNATING = 2,
    PREAMBLE    = 3,
};

static void generatePayload(PayloadType type) {
    switch (type) {
        case PayloadType::RANDOM:
            esp_fill_random(g_jamPayload, 32); break;
        case PayloadType::ALL_HIGH:
            memset(g_jamPayload, 0xFF, 32); break;
        case PayloadType::ALTERNATING:
            for (uint8_t i = 0; i < 32; i++)
                g_jamPayload[i] = (i % 2 == 0) ? 0xAA : 0x55;
            break;
        case PayloadType::PREAMBLE:
            for (uint8_t i = 0; i < 32; i++)
                g_jamPayload[i] = (i % 4 < 2) ? 0xAA : 0x55;
            break;
    }
}

// ── Mesaj Gönderim Durum Bayrakları ───────────────────────
static bool g_wifiMsgSent    = false;
static bool g_bleMsgSent     = false;
static bool g_btStartMsgSent = false;

// ── PA Seviyesi ───────────────────────────────────────────
static rf24_pa_dbm_e getAdaptivePA(uint8_t /*ch*/) {
    return (rf24_pa_dbm_e)NRF_PA_LEVEL; // RF24_PA_HIGH = +7dBm = ~%80
}

// ── Modül Başlatma ────────────────────────────────────────
static bool jammerInitRadio(RF24* r, uint8_t ch) {
    if (!r->begin(&SPI)) return false;

    r->powerUp(); // Modülü bir kez baştan uyandır (dalgalanmayı önlemek için)
    r->setAutoAck(false);
    r->setCRCLength(RF24_CRC_DISABLED);
    r->setRetries(0, 0);
    r->setDataRate(RF24_2MBPS);
    r->setPALevel(getAdaptivePA(ch));
    r->setPayloadSize(32);       // Standart 32-byte (Scanner/Spectrum uyumluluğu için)
    r->setAddressWidth(5);       // Standart 5-byte adres genişliği
    r->setChannel(ch);
    r->openWritingPipe((const uint8_t*)"NNJAM"); // Standart 5-byte adres
    r->stopListening();

    return true;
}

// ── Adaptive Burst TX ─────────────────────────────────────
static void jammerBurstAdaptive(RF24* r, uint8_t* payload,
                                 uint8_t payloadLen,
                                 uint8_t targetBurst,
                                 uint8_t* intensity) {
    uint8_t adjBurst = targetBurst;
    if (*intensity > 50) adjBurst = (targetBurst * 3) / 2;
    if (*intensity > 80) adjBurst =  targetBurst * 2;

    for (uint8_t b = 0; b < adjBurst; b++) {
        r->writeFast(payload, payloadLen, true);
        delayMicroseconds(esp_random() % 5 + 1);
    }
    r->txStandBy();
    *intensity = (*intensity * 7 + 30) / 8;
}

// ── Jammer Başlatma ───────────────────────────────────────
static uint8_t jammerInit() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
    // Global SPI transaction hızını ayarla
    SPI.beginTransaction(g_jamSpiSettings);
    SPI.endTransaction();

    g_wifiMsgSent    = false;
    g_bleMsgSent     = false;
    g_btStartMsgSent = false;

    uint8_t okCount = 0;
    for (auto& j : g_jammers) {
        j.ok       = jammerInitRadio(j.radio, j.chStart);
        j.txCount  = 0;
        j.ccaFail  = 0;
        j.burstSize= 10;
        j.isCW     = false;
        j.isActive = false;  // başta hepsi uyuyor
        if (j.ok) okCount++;
    }
    // Sadece modül 0 ile başla (4-fazlı interleaving: 1 modül aktif)
    g_interleavedPhase = 0;
    if (g_jammers[0].ok) g_jammers[0].isActive = true;
    return okCount;
}

// ── Jammer Durdurma ───────────────────────────────────────
static void jammerStop() {
    g_wifiMsgSent    = false;
    g_bleMsgSent     = false;
    g_btStartMsgSent = false;
    for (auto& j : g_jammers) {
        if (!j.ok) continue;
        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }
        j.radio->powerDown();
        j.curCh   = j.chStart;
        j.txCount = 0;
        j.isActive= true;
    }
}

// ─────────────────────────────────────────────────────────
//  MOD 1 — Band Sweep (Interleaved, CH 0-83)
// ─────────────────────────────────────────────────────────
static void modBandSweep(uint8_t burstPerCh, PayloadType ptype) {
    generatePayload(ptype);
    bool anyTx = false;

    for (uint8_t i = 0; i < 2; i++) {
        auto& j = g_jammers[i];
        if (!j.ok || !j.isActive) continue;

        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }

        j.radio->powerUp();
        j.radio->setChannel(j.curCh);
        if (ptype == PayloadType::RANDOM)
            esp_fill_random(g_jamPayload, 32);
        jammerBurstAdaptive(j.radio, g_jamPayload, 32, burstPerCh, &j.ccaFail);
        j.txCount += burstPerCh;

        j.curCh++;
        if (j.curCh > j.chEnd) j.curCh = j.chStart;
        anyTx = true;
    }
    if (anyTx) {
        if (g_dwellTimeUs[0] > 0) {
            delayMicroseconds(g_dwellTimeUs[0] + (esp_random() % 40));
        }
        interleavedSwitch();
    }
}

// ─────────────────────────────────────────────────────────
//  MOD 2 — BLE Full Target (Donanımsal CW + Adv & Data Kanalları + Interleaved)
//  BLE Advertising (3 adet):
//    CH37 = nRF24 CH2  (2402 MHz)
//    CH38 = nRF24 CH26 (2426 MHz)
//    CH39 = nRF24 CH80 (2480 MHz)
//  2 modül dağılımı:
//    #1 (Modül 0) → CH37/38/39 advertising + Alt data kanalları (nRF24 CH 2, 26, 80 ve 4..38 arası)
//    #2 (Modül 1) → Üst data kanalları (nRF24 CH 40..78 arası)
//  Tüm modüller startConstCarrier() (CW) kullanır.
// ─────────────────────────────────────────────────────────
static void modBleFocused() {
    static const uint8_t m1AdvData[] = {2, 26, 80, 4, 8, 12, 16, 20, 24, 28, 32, 36};
    static const uint8_t m2Data[]    = {40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 78};

    static uint8_t idx[2] = {0, 0};
    bool anyTx = false;

    if (!g_bleMsgSent) {
        bleUartSend("JAM:BLE_FULL:ADV+DATA\n");
        g_bleMsgSent = true;
    }

    for (uint8_t i = 0; i < 2; i++) {
        auto& j = g_jammers[i];
        if (!j.ok || !j.isActive) continue;

        uint8_t targetCh = 0;
        if (i == 0) {
            targetCh = m1AdvData[idx[0] % 12];
            idx[0] = (idx[0] + 1) % 12;
        } else if (i == 1) {
            targetCh = m2Data[idx[1] % 11];
            idx[1] = (idx[1] + 1) % 11;
        }

        j.curCh = targetCh;

        // CW modu — gerçek Nordic nRF24L01+PA+LNA modüller destekler
        if (!j.isCW) {
            j.radio->startConstCarrier(getAdaptivePA(j.curCh), j.curCh);
            j.isCW = true;
        } else {
            j.radio->setChannel(j.curCh);
        }

        j.txCount++;
        anyTx = true;
    }

    if (anyTx) {
        if (g_dwellTimeUs[1] > 0) {
            delayMicroseconds(g_dwellTimeUs[1] + (esp_random() % 40));
        }
        interleavedSwitch();
    }
}


// ─────────────────────────────────────────────────────────
//  MOD 3 — WiFi Full Target (Donanımsal CW + CH1-CH13 Hepsi + Interleaved)
//  WiFi CH1-13 nRF24 Eşdeğerleri:
//    CH1=12, CH2=17, CH3=22, CH4=27, CH5=32, CH6=37,
//    CH7=42, CH8=47, CH9=52, CH10=57, CH11=62, CH12=67, CH13=72
//  4 modül dağılımı:
//    #1 (Modül 0) → CH1-4   (12, 17, 22, 27)
//    #2 (Modül 1) → CH5-8   (32, 37, 42, 47)
//    #3 (Modül 2) → CH9-11  (52, 57, 62)
//    #4 (Modül 3) → CH12-13 + ±3 marjin (64,65,66,67,68,69,70,71,72,73,74,75)
//  Tüm modüller startConstCarrier() (CW) kullanır.
// ─────────────────────────────────────────────────────────
static void modWifiFocused() {
    // WiFi CH 1-13 frekanslarına karşılık gelen nRF24 kanalları
    static const uint8_t wifiChannels[] = {
        12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72
    };
    static uint8_t idx = 0;
    
    if (!g_wifiMsgSent) {
        bleUartSend("JAM:WIFI_FULL:CH1-13\n");
        g_wifiMsgSent = true;
    }

    auto& j = g_jammers[0]; // Sadece jam1 (ilk modül) kullanılıyor. LDO ve SPI güvende.
    if (!j.ok) return;

    // Diğer 3 modülü güç çekmemeleri için kapatalım
    if (g_jammers[1].ok && g_jammers[1].isCW) { g_jammers[1].radio->stopConstCarrier(); g_jammers[1].isCW = false; }
    if (g_jammers[2].ok && g_jammers[2].isCW) { g_jammers[2].radio->stopConstCarrier(); g_jammers[2].isCW = false; }
    if (g_jammers[3].ok && g_jammers[3].isCW) { g_jammers[3].radio->stopConstCarrier(); g_jammers[3].isCW = false; }

    uint8_t targetCh = wifiChannels[idx % 13];
    idx++;

    j.curCh = targetCh;

    // CW Modu (Taşıyıcı Dalga) başlat ve hızlıca kanal değiştir
    if (!j.isCW) {
        j.radio->startConstCarrier(getAdaptivePA(j.curCh), j.curCh);
        j.isCW = true;
    } else {
        j.radio->setChannel(j.curCh);
    }

    j.txCount++;

    // Kaotik gecikme (jitter) ile hedef sistemlerin kaçmasını engelle
    if (g_dwellTimeUs[2] > 0) {
        delayMicroseconds(g_dwellTimeUs[2] + (esp_random() % 40));
    }
}


// ─────────────────────────────────────────────────────────
//  MOD 4 — Drone Hunt (4 Modül Bağımsız Protokol Dağılımı)
//  #1 → DJI (CH36-74) + Syma (CH43,49,55,61)
//  #2 → FrSky + FlySky AFHDS/AFHDS-2A (16 hop CH0-83)
//  #3 → DSM2/DSMX (CH10,30,50,70) + Futaba FASST (CH5-75)
//  #4 → CrazyRadio (nRF24 2.4G link) + Yüksek Frekans Drone Hop
// ─────────────────────────────────────────────────────────
static void modDroneFocused() {
    // 2 modüle göre drone kanallarını birleştirdik
    static const uint8_t m1DjiSymaFrSky[] = {36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 43, 49, 55, 61, 2, 7, 12, 17, 22, 27, 32, 37};
    static const uint8_t m2DsmFutabaCrazy[] = {5, 15, 25, 35, 45, 55, 65, 75, 10, 30, 50, 70, 3, 9, 14, 21, 28, 33, 41, 48, 53, 61, 69, 76, 81};

    static uint8_t idx[2] = {0, 0};
    generatePayload(PayloadType::ALL_HIGH);
    bool anyTx = false;

    for (uint8_t i = 0; i < 2; i++) {
        auto& j = g_jammers[i];
        if (!j.ok || !j.isActive) continue;

        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }
        j.radio->powerUp();

        uint8_t ch = 0;
        if (i == 0)      { ch = m1DjiSymaFrSky[idx[0] % 32]; idx[0] = (idx[0] + 1) % 32; }
        else if (i == 1) { ch = m2DsmFutabaCrazy[idx[1] % 25]; idx[1] = (idx[1] + 1) % 25; }

        j.curCh = ch;
        j.radio->setChannel(ch);
        jammerBurstAdaptive(j.radio, g_jamPayload, 32, 25, &j.ccaFail);
        j.txCount += 25;
        anyTx = true;
    }

    if (anyTx) {
        if (g_dwellTimeUs[3] > 0) {
            delayMicroseconds(g_dwellTimeUs[3] + (esp_random() % 40));
        }
        interleavedSwitch();
    }
}

// ─────────────────────────────────────────────────────────
//  MOD 5 — Chaos (Çakışma Önlemeli Rastgele CH0-83)
//  - 2 modül birbiriyle en az 10 kanal mesafede tutulur
//  - Aynı veya ±3 yakınlıktaki kanallar ardışık seçilmez
// ─────────────────────────────────────────────────────────
static void modRollingChaos() {
    static uint8_t lastCh[2] = {255, 255};
    uint8_t curSelected[2] = {255, 255};
    bool anyTx = false;

    for (uint8_t i = 0; i < 2; i++) {
        auto& j = g_jammers[i];
        if (!j.ok || !j.isActive) continue;

        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }
        j.radio->powerUp();

        uint8_t ch = 0;
        bool valid = false;
        for (uint8_t attempt = 0; attempt < 50; attempt++) {
            ch = esp_random() % (JAM_CH_MAX + 1);

            // 1. Önceki kanala aynı veya ±3 mesafede olmasın
            if (lastCh[i] != 255 && abs((int)ch - (int)lastCh[i]) <= 3) {
                continue;
            }

            // 2. Diğer modüllerin seçilen kanallarından en az 10 kanal uzakta olsun
            bool conflict = false;
            for (uint8_t k = 0; k < i; k++) {
                if (curSelected[k] != 255 && abs((int)ch - (int)curSelected[k]) < 10) {
                    conflict = true;
                    break;
                }
            }

            if (!conflict) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            ch = (i * 20 + (esp_random() % 15)) % (JAM_CH_MAX + 1);
        }

        lastCh[i] = ch;
        curSelected[i] = ch;
        j.curCh = ch;

        j.radio->setChannel(ch);
        PayloadType pt = (PayloadType)(esp_random() % 4);
        generatePayload(pt);

        uint8_t burst = 10 + (esp_random() % 26);
        jammerBurstAdaptive(j.radio, g_jamPayload, 32, burst, &j.ccaFail);
        j.txCount += burst;
        anyTx = true;
    }

    if (anyTx) {
        // Mod 5: g_dwellTimeUs[4]==0 → rastgele 100-400µs
        uint16_t dw = (g_dwellTimeUs[4] == 0)
                      ? (uint16_t)(100 + esp_random() % 301)
                      : g_dwellTimeUs[4];
        delayMicroseconds(dw);
        interleavedSwitch();
    }
}

// ─────────────────────────────────────────────────────────
//  MOD 6 — CW Sweep (powerDownsuz Donanımsal CW Interleaved)
//  - CW modunda powerDown() çağrılmaz (PLL lock / CW kopmaz)
//  - Faz A (Modül 1,3) ve Faz B (Modül 2,4) dönüşümlü 200µs çalışır
// ─────────────────────────────────────────────────────────
static bool g_cwPhaseToggle = false; // false = Faz A (0,2), true = Faz B (1,3)

static void modCWBandSweep() {
    g_cwPhaseToggle = !g_cwPhaseToggle;
    generatePayload(PayloadType::RANDOM);
    bool anyTx = false;

    // stopConstCarrier gereksiz aramaları engellemek için isCW temizlenir
    for (auto& j : g_jammers) {
        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }
    }

    if (!g_cwPhaseToggle) {
        // Faz A: Sadece Modül #1 (idx 0) aktif
        uint8_t i = 0;
        auto& j = g_jammers[i];
        if (j.ok) {
            j.radio->powerUp();
            j.radio->setChannel(j.curCh);
            
            // Hızlı 8 paketlik burst
            jammerBurstAdaptive(j.radio, g_jamPayload, 32, 8, &j.ccaFail);
            j.txCount += 8;
            anyTx = true;

            j.curCh++;
            if (j.curCh > j.chEnd) j.curCh = j.chStart;
        }
    } else {
        // Faz B: Sadece Modül #2 (idx 1) aktif
        uint8_t i = 1;
        auto& j = g_jammers[i];
        if (j.ok) {
            j.radio->powerUp();
            j.radio->setChannel(j.curCh);
            
            // Hızlı 8 paketlik burst
            jammerBurstAdaptive(j.radio, g_jamPayload, 32, 8, &j.ccaFail);
            j.txCount += 8;
            anyTx = true;

            j.curCh++;
            if (j.curCh > j.chEnd) j.curCh = j.chStart;
        }
    }

    if (anyTx) {
        if (g_dwellTimeUs[5] > 0) {
            delayMicroseconds(g_dwellTimeUs[5] + (esp_random() % 40));
        }
        // Fazlar arası donanımsal geçiş
        interleavedSwitch();
    }
}

// ─────────────────────────────────────────────────────────
//  MOD 7 — Classic BT Killer (Donanımsal CW + 130µs Dwell + Interleaved)
//  Classic BT: 79 kanal (BT CH0-78 = nRF24 CH2-80)
//  Hop hızı: 1600 hop/s → her kanalda 625µs kalıyor
//  Dwell time: 130µs (her kanala 625µs içinde ~4 kez vurma)
//  AFH Koruması: CH20-59 (orta bant) yüksek trafik 2x ağırlıklı
//  4 Modül Dağılımı:
//    #1 → BT CH0-19  (nRF24 CH2-21)  Faz A
//    #2 → BT CH20-39 (nRF24 CH22-41) Faz B (2x öncelik)
//    #3 → BT CH40-59 (nRF24 CH42-61) Faz A (2x öncelik)
//    #4 → BT CH60-78 (nRF24 CH62-80) Faz B
// ─────────────────────────────────────────────────────────
static uint32_t g_btHopTotalCount = 0;
static uint32_t g_btLastStatTime  = 0;

static void modClassicBTKiller() {
    // 2 modüle göre Bluetooth kanallarını böldük (BT CH0-78 = nRF24 CH2-80)
    // Modül 1: CH 2 - 41 (40 kanal, AFH orta bant yoğunluk için çiftlenmiş)
    static const uint8_t m1BtCh[40] = {
        2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,
        22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41
    };
    // Modül 2: CH 42 - 80 (39 kanal, AFH orta bant yoğunluk için çiftlenmiş)
    static const uint8_t m2BtCh[39] = {
        42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,
        62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80
    };

    static uint8_t idx[2] = {0, 0};

    if (!g_btStartMsgSent) {
        bleUartSend("JAM:BT_CLASSIC:START\n");
        g_btStartMsgSent = true;
        g_btHopTotalCount = 0;
        g_btLastStatTime  = millis();
    }

    bool anyTx = false;

    for (uint8_t i = 0; i < 2; i++) {
        auto& j = g_jammers[i];
        if (!j.ok || !j.isActive) continue;

        uint8_t targetCh = 0;
        if (i == 0)      { targetCh = m1BtCh[idx[0] % 40]; idx[0] = (idx[0] + 1) % 40; }
        else if (i == 1) { targetCh = m2BtCh[idx[1] % 39]; idx[1] = (idx[1] + 1) % 39; }

        j.curCh = targetCh;

        // CW modu — gerçek Nordic nRF24L01+PA+LNA modüller destekler
        if (!j.isCW) {
            j.radio->startConstCarrier(getAdaptivePA(j.curCh), j.curCh);
            j.isCW = true;
        } else {
            j.radio->setChannel(j.curCh);
        }

        j.txCount++;
        g_btHopTotalCount++;
        anyTx = true;
    }

    // Dwell time: yapılandırılabilir (varsayılan 130µs → 625µs slot'ta 4x vurma) + Kaotik dalgalanma (0-39µs)
    if (g_dwellTimeUs[6] > 0) {
        delayMicroseconds(g_dwellTimeUs[6] + (esp_random() % 40));
    }

    if (anyTx) {
        interleavedSwitch();
    }

    // Her 2 saniyede bir UART istatistiği gönder
    if (millis() - g_btLastStatTime > 2000) {
        g_btLastStatTime = millis();
        bleUartSend("JAM:BT:HOP=" + String(g_btHopTotalCount) + "\n");
    }
}

// ── Sıcak Kanal Tarama (Hot Channel Scan) ─────────────────
// mod_scanner.h içindeki measureRSSI() ile RPD hit >= 3 olan kanalları bulur
// ve g_hotChannels dizisine kaydeder.
// Her 5 saniyede otomatik çağrılır.
static uint32_t g_hotScanLastMs = 0;

static void scanHotChannels() {
    g_hotChCount = 0;
    // jam1 modülü dinleme moduna geçirip tüm 0-83 kanalı tara
    // (jammerStop çağrılmış varsayılır — REACT komutu öncesi durdurulur)
    RF24* r = &jam1;
    r->begin(&SPI);
    r->setPALevel(NRF_PA_LEVEL);
    r->setDataRate(RF24_2MBPS);
    r->setAutoAck(false);
    r->setCRCLength(RF24_CRC_DISABLED);

    for (uint8_t ch = 0; ch <= JAM_CH_MAX; ch++) {
        uint8_t hits = measureRSSI(r, ch);
        if (hits >= 3) {
            g_hotChannels[g_hotChCount++] = ch;
            if (g_hotChCount >= 126) break;
        }
    }
    r->stopListening();
    r->powerDown();

    bleUartSend("JAM:HOT:" + String(g_hotChCount) + "\n");
    g_hotScanLastMs = millis();
}

// ── Reaktif Jamming ────────────────────────────────────────
// Sıcak kanallara 20 burst, boş kanallara 2 burst atar.
// REACT UART komutuyla tetiklenir.
static void reactiveJam() {
    if (g_hotChCount == 0) {
        bleUartSend("JAM:REACT:ERR:NO_HOT_CH\n");
        return;
    }
    bleUartSend("JAM:REACT:START:" + String(g_hotChCount) + "\n");

    // Hot kanal seti — hızlı lookup için boolean map
    bool isHot[JAM_CH_MAX + 1] = {false};
    for (uint8_t i = 0; i < g_hotChCount; i++) {
        if (g_hotChannels[i] <= JAM_CH_MAX) isHot[g_hotChannels[i]] = true;
    }

    generatePayload(PayloadType::RANDOM);

    for (uint8_t i = 0; i < 4; i++) {
        auto& j = g_jammers[i];
        if (!j.ok) continue;
        if (j.isCW) { j.radio->stopConstCarrier(); j.isCW = false; }
        j.radio->powerUp();
    }

    // Her kanalı tara; sıcak → 20 burst, soğuk → 2 burst
    for (uint8_t ch = 0; ch <= JAM_CH_MAX; ch++) {
        uint8_t burst = isHot[ch] ? 20 : 2;
        esp_fill_random(g_jamPayload, 32);

        // 4 modülü sırayla bu kanala yönlendir
        for (uint8_t i = 0; i < 4; i++) {
            auto& j = g_jammers[i];
            if (!j.ok) continue;
            j.radio->setChannel(ch);
            for (uint8_t b = 0; b < burst; b++) {
                j.radio->writeFast(g_jamPayload, 32, true);
            }
            j.radio->txStandBy();
            j.txCount += burst;
        }
        // Otomatik 5s güncelleme kontrolü
        if (millis() - g_hotScanLastMs > 5000) {
            jammerStop();
            scanHotChannels();
            // Yeniden başlat
            for (uint8_t i = 0; i < 4; i++) {
                auto& j = g_jammers[i];
                if (!j.ok) continue;
                j.radio->powerUp();
            }
            // Hot haritayı güncelle
            for (uint8_t k = 0; k <= JAM_CH_MAX; k++) isHot[k] = false;
            for (uint8_t k = 0; k < g_hotChCount; k++) {
                if (g_hotChannels[k] <= JAM_CH_MAX) isHot[g_hotChannels[k]] = true;
            }
        }

        // STOP komutunu dinle
        String cmd = bleGetCmd();
        if (cmd == "STOP") {
            bleUartSend("JAM:REACT:STOP\n");
            return;
        }
    }
    bleUartSend("JAM:REACT:DONE\n");
}

// ── İstatistik ────────────────────────────────────────────
static String jammerStats() {
    uint32_t total = 0;
    String s = "JAM:STAT:";
    for (uint8_t i = 0; i < 4; i++) {
        total += g_jammers[i].txCount;
        s += String(i+1) + "=" + String(g_jammers[i].txCount);
        if (i < 3) s += ",";
    }
    s += ":TOT=" + String(total);
    return s;
}

// ── Ana Jammer Döngüsü ────────────────────────────────────
static void jammerRun(uint8_t mode = 1) {
    uint8_t okCount = jammerInit();
    if (okCount == 0) {
        bleUartSend("JAM:ERR:NO_RADIO\n");
        return;
    }
    bleUartSend("JAM:OK:" + String(okCount) + "/4\n");

    const char* modeNames[] = {
        "", "BAND_SWEEP", "BLE_FOCUS",
        "WIFI_FOCUS", "DRONE_FOCUS", "CHAOS", "CW_SWEEP", "BT_CLASSIC"
    };
    if (mode > 7) mode = 1;
    bleUartSend("JAM:MODE:" + String(modeNames[mode]) + "\n");

    uint32_t count    = 0;
    uint32_t lastStat = 0;

    while (true) {
        switch (mode) {
            case 1: modBandSweep(10, PayloadType::RANDOM); break;
            case 2: modBleFocused();                        break;
            case 3: modWifiFocused();                       break;
            case 4: modDroneFocused();                      break;
            case 5: modRollingChaos();                      break;
            case 6: modCWBandSweep();                       break;
            case 7: modClassicBTKiller();                   break;
            default: modBandSweep(10, PayloadType::RANDOM); break;
        }
        count++;

        if (millis() - lastStat > 2000) {
            lastStat = millis();
            bleUartSend(jammerStats() + "\n");
        }

        String cmd = bleGetCmd();
        if (cmd == "STOP") break;

        // PING — jammer çalışırken de cevap ver
        if (cmd == "PING") {
            bleUartSend("PONG:nRF24-Slave:v2.0\n");
        }
        // JAM:n — mod değiştir
        if (cmd.startsWith("JAM:")) {
            uint8_t newMode = cmd.substring(4).toInt();
            if (newMode >= 1 && newMode <= 7) {
                mode = newMode;
                bleUartSend("JAM:MODE:" + String(modeNames[mode]) + "\n");
            }
        }
        // DWELL:mod:us — runtime dwell time değiştir (örn. DWELL:2:300)
        else if (cmd.startsWith("DWELL:")) {
            // Format: DWELL:<mod 1-7>:<mikrosaniye>
            int c1 = cmd.indexOf(':', 6);
            if (c1 > 0) {
                uint8_t  m  = cmd.substring(6, c1).toInt();
                uint16_t us = cmd.substring(c1 + 1).toInt();
                if (m >= 1 && m <= 7) {
                    g_dwellTimeUs[m - 1] = us;
                    bleUartSend("DWELL:OK:" + String(m) + ":" + String(us) + "\n");
                }
            }
        }
        // REACT — reaktif jamming modu (önce hot scan, sonra reaktif jam)
        else if (cmd == "REACT") {
            jammerStop();
            scanHotChannels();
            reactiveJam();
            // Reaktif jam bitti, normal jammer'a geri dön
            jammerInit();
            bleUartSend("JAM:MODE:" + String(modeNames[mode]) + "\n");
        }

        vTaskDelay(1);
    }


    bleUartSend(jammerStats() + "\n");
    jammerStop();
    bleUartSend("JAM:STOP:ROUNDS=" + String(count) + "\n");
}


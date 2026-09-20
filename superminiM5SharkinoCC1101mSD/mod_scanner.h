#pragma once
// ═══════════════════════════════════════════════════════════
//  mod_scanner.h — Gelişmiş Drone Scanner + Spectrum Analyzer
//
//  Drone Scanner:
//    - 4 modül paralel, her biri kendi band segmentini tarar
//    - Bilinen drone protokollerine göre imza eşleştirme
//    - RSSI ölçümü: RPD + çoklu örnekleme (8x) → güvenilir tespit
//    - Frekans hopping takibi — drone hareket ederse takip eder
//    - Paket yakalama + içerik analizi
//    - "DRONE:protokol:CH:RSSI:PKTS\n" formatında bildirim
//
//  Spectrum Analyzer:
//    - 126 kanal (2400-2525 MHz) tam tarama
//    - 4 modül paralel → 4x hız
//    - Her kanal 8 ölçüm ortalaması — gürültü filtresi
//    - Peak hold — en yüksek değeri tutar
//    - BLE/WiFi/Drone kanallarını etiketler
//    - "SPEC:v0,v1,...,v125\n" formatında gönderim
//
//  Bilinen Protokoller:
//    DJI Mini/Mavic/Air  → 2402-2483 MHz, FHSS, 10ms hop
//    FrSky D8/D16        → 2400-2525 MHz, FHSS, geniş band
//    Syma/WLtoys/JJRC    → 2440-2476 MHz, yarı-sabit kanal
//    CrazyFlie/CrazyRadio→ 2400-2525 MHz, özel ESB
//    FlySky AFHDS        → 2400-2525 MHz, 16 kanal hop
//    Spektrum DSM2/DSMX  → 2400-2483 MHz, 2 kanal sabit
//    FutabaFASST         → 2400-2483 MHz, FHSS
//    Generic nRF24       → 2400-2525 MHz, ESB
// ═══════════════════════════════════════════════════════════

#include <RF24.h>
#include "pin_config.h"

extern RF24 jam1, jam2, jam3, jam4;

// ── Drone protokol profilleri ─────────────────────────────
struct DroneProfile {
    const char* name;
    uint8_t     chMin;
    uint8_t     chMax;
    uint8_t     dataRate;   // 0=250kbps, 1=1Mbps, 2=2Mbps
    uint8_t     addrWidth;  // 3-5 byte
    bool        fixedCh;    // sabit kanal mı?
    int8_t      rssiMin;    // minimum sinyal eşiği (RPD hit sayısı /8)
};

static const DroneProfile DRONE_PROFILES[] = {
    // name          chMin chMax rate addr fixed rssiMin
    { "DJI",          0,   83,   1,   5,  false,  3 },
    { "FrSky-D8",     0,  125,   1,   3,  false,  2 },
    { "FrSky-D16",    0,  125,   2,   3,  false,  2 },
    { "Syma",        40,   76,   2,   5,  false,  4 },
    { "FlySky",       0,  125,   1,   4,  false,  2 },
    { "CrazyRadio",   0,  125,   2,   5,  false,  2 },
    { "DSM2",         0,   83,   2,   3,  true,   5 },
    { "DSMX",         0,   83,   2,   3,  false,  3 },
    { "Futaba",       0,   83,   1,   4,  false,  3 },
    { "Generic",      0,  125,   2,   5,  false,  1 },
};
static constexpr uint8_t DRONE_PROFILE_CNT = 10;

// ── Tespit yapısı ─────────────────────────────────────────
struct Detection {
    char     protocol[12];
    uint8_t  channel;
    uint8_t  rssiHits;    // 0-8 (8x RPD ölçümü)
    uint32_t firstSeen;
    uint32_t lastSeen;
    uint32_t pktCount;
    uint8_t  lastPayload[32];
    uint8_t  payloadLen;
    bool     active;
};

static Detection g_detections[32];
static uint8_t   g_detCount = 0;

// ── Spectrum buffer ───────────────────────────────────────
static uint8_t g_specBuf[126]     = {0};  // anlık RSSI
static uint8_t g_specPeak[126]    = {0};  // peak hold
static uint32_t g_specSweepCount  = 0;

// ── Scanner init ──────────────────────────────────────────
static uint8_t scannerInit() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
    RF24* radios[2] = { &jam1, &jam2 };
    uint8_t starts[2] = { 0, 60 };
    uint8_t okCount = 0;

    for (uint8_t i = 0; i < 2; i++) {
        if (!radios[i]->begin(&SPI)) continue;
        radios[i]->setPALevel(NRF_PA_LEVEL);
        radios[i]->setDataRate(RF24_2MBPS);
        radios[i]->setAutoAck(false);
        radios[i]->setCRCLength(RF24_CRC_DISABLED);
        radios[i]->setPayloadSize(32);
        radios[i]->setChannel(starts[i]);
        radios[i]->openReadingPipe(0, (const uint8_t*)"\xAA\xBB\xCC\xDD\xEE");
        radios[i]->openReadingPipe(1, (const uint8_t*)"\x12\x34\x56\x78\x9A");
        radios[i]->startListening();
        okCount++;
    }
    return okCount;
}

// ── RSSI ölçüm — 8 örnekli RPD ──────────────────────────
// nRF24 gerçek RSSI vermez, RPD (-64dBm eşiği) kullanır
// 8 ölçüm alıp kaç tanesinin pozitif geldiğine bakıyoruz
// Sonuç: 0-8 (8 = çok güçlü sinyal, 0 = sinyal yok)
static uint8_t measureRSSI(RF24* r, uint8_t ch) {
    r->stopListening();
    r->setChannel(ch);
    r->startListening();
    delayMicroseconds(130);  // settle time

    uint8_t hits = 0;
    // Modül zaten RX modunda (Listening). 
    // Döngü içinde sürekli kapat-aç yapmadan RPD register'ını test ediyoruz.
    for (uint8_t t = 0; t < 8; t++) {
        if (r->testRPD()) {
            hits++;
        }
        delayMicroseconds(20); // Örnekler arası kısa bekleme
    }
    r->stopListening();
    return hits;
}

// ── Paket yakalama denemesi ───────────────────────────────
static bool tryCapture(RF24* r, uint8_t ch, uint8_t addrWidth,
                       uint8_t* outBuf, uint8_t& outLen) {
    r->stopListening();
    r->setAddressWidth(addrWidth);
    r->setChannel(ch);
    r->startListening();
    delay(2);  // 2ms dinle

    if (r->available()) {
        outLen = r->getPayloadSize();
        if (outLen > 32) outLen = 32;
        r->read(outBuf, outLen);
        return true;
    }
    return false;
}

// ── Tespit kaydet veya güncelle ───────────────────────────
static void recordDetection(const char* proto, uint8_t ch,
                            uint8_t hits, uint8_t* payload, uint8_t plen) {
    // Mevcut tespitte var mı?
    for (uint8_t i = 0; i < g_detCount; i++) {
        if (strcmp(g_detections[i].protocol, proto) == 0 &&
            abs((int)g_detections[i].channel - ch) <= 5) {
            g_detections[i].channel  = ch;
            g_detections[i].rssiHits = hits;
            g_detections[i].lastSeen = millis();
            g_detections[i].pktCount++;
            g_detections[i].active   = true;
            if (payload && plen > 0) {
                memcpy(g_detections[i].lastPayload, payload, plen);
                g_detections[i].payloadLen = plen;
            }
            return;
        }
    }
    // Yeni tespit
    if (g_detCount < 32) {
        Detection& d = g_detections[g_detCount++];
        strncpy(d.protocol, proto, 11);
        d.channel   = ch;
        d.rssiHits  = hits;
        d.firstSeen = millis();
        d.lastSeen  = millis();
        d.pktCount  = 1;
        d.active    = true;
        if (payload && plen > 0) {
            d.payloadLen = plen;
        }
    }
}

// ── Payload protokol eşleştirme ───────────────────────────
static const char* matchProtocol(uint8_t* pkt, uint8_t len, uint8_t ch) {
    if (len < 4) return "Generic";

    // DJI imzası: ilk byte 0x55 veya 0xAA (sync byte)
    if ((pkt[0] == 0x55 || pkt[0] == 0xAA) && ch < 84)
        return "DJI";

    // CrazyRadio: belirli header pattern
    if (pkt[0] == 0x00 && pkt[1] == 0x00 && len >= 6)
        return "CrazyRadio";

    // FrSky D8: 0x7E header
    if (pkt[0] == 0x7E)
        return "FrSky-D8";

    // Syma: 0xA2 header, sabit kanal
    if (pkt[0] == 0xA2 && ch >= 40 && ch <= 76)
        return "Syma";

    // FlySky: kanal aralığı + uzun adres
    if (ch < 125 && len >= 10)
        return "FlySky";

    return "Generic";
}

// ── Drone Scanner ana döngüsü ─────────────────────────────
static void droneScanner() {
    uint8_t okCount = scannerInit();
    if (okCount == 0) {
        bleUartSend("SCAN:ERR:NO_RADIO\n");
        return;
    }
    bleUartSend("SCAN:OK:" + String(okCount) + "/2\n");

    RF24* radios[2] = { &jam1, &jam2 };
    uint8_t chCur[2]  = { 0,  60 };
    uint8_t chEnd[2]  = { 59, 125 };
    uint8_t chStep[2] = { 1,   1 };  // adaptif adım

    uint32_t totalDet  = 0;
    uint32_t scanCount = 0;
    uint32_t lastStat  = 0;

    uint8_t capBuf[32];
    uint8_t capLen = 0;

    while (true) {
        for (uint8_t i = 0; i < 2; i++) {
            if (radios[i] == nullptr) continue;

            uint8_t hits = measureRSSI(radios[i], chCur[i]);

            if (hits >= 1) {
                // Sinyal var — hangi protokol?
                for (uint8_t p = 0; p < DRONE_PROFILE_CNT; p++) {
                    const DroneProfile& prof = DRONE_PROFILES[p];
                    if (chCur[i] < prof.chMin || chCur[i] > prof.chMax) continue;
                    if (hits < prof.rssiMin) continue;

                    // Data rate ve adres genişliği ayarla
                    rf24_datarate_e dr = (prof.dataRate == 0) ? RF24_250KBPS :
                                         (prof.dataRate == 1) ? RF24_1MBPS : RF24_2MBPS;
                    radios[i]->setDataRate(dr);
                    radios[i]->setAddressWidth(prof.addrWidth);

                    // Paket yakalamayı dene
                    bool captured = tryCapture(radios[i], chCur[i],
                                               prof.addrWidth, capBuf, capLen);

                    const char* matchedProto = captured ?
                        matchProtocol(capBuf, capLen, chCur[i]) : prof.name;

                    recordDetection(matchedProto, chCur[i], hits,
                                    captured ? capBuf : nullptr,
                                    captured ? capLen : 0);

                    // Hex payload
                    String hexStr = "";
                    if (captured && capLen > 0) {
                        for (uint8_t b = 0; b < min((int)capLen, 8); b++) {
                            if (capBuf[b] < 0x10) hexStr += "0";
                            hexStr += String(capBuf[b], HEX);
                        }
                    }

                    bleUartSend("DRONE:" +
                        String(matchedProto) + ":" +
                        String(chCur[i]) + ":" +
                        String(hits) + ":" +
                        String(captured ? capLen : 0) + ":" +
                        hexStr + "\n");
                    totalDet++;

                    // Sinyal güçlüyse bu kanalda daha fazla dinle
                    if (hits >= 6) {
                        chStep[i] = 0;  // bu kanalda kal
                        delay(5);
                    } else {
                        chStep[i] = 1;
                    }

                    // İlk eşleşen profil yeterli
                    break;
                }

                // Data rate'i sıfırla
                radios[i]->setDataRate(RF24_2MBPS);
            } else {
                chStep[i] = 1;  // sinyal yok, ilerle
            }

            // Sonraki kanala geç
            chCur[i] = (chCur[i] + chStep[i]);
            if (chCur[i] > chEnd[i]) chCur[i] = (i == 0) ? 0 : 60;
        }

        scanCount++;

        // Her 2 saniyede bir durum raporu
        if (millis() - lastStat > 2000) {
            lastStat = millis();
            bleUartSend("SCAN:STAT:SWEEPS=" + String(scanCount) +
                        ":DET=" + String(totalDet) +
                        ":ACTIVE=" + String(g_detCount) + "\n");

            // Aktif tespitleri listele
            for (uint8_t d = 0; d < g_detCount; d++) {
                if (!g_detections[d].active) continue;
                // 10 saniyedir görülmeyen tespiti pasif yap
                if (millis() - g_detections[d].lastSeen > 10000)
                    g_detections[d].active = false;
            }
        }

        String cmd = bleGetCmd();
        if (cmd == "STOP") break;

        // Belirli bir protokole odaklan: "FOCUS:DJI"
        if (cmd.startsWith("FOCUS:")) {
            String target = cmd.substring(6);
            // Her iki modülü DJI kanallarına yönlendir
            chCur[0] = 30; chEnd[0] = 50; chStep[0] = 1;
            chCur[1] = 51; chEnd[1] = 71; chStep[1] = 1;
            bleUartSend("SCAN:FOCUS:" + target + "\n");
        }

        vTaskDelay(1);
    }

    for (uint8_t i = 0; i < 2; i++) {
        if (radios[i] != nullptr) radios[i]->powerDown();
    }
    bleUartSend("SCAN:STOP:DET=" + String(totalDet) + "\n");
}

// ── Spectrum Analyzer ─────────────────────────────────────
static void spectrumAnalyzer() {
    uint8_t okCount = scannerInit();
    if (okCount == 0) {
        bleUartSend("SPEC:ERR:NO_RADIO\n");
        return;
    }
    bleUartSend("SPEC:OK:" + String(okCount) + "/2\n");

    RF24* r = &jam1; // Sadece ilk modülü (jam1) kullanıyoruz. Tek modül SPI çakışmasını engeller.
    memset(g_specBuf,  0, sizeof(g_specBuf));
    memset(g_specPeak, 0, sizeof(g_specPeak));

    uint32_t sweepCount = 0;
    uint32_t lastSend   = 0;

    // İkinci modülü güç tasarrufu için kapatalım
    if (jam2.isChipConnected()) jam2.powerDown();

    while (true) {
        // Tek modülle 0'dan 125'e kadar tüm kanalları sırayla tarıyoruz
        for (uint8_t ch = 0; ch <= 125; ch++) {
            uint8_t hits = measureRSSI(r, ch);
            // 0-8 hit -> 0-255 değerine normalize et
            uint8_t val = hits * 32;
            g_specBuf[ch] = val;
            
            // Peak hold
            if (val > g_specPeak[ch]) g_specPeak[ch] = val;
        }
        sweepCount++;

        // Her 500ms'de bir UART üzerinden gönder
        if (millis() - lastSend > 500) {
            lastSend = millis();

            // Anlık spektrum verisi
            String out = "SPEC:";
            for (uint8_t ch = 0; ch <= 125; ch++) {
                out += String(g_specBuf[ch]);
                if (ch < 125) out += ",";
            }
            out += "\n";
            bleUartSend(out);

            // Peak hold verisi
            String peak = "PEAK:";
            for (uint8_t ch = 0; ch <= 125; ch++) {
                peak += String(g_specPeak[ch]);
                if (ch < 125) peak += ",";
            }
            peak += "\n";
            bleUartSend(peak);

            bleUartSend("SPEC:SWEEP=" + String(sweepCount) + "\n");
        }

        // Peak sıfırlama veya durdurma komutlarını kontrol et
        String cmd = bleGetCmd();
        if (cmd == "STOP") break;
        if (cmd == "PEAK_RST") {
            memset(g_specPeak, 0, sizeof(g_specPeak));
            bleUartSend("SPEC:PEAK_RESET\n");
        }

        vTaskDelay(1);
    }

    // Çıkışta tüm radyoları kapat
    jam1.powerDown();
    bleUartSend("SPEC:STOP:SWEEPS=" + String(sweepCount) + "\n");
}

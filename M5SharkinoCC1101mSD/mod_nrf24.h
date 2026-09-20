#pragma once
// ═══════════════════════════════════════════════════════════
//  mod_nrf24.h — ESP32-S3 DevKitC / M5Sharkino
//  nRF24 Slave (ESP32-S3 SuperMini) ile BLE Nordic UART haberleşme
//
//  DevKitC → SuperMini:  "JAM\n" / "SCAN\n" / "SPEC\n"
//                        "MITM_P\n" / "MITM_A\n" / "STOP\n"
//                        "INJ:AABBCC...\n"
//  SuperMini → DevKitC:  "JAM:OK\n" / "JAM:CNT:500\n"
//                        "DRONE:DJI:52:180\n"
//                        "SPEC:0,1,255,...\n"
//                        "MITM:P:DEV:AABB...:LOGI_KEY_ENC\n"
//                        "MITM:A:INTERCEPT:...\n"
//                        "HELLO:nRF24-Slave:v1.0\n"
// ═══════════════════════════════════════════════════════════

// ── nRF24 Slave BLE bağlantı durumu ──────────────────────
static bool     g_nrf24SlaveConnected = false;
static uint32_t g_nrf24LastAlive      = 0;
static String   g_nrf24SlaveRxBuf     = "";
static bool     g_nrf24SlaveRxReady   = false;
static String   g_nrf24SlaveLastMsg   = "";

// Spectrum & Peak buffer (125 kanal)
static uint8_t g_nrf24SpecBuf[126]  = {0};
static uint8_t g_nrf24PeakBuf[126]  = {0};
static bool    g_nrf24SpecReady     = false;

// Drone tespit buffer
struct DroneHit {
    char    protocol[12];
    uint8_t channel;
    uint8_t rssi;
    uint32_t timestamp;
};
static DroneHit g_droneHits[16];
static uint8_t  g_droneHitCount = 0;

// MITM paket buffer
struct MitmLog {
    String  direction;  // "DEV" / "RCV" / "INTERCEPT" / "RELAY"
    String  hex;
    String  decoded;
    uint32_t ts;
};
static MitmLog  g_mitmLog[32];
static uint8_t  g_mitmLogHead  = 0;
static uint8_t  g_mitmLogCount = 0;

// ── nRF24 Slave'e komut gönder (Hardware UART Serial2) ───────
// DevKitC TX=GPIO43 → SuperMini RX
// DevKitC RX=GPIO44 ← SuperMini TX
static void nrf24UartInit() {
    Serial2.begin(115200, SERIAL_8N1, 16, 15); // RX=GPIO16, TX=GPIO15
    Serial.println("[UART2] nRF24 Slave haberleşme başlatıldı");
}

static void nrf24SendCmd(const String& cmd) {
    Serial2.println(cmd);
}

// ── Gelen mesajı parse et ─────────────────────────────────
static void nrf24ParseMsg(const String& msg) {
    // ALIVE: heartbeat
    if (msg.startsWith("ALIVE:")) {
        g_nrf24LastAlive = millis();
        g_nrf24SlaveConnected = true;
        return;
    }

    // JAM:HOT:N — Reaktif jam sıcak kanal tarama sonucu
    if (msg.startsWith("JAM:HOT:")) {
        uint8_t n = msg.substring(8).toInt();
        logPush("[REACT] Sicak kanal: " + String(n));
        dispBanner("Hot: " + String(n) + " kanal", 600);
        return;
    }

    // JAM:REACT: — Reaktif jamming durum bildirimleri
    if (msg.startsWith("JAM:REACT:")) {
        String state = msg.substring(10);
        if (state.startsWith("START:")) {
            uint8_t n = state.substring(6).toInt();
            logPush("[REACT] Basliyor: " + String(n) + " hot kanal");
        } else if (state == "DONE") {
            logPush("[REACT] Tamamlandi");
            dispBannerOK("React tamamlandi");
        } else if (state == "STOP") {
            logPush("[REACT] Durduruldu");
        } else if (state.startsWith("ERR:")) {
            logPush("[REACT] Hata: " + state.substring(4));
            dispBannerErr("React: " + state.substring(4));
        } else {
            logPush("[REACT] " + state);
        }
        return;
    }

    // DWELL:OK:mod:us — Dwell time onayı
    if (msg.startsWith("DWELL:OK:")) {
        logPush("[DWELL] " + msg.substring(9));
        return;
    }

    // JAM spesifik loglar (catch-all)
    if (msg.startsWith("JAM:STAT:") || msg.startsWith("JAM:MODE:") ||
        msg.startsWith("JAM:BT:HOP=") || msg.startsWith("JAM:WIFI_FULL:") ||
        msg.startsWith("JAM:BLE_FULL:") || msg.startsWith("JAM:")) {
        logPush("[nRF24] " + msg);
        return;
    }

    // SPEC:SWEEP=
    if (msg.startsWith("SPEC:SWEEP=")) {
        logPush("[nRF24] " + msg);
        return;
    }

    // DRONE:DJI:52:180
    if (msg.startsWith("DRONE:")) {
        // parse: DRONE:protokol:kanal:rssi
        int c1 = msg.indexOf(':', 6);
        int c2 = msg.indexOf(':', c1 + 1);
        int c3 = msg.indexOf(':', c2 + 1);
        if (c1 > 0 && c2 > 0) {
            String proto = msg.substring(6, c1);
            uint8_t ch   = msg.substring(c1+1, c2).toInt();
            uint8_t rssi = (c3 > 0) ? msg.substring(c2+1, c3).toInt() : 0;

            if (g_droneHitCount < 16) {
                DroneHit& h = g_droneHits[g_droneHitCount++];
                strncpy(h.protocol, proto.c_str(), 11);
                h.channel   = ch;
                h.rssi      = rssi;
                h.timestamp = millis();
            }
            logPush("[DRONE] " + proto + " CH" + String(ch) +
                    " RSSI=" + String(rssi));
        }
        return;
    }

    // SPEC:0,1,255,...
    if (msg.startsWith("SPEC:") && !msg.startsWith("SPEC:SWEEP=")) {
        String vals = msg.substring(5);
        uint8_t idx = 0;
        int start = 0;
        while (idx <= 125) {
            int comma = vals.indexOf(',', start);
            String v = (comma >= 0) ? vals.substring(start, comma)
                                    : vals.substring(start);
            g_nrf24SpecBuf[idx++] = (uint8_t)v.toInt();
            if (comma < 0) break;
            start = comma + 1;
        }
        g_nrf24SpecReady = true;
        return;
    }

    // PEAK:0,1,255,...
    if (msg.startsWith("PEAK:")) {
        String vals = msg.substring(5);
        uint8_t idx = 0;
        int start = 0;
        while (idx <= 125) {
            int comma = vals.indexOf(',', start);
            String v = (comma >= 0) ? vals.substring(start, comma)
                                    : vals.substring(start);
            g_nrf24PeakBuf[idx++] = (uint8_t)v.toInt();
            if (comma < 0) break;
            start = comma + 1;
        }
        return;
    }

    // MITM:P:DEV:AABB...:LOGI_KEY_ENC
    if (msg.startsWith("MITM:")) {
        MitmLog& l = g_mitmLog[g_mitmLogHead];
        int c1 = msg.indexOf(':', 5);
        int c2 = (c1 > 0) ? msg.indexOf(':', c1+1) : -1;
        int c3 = (c2 > 0) ? msg.indexOf(':', c2+1) : -1;
        l.direction = (c1 > 0) ? msg.substring(5, c1) : msg;
        l.hex       = (c2 > 0 && c3 > 0) ? msg.substring(c2+1, c3) : "";
        l.decoded   = (c3 > 0) ? msg.substring(c3+1) : "";
        l.ts        = millis();
        g_mitmLogHead  = (g_mitmLogHead + 1) % 32;
        g_mitmLogCount = min((int)g_mitmLogCount + 1, 32);
        logPush("[MITM] " + msg.substring(5));
        return;
    }

    // HELLO
    if (msg.startsWith("HELLO:")) {
        g_nrf24LastAlive = millis();
        g_nrf24SlaveConnected = true;
        logPush("[nRF24] Slave bağlandı: " + msg.substring(6));
        return;
    }

    // PONG — Ping'e cevap
    if (msg.startsWith("PONG:") || msg == "PONG") {
        g_nrf24LastAlive = millis();
        g_nrf24SlaveConnected = true;
        logPush("[nRF24] PONG alındı");
        return;
    }

    // ALIVE heartbeat (zaten yukarıda var ama buraya da ekle)
    if (msg.startsWith("STATUS:")) {
        logPush("[nRF24] " + msg);
        return;
    }
}

// ── UART RX — Serial2'den gelen verileri oku ve parse et ──
// nrf24Poll() her menü döngüsünde çağrılır
static String g_nrf24UartBuf = "";

static void nrf24Poll() {
    // 15 saniye boyunca ALIVE gelmezse bağlantıyı kopuk say
    if (g_nrf24LastAlive > 0 && (millis() - g_nrf24LastAlive > 15000)) {
        g_nrf24SlaveConnected = false;
    }

    while (Serial2.available()) {
        char c = (char)Serial2.read();
        if (c == '\n') {
            g_nrf24UartBuf.trim();
            if (g_nrf24UartBuf.length() > 0) {
                nrf24ParseMsg(g_nrf24UartBuf);
            }
            g_nrf24UartBuf = "";
        } else {
            g_nrf24UartBuf += c;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SPECTRUM GÖRSELLEŞTIRME — OLED bar chart (Peak Hold Destekli)
// ═══════════════════════════════════════════════════════════
static void nrf24DrawSpectrum() {
    dispClear();
    dispStatusBar("2.4GHz Spektrum");

    // 126 kanal → 128 pixel genişlikte sıkıştır
    for (uint8_t ch = 0; ch <= 125; ch++) {
        uint8_t x   = map(ch, 0, 125, 0, 127);
        uint8_t val = g_nrf24SpecBuf[ch];
        uint8_t h   = map(val, 0, 255, 0, 38);  // anlık bar
        if (h > 0) {
            display.drawFastVLine(x, 52 - h, h, SSD1306_WHITE);
        }
        uint8_t pval = g_nrf24PeakBuf[ch];
        uint8_t ph   = map(pval, 0, 255, 0, 38);  // peak bar
        if (ph > 0) {
            display.drawPixel(x, 52 - ph, SSD1306_WHITE);
        }
    }

    // BLE kanallarını işaretle
    display.drawFastVLine(1,  10, 44, SSD1306_WHITE);  // BLE CH37
    display.drawFastVLine(25, 10, 44, SSD1306_WHITE);  // BLE CH38
    display.drawFastVLine(80, 10, 44, SSD1306_WHITE);  // BLE CH39

    // WiFi kanallarını işaretle
    display.drawPixel(10, 11, SSD1306_WHITE);  // WiFi CH1
    display.drawPixel(36, 11, SSD1306_WHITE);  // WiFi CH6
    display.drawPixel(61, 11, SSD1306_WHITE);  // WiFi CH11

    dispText(0, 54, "Anlik:Bar Peak:Nokta");
    dispCommit();
}

// ═══════════════════════════════════════════════════════════
//  JAMMER MODU
// ═══════════════════════════════════════════════════════════
static void nrf24JammerRun(uint8_t mode = 1) {
    const char* modeNames[] = {
        "",
        "Genel Tarayici",
        "BLE Kesici",
        "WiFi Kesici",
        "Drone Kesici",
        "Kaotik Tarayici",
        "Dalga Kesici",
        "Bluetooth Kesici"
    };

    const char* title = (mode >= 1 && mode <= 7) ? modeNames[mode] : "nRF24 Jammer";
    dispClear(); dispStatusBar(title);
    dispText(0, 14, "4x nRF24L01+ Interleaved");
    if (mode <= 3) {
        dispText(0, 26, "ESP32 WiFi TX: ACIK");
        dispText(0, 38, "BLE DoS + Flood: ACIK");
    } else {
        dispText(0, 26, "ESP32 WiFi TX: KAPALI");
        dispText(0, 38, "BLE Flood: KAPALI");
    }
    dispText(0, 50, "Baslatiliyor...");
    dispCommit();
    delay(800);

    g_bleDosPktCount = 0;

    // Slave'e JAM komutu gönder
    nrf24SendCmd("JAM:" + String(mode));

    // TERMAL KORUMA: Master kart (N16R8) akım çekimini azaltmak için kendi Wi-Fi ve BLE'sini kapatır.
    // Tüm karıştırma işlemi sadece Slave (SuperMini) nRF24 modülleri tarafından gerçekleştirilir.
    wifiTxStop();
    rawBleDeinit();

    uint32_t lastDraw = 0;

    while (!readButtonLong()) {
        nrf24Poll();

        if (millis() - lastDraw > 300) {
            lastDraw = millis();
            dispClear(); dispStatusBar(title);
            if (mode == 7) {
                dispText(0, 10, "nRF24: 4x CH2-80 (BT)");
            } else if (mode == 2) {
                dispText(0, 10, "nRF24: BLE Adv+Data");
            } else if (mode == 3) {
                dispText(0, 10, "nRF24: WiFi CH1-13");
            } else {
                dispText(0, 10, "nRF24: 4x CH0-83");
            }

            dispText(0, 22, "N16R8 RF: PASIF (KORUMA)");
            dispText(0, 34, "Güç: Sadece SuperMini");
            dispText(0, 46, "Isınma Engellendi ✓");
            dispText(0, 54, "[LONG] dur");
            dispCommit();
        }

        vTaskDelay(1);
    }

    // Durdur
    nrf24SendCmd("STOP");
    if (mode <= 3) {
        wifiTxStop();
        rawBleDeinit();
        BLEDevice::init("M5Sharkino");
    }

    logPush("[nRF24_JAM] dos_pkts=" + String(g_bleDosPktCount));

    dispBannerOK("Jammer durduruldu");
}


// ═══════════════════════════════════════════════════════════
//  DRONE SCANNER MODU
// ═══════════════════════════════════════════════════════════
static void nrf24DroneScanner() {
    g_droneHitCount = 0;
    nrf24SendCmd("SCAN");

    uint32_t lastDraw = 0;

    while (!readButtonLong()) {
        nrf24Poll();

        if (millis() - lastDraw > 300) {
            lastDraw = millis();
            dispClear(); dispStatusBar("Drone Scanner");
            dispText(0, 10, "4x nRF24 tarama...");
            dispText(0, 22, "Tespitler: " + String(g_droneHitCount));

            // Son 3 tespiti göster
            uint8_t start = (g_droneHitCount > 3) ? g_droneHitCount - 3 : 0;
            for (uint8_t i = start; i < g_droneHitCount && i < start + 3; i++) {
                DroneHit& h = g_droneHits[i];
                dispText(0, 32 + (i - start) * 10,
                    String(h.protocol) + " CH" + String(h.channel) +
                    " R:" + String(h.rssi));
            }

            dispText(0, 54, "[LONG] dur");
            dispCommit();
        }

        vTaskDelay(10);
    }

    nrf24SendCmd("STOP");
    dispBannerOK("Tespit: " + String(g_droneHitCount));
}

// ═══════════════════════════════════════════════════════════
//  SPECTRUM ANALYZER MODU
// ═══════════════════════════════════════════════════════════
static void nrf24SpectrumRun() {
    nrf24SendCmd("SPEC");

    while (!readButtonLong()) {
        nrf24Poll();
        if (g_nrf24SpecReady) {
            g_nrf24SpecReady = false;
            nrf24DrawSpectrum();
        }
        vTaskDelay(10);
    }

    nrf24SendCmd("STOP");
    dispBannerOK("Spektrum durduruldu");
}

// ═══════════════════════════════════════════════════════════
//  MITM MODU
// ═══════════════════════════════════════════════════════════
static void nrf24MitmRun(bool active) {
    g_mitmLogCount = 0;
    g_mitmLogHead  = 0;
    nrf24SendCmd(active ? "MITM_A" : "MITM_P");

    uint32_t lastDraw = 0;

    while (!readButtonLong()) {
        nrf24Poll();

        if (millis() - lastDraw > 300) {
            lastDraw = millis();
            dispClear();
            dispStatusBar(active ? "MITM Aktif" : "MITM Pasif");
            dispText(0, 10, "Paketler: " + String(g_mitmLogCount));

            // Son 4 paketi göster
            uint8_t total = min((int)g_mitmLogCount, 4);
            for (uint8_t i = 0; i < total; i++) {
                uint8_t idx = (g_mitmLogHead - total + i + 32) % 32;
                MitmLog& l = g_mitmLog[idx];
                String line = l.direction + ":" +
                    l.hex.substring(0, 6) +
                    (l.decoded.length() > 0 ? ">" + l.decoded.substring(0, 6) : "");
                dispText(0, 20 + i * 10, line);
            }

            dispText(0, 54, "[LONG] dur");
            dispCommit();
        }

        vTaskDelay(10);
    }

    nrf24SendCmd("STOP");
    dispBannerOK("MITM durduruldu | " + String(g_mitmLogCount) + " pkt");
}

// ═══════════════════════════════════════════════════════════
//  ANA MENÜ
// ═══════════════════════════════════════════════════════════
static void nrf24_menu_run() {
    OptionList opts = {
        {"Genel Tarayici",   []() { nrf24JammerRun(1);        }},
        {"BLE Kesici",       []() { nrf24JammerRun(2);        }},
        {"WiFi Kesici",      []() { nrf24JammerRun(3);        }},
        {"Drone Kesici",     []() { nrf24JammerRun(4);        }},
        {"Kaotik Tarayici",  []() { nrf24JammerRun(5);        }},
        {"Dalga Kesici",     []() { nrf24JammerRun(6);        }},
        {"Bluetooth Kesici", []() { nrf24JammerRun(7);        }},
        {"Drone Scanner",    []() { nrf24DroneScanner();      }},
        {"Spektrum",         []() { nrf24SpectrumRun();       }},
        {"Peak Sifirla",     []() {
            nrf24SendCmd("PEAK_RST");
            memset(g_nrf24PeakBuf, 0, sizeof(g_nrf24PeakBuf));
            dispBanner("Peak sifirlandi", 1000);
        }},
        {"MITM Pasif",       []() { nrf24MitmRun(false);      }},
        {"MITM Aktif",       []() { nrf24MitmRun(true);       }},
        {"Reaktif Jam",      []() {
            nrf24SendCmd("REACT");
            dispClear();
            dispStatusBar("Reaktif Jam");
            dispText(0, 14, "Hot kanal taranıyor...");
            dispText(0, 26, "Slave cevabı bekleniyor");
            dispText(0, 50, "[LONG] dur");
            dispCommit();
            // Slave tamamlandığında nrf24Poll cevabı yakalayacak
        }},
        {"Dwell Ayarla",     []() {
            // Mod seç (1-7) → µs gir
            OptionList dwOpts = {
                {"Mod 1 BandSweep",  []() { nrf24SendCmd("DWELL:1:200"); }},
                {"Mod 2 BLE",        []() { nrf24SendCmd("DWELL:2:500"); }},
                {"Mod 3 WiFi",       []() { nrf24SendCmd("DWELL:3:300"); }},
                {"Mod 4 Drone",      []() { nrf24SendCmd("DWELL:4:150"); }},
                {"Mod 5 Chaos Rnd",  []() { nrf24SendCmd("DWELL:5:0");   }},
                {"Mod 5 Chaos 200",  []() { nrf24SendCmd("DWELL:5:200"); }},
                {"Mod 6 CW 130",     []() { nrf24SendCmd("DWELL:6:130"); }},
                {"Mod 6 CW 50",      []() { nrf24SendCmd("DWELL:6:50");  }},
                {"Mod 7 BT 130",     []() { nrf24SendCmd("DWELL:7:130"); }},
                {"Mod 7 BT 65",      []() { nrf24SendCmd("DWELL:7:65");  }},
                {"Back",             []() {}},
            };
            int ds = loopOptions(dwOpts, "Dwell Ayarla");
            if (ds >= 0 && ds < 10) {
                dwOpts[ds].action();
                delay(300);
                nrf24Poll();
            }
        }},
        {"Ping Slave",       []() {
            nrf24SendCmd("PING");
            delay(500);
            nrf24Poll();
            dispBanner(g_nrf24SlaveConnected ? "PONG alindi!" : "Cevap yok", 1000);
        }},
        {"Back",             []() {}},
    };

    // Menü açılınca otomatik ping — kullanıcı manuel ping yapmak zorunda kalmasın
    nrf24SendCmd("PING");
    delay(600);
    nrf24Poll();

    while (true) {
        // Slave durumunu başlıkta göster
        int sel = loopOptions(opts, "nRF24 | " +
                              String(g_nrf24SlaveConnected ? "BAGLI" : "YOK"));
        if (sel < 0 || sel == 15) break;  // 15 = Back (2 yeni item eklendi)

        // Merkezi bağlantı kontrolü ("Ping Slave" ve "Back" hariç)
        if (!g_nrf24SlaveConnected && sel != 14) {
            dispBannerErr("Slave bagli degil!", 1500);
            continue;
        }

        opts[sel].action();
        nrf24Poll();
    }
}

// ── nRF24 menü ikonu sınıfı ───────────────────────────────
class NRF24Menu : public MenuItem {
public:
    String getName() override { return "nRF24"; }
    void drawIcon(int x, int y) override {
        // Anten sembolü
        display.drawFastVLine(x+16, y+8,  16, SSD1306_WHITE);
        display.drawFastHLine(x+8,  y+8,  16, SSD1306_WHITE);
        display.drawCircleHelper(x+16, y+8, 8,  0b1100, SSD1306_WHITE);
        display.drawCircleHelper(x+16, y+8, 12, 0b1100, SSD1306_WHITE);
        display.fillRect(x+14, y+24, 4, 6, SSD1306_WHITE);
    }
    void drawIconInv(int x, int y) override {
        display.fillRoundRect(x, y, 32, 32, 4, SSD1306_WHITE);
        display.drawFastVLine(x+16, y+8,  16, SSD1306_BLACK);
        display.drawFastHLine(x+8,  y+8,  16, SSD1306_BLACK);
        display.drawCircleHelper(x+16, y+8, 8,  0b1100, SSD1306_BLACK);
        display.drawCircleHelper(x+16, y+8, 12, 0b1100, SSD1306_BLACK);
        display.fillRect(x+14, y+24, 4, 6, SSD1306_BLACK);
    }
    void drawIconSmall(int x, int y, uint16_t col) override {
        display.drawFastVLine(x+8, y+2, 10, col);
        display.drawCircleHelper(x+8, y+4, 4, 0b1100, col);
        display.drawCircleHelper(x+8, y+4, 7, 0b1100, col);
        display.fillRect(x+7, y+12, 3, 4, col);
    }
    void runMenu() override { nrf24_menu_run(); }
};

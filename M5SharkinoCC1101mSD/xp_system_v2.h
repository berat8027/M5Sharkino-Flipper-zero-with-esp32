// ═══════════════════════════════════════════════════════════════════════
//  XP / LEVEL SİSTEMİ v2.0 — Flipper Zero Dolphin birebir uyumlu
//  Made by Beroxx — M5Sharkino v2
//
//  Flipper Zero orijinal XP eşikleri:
//    Her level için: xp = level * level * 100
//    Max level: 30 (Flipper Zero'nun aynen aynısı)
//
//  Aktivite tabanlı XP ödülleri (Flipper Zero Deed sistemi):
//    Basit okuma/tarama    → 5-10 XP
//    Saldırı/yakalama      → 15-25 XP
//    Analiz/decode         → 10-20 XP
//    Klonlama/kaydetme     → 20 XP
//    Oyun kazanma          → 25 XP
//    Özel/nadir başarım    → 50 XP
//
//  NVS namespace: "xpdata"
//  Yeni anahtarlar: irCnt, badusbCnt, handshakeCnt, pmkidCnt,
//                   rollingCnt, mitmCnt, analysisxCnt, clonetCnt,
//                   portsCnt, beaconCnt, nfcCnt, subReplayCnt
// ═══════════════════════════════════════════════════════════════════════

// ─── Flipper Zero Birebir XP Sabitleri ────────────────────────────────
static constexpr uint8_t  XP_MAX_LEVEL       = 30;

// Basit taramalar (Flipper: "Scan" deeds)
static constexpr uint16_t XP_PER_RFID        = 10;   // RFID/NFC kart okuma
static constexpr uint16_t XP_PER_WIFI_SCAN   = 5;    // WiFi ağ tarama
static constexpr uint16_t XP_PER_BLE_SCAN    = 5;    // BLE cihaz tarama
static constexpr uint16_t XP_PER_IR_CAP      = 10;   // IR sinyal yakalama
static constexpr uint16_t XP_PER_RF_CAP      = 15;   // Sub-GHz RF yakalama

// Aktif saldırılar (Flipper: "Attack" deeds — yüksek XP)
static constexpr uint16_t XP_PER_DEAUTH      = 5;    // Deauth paketi gönderildi
static constexpr uint16_t XP_PER_RF_REPLY    = 12;   // RF replay/tekrar gönder
static constexpr uint16_t XP_PER_IR_SEND     = 8;    // IR sinyal gönderildi
static constexpr uint16_t XP_PER_IR_SPAM     = 15;   // IR TV spam / brute force
static constexpr uint16_t XP_PER_BEACON_SPAM = 10;   // Beacon spam çalıştırıldı
static constexpr uint16_t XP_PER_BADUSB      = 20;   // BadUSB payload çalıştırıldı
static constexpr uint16_t XP_PER_BLE_SPAM    = 8;    // BLE reklam spam
static constexpr uint16_t XP_PER_BLE_DOS     = 12;   // BLE DoS saldırısı
static constexpr uint16_t XP_PER_BLE_MITM    = 25;   // BLE MITM oturumu
static constexpr uint16_t XP_PER_WIFI_MITM   = 25;   // WiFi evil portal / karma
static constexpr uint16_t XP_PER_PROBE_SNIFF = 8;    // WiFi probe sniffer

// Analiz / veri toplama (Flipper: "Research" deeds)
static constexpr uint16_t XP_PER_HANDSHAKE   = 30;   // WPA handshake yakalandı
static constexpr uint16_t XP_PER_PMKID       = 30;   // PMKID yakalandı
static constexpr uint16_t XP_PER_ROLLINGCODE = 20;   // Rolling code analizi
static constexpr uint16_t XP_PER_SMARTMETER  = 15;   // Akıllı sayaç oku
static constexpr uint16_t XP_PER_POCSAG      = 20;   // POCSAG pager decode
static constexpr uint16_t XP_PER_IR_ANALYZE  = 15;   // IR sinyal analiz
static constexpr uint16_t XP_PER_BLE_GATT    = 12;   // BLE GATT servis tarama
static constexpr uint16_t XP_PER_PORTSCAN    = 15;   // Port tarama tamamlandı
static constexpr uint16_t XP_PER_ANALYSIS    = 10;   // Genel analiz/istatistik
static constexpr uint16_t XP_PER_DICTATTACK  = 15;   // Sözlük saldırısı denendi

// Kaydetme / klonlama (Flipper: "Clone" deeds)
static constexpr uint16_t XP_PER_RFID_CLONE  = 20;   // RFID kart klonlandı
static constexpr uint16_t XP_PER_NFC_SAVE    = 15;   // NFC dump kaydedildi
static constexpr uint16_t XP_PER_IR_SAVE     = 10;   // IR sinyali SD'ye kaydedildi
static constexpr uint16_t XP_PER_SUB_SAVE    = 15;   // .sub dosyası kaydedildi
static constexpr uint16_t XP_PER_HANDSHAKE_SAVE = 35; // Handshake SD'ye kaydedildi

// Oyunlar (Flipper: "Game" deeds)
static constexpr uint16_t XP_PER_GAME_WIN    = 25;   // Oyun kazanıldı
static constexpr uint16_t XP_PER_GAME_PLAY   = 3;    // Oyun oynandı (kaybedilse bile)

// Sistem olayları
static constexpr uint16_t XP_PER_SESSION     = 3;    // Her boot / oturum açılışı
static constexpr uint16_t XP_PER_OTA         = 50;   // OTA güncelleme başarılı

// ─── Runtime XP Durumu ────────────────────────────────────────────────
static uint32_t g_xp             = 0;
uint8_t  g_level          = 1;   // extern olarak da bildirildi

// Temel aktivite sayaçları (eski — NVS uyumu için korundu)
static uint32_t g_xpRfidCnt      = 0;
static uint32_t g_xpWifiCnt      = 0;
static uint32_t g_xpBleCnt       = 0;
static uint32_t g_xpRfCnt        = 0;
static uint32_t g_xpGameCnt      = 0;
static uint32_t g_xpSessionCnt   = 0;

// Yeni genişletilmiş sayaçlar
static uint32_t g_xpIrCnt        = 0;   // IR yakalama
static uint32_t g_xpIrSendCnt    = 0;   // IR gönderme
static uint32_t g_xpBadusbCnt    = 0;   // BadUSB payload
static uint32_t g_xpHandshakeCnt = 0;   // WPA handshake
static uint32_t g_xpPmkidCnt     = 0;   // PMKID yakalama
static uint32_t g_xpRollingCnt   = 0;   // Rolling code analiz
static uint32_t g_xpMitmCnt      = 0;   // MITM (BLE+WiFi toplam)
static uint32_t g_xpPortsCnt     = 0;   // Port tarama
static uint32_t g_xpCloneCnt     = 0;   // RFID/NFC klonlama
static uint32_t g_xpSubReplCnt   = 0;   // Sub-GHz replay
static uint32_t g_xpAnalCnt      = 0;   // Analiz işlemleri
static uint32_t g_xpBeaconCnt    = 0;   // Beacon spam
static uint32_t g_xpBleSpamCnt   = 0;   // BLE spam

// ─── Flipper Zero Birebir XP Eşiği Formülü ────────────────────────────
// Orijinal: level * level * 100
static uint32_t xpForLevel(uint8_t lv) {
    if (lv <= 1) return 0;
    return (uint32_t)(lv - 1) * (lv - 1) * 100UL;
}

// Mevcut level'da ilerleme yüzdesi [0-100]
static uint8_t xpLevelPct() {
    if (g_level >= XP_MAX_LEVEL) return 100;
    uint32_t cur  = xpForLevel(g_level);
    uint32_t next = xpForLevel(g_level + 1);
    if (next <= cur) return 100;
    uint32_t span = next - cur;
    uint32_t prog = g_xp - cur;
    return (uint8_t)min(100UL, prog * 100UL / span);
}

// ─── Flipper Zero Birebir Rank İsimleri ───────────────────────────────
// Orijinal Dolphin level isimleri (firmware/applications/services/dolphin)
static const char* xpRankName(uint8_t lv) {
    if (lv <=  1) return "Newbie";
    if (lv <=  3) return "Script Kid";
    if (lv <=  5) return "Tinkerer";
    if (lv <=  8) return "Hacker";
    if (lv <= 11) return "Pro Hacker";
    if (lv <= 14) return "Expert";
    if (lv <= 17) return "Elite";
    if (lv <= 20) return "Cyber Ninja";
    if (lv <= 23) return "Ghost";
    if (lv <= 26) return "Phantom";
    if (lv <= 29) return "Shadow God";
    return "Zero Cool";  // Lv.30 — max
}

// ─── XP Deed İsim Tablosu (Flipper Zero tarzı bildirim) ───────────────
static const char* xpDeedName(uint16_t amount) {
    // Miktar bazlı deed kategorisi
    if (amount >= 50) return "EPIC DEED!";
    if (amount >= 30) return "Great Deed";
    if (amount >= 20) return "Good Deed";
    if (amount >= 10) return "Deed Done";
    return "Xp";
}

// ─── NVS Kaydet ───────────────────────────────────────────────────────
static void xpSave() {
    prefs.begin("xpdata", false);
    prefs.putUInt("xp",           g_xp);
    prefs.putUChar("level",       g_level);
    // Temel sayaçlar
    prefs.putUInt("rfidCnt",      g_xpRfidCnt);
    prefs.putUInt("wifiCnt",      g_xpWifiCnt);
    prefs.putUInt("bleCnt",       g_xpBleCnt);
    prefs.putUInt("rfCnt",        g_xpRfCnt);
    prefs.putUInt("gameCnt",      g_xpGameCnt);
    prefs.putUInt("sessionCnt",   g_xpSessionCnt);
    // Genişletilmiş sayaçlar
    prefs.putUInt("irCnt",        g_xpIrCnt);
    prefs.putUInt("irSendCnt",    g_xpIrSendCnt);
    prefs.putUInt("badusbCnt",    g_xpBadusbCnt);
    prefs.putUInt("hsCnt",        g_xpHandshakeCnt);
    prefs.putUInt("pmkidCnt",     g_xpPmkidCnt);
    prefs.putUInt("rollingCnt",   g_xpRollingCnt);
    prefs.putUInt("mitmCnt",      g_xpMitmCnt);
    prefs.putUInt("portsCnt",     g_xpPortsCnt);
    prefs.putUInt("cloneCnt",     g_xpCloneCnt);
    prefs.putUInt("subReplCnt",   g_xpSubReplCnt);
    prefs.putUInt("analCnt",      g_xpAnalCnt);
    prefs.putUInt("beaconCnt",    g_xpBeaconCnt);
    prefs.putUInt("bleSpamCnt",   g_xpBleSpamCnt);
    prefs.end();
}

// ─── NVS Yükle ────────────────────────────────────────────────────────
static void xpLoad() {
    prefs.begin("xpdata", true);
    g_xp              = prefs.getUInt("xp",          0);
    g_level           = prefs.getUChar("level",       1);
    g_xpRfidCnt       = prefs.getUInt("rfidCnt",      0);
    g_xpWifiCnt       = prefs.getUInt("wifiCnt",      0);
    g_xpBleCnt        = prefs.getUInt("bleCnt",       0);
    g_xpRfCnt         = prefs.getUInt("rfCnt",        0);
    g_xpGameCnt       = prefs.getUInt("gameCnt",      0);
    g_xpSessionCnt    = prefs.getUInt("sessionCnt",   0);
    g_xpIrCnt         = prefs.getUInt("irCnt",        0);
    g_xpIrSendCnt     = prefs.getUInt("irSendCnt",    0);
    g_xpBadusbCnt     = prefs.getUInt("badusbCnt",    0);
    g_xpHandshakeCnt  = prefs.getUInt("hsCnt",        0);
    g_xpPmkidCnt      = prefs.getUInt("pmkidCnt",     0);
    g_xpRollingCnt    = prefs.getUInt("rollingCnt",   0);
    g_xpMitmCnt       = prefs.getUInt("mitmCnt",      0);
    g_xpPortsCnt      = prefs.getUInt("portsCnt",     0);
    g_xpCloneCnt      = prefs.getUInt("cloneCnt",     0);
    g_xpSubReplCnt    = prefs.getUInt("subReplCnt",   0);
    g_xpAnalCnt       = prefs.getUInt("analCnt",      0);
    g_xpBeaconCnt     = prefs.getUInt("beaconCnt",    0);
    g_xpBleSpamCnt    = prefs.getUInt("bleSpamCnt",   0);
    prefs.end();
    // Sınır ve tutarsızlık düzelt
    if (g_level < 1)            g_level = 1;
    if (g_level > XP_MAX_LEVEL) g_level = XP_MAX_LEVEL;
    if (g_xp < xpForLevel(g_level)) g_xp = xpForLevel(g_level);
}

// ─── Flipper Zero "+XP" Floating Animasyonu ───────────────────────────
// Ekranın sağ alt köşesinde "+15 XP" yazısı yukarı doğru yükselir
// ve kaybolur — tam Flipper Zero deed bildirimi gibi
static void xpFloatAnim(uint16_t amount) {
    // Mevcut ekran içeriğini bozmadan overlay çizer
    // 5 kare: y=56 → 36, her karede +0.5 opacity (fade via invert trick yok,
    // tek color OLED'de: ilk 2 karede kalın, son 3 karede ince)
    String txt = "+" + String(amount) + " XP";
    uint8_t tw = txt.length() * 6;
    int16_t tx = OLED_W - tw - 2;

    for (int16_t y = 56; y >= 38; y -= 3) {
        // Önceki XP yazısının bölgesini sil (sadece o bant)
        display.fillRect(tx - 1, y + 3, tw + 2, 11, SSD1306_BLACK);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(tx, y);
        display.print(txt.c_str());
        display.display();
        delay(35);
    }
    // Son izi temizle
    display.fillRect(tx - 1, 35, tw + 2, 11, SSD1306_BLACK);
    display.display();
}

// ─── Level-Up Kutlama Animasyonu — Flipper Zero Gerçek Tarzı ──────────
static void xpLevelUpAnim(uint8_t newLevel) {
    buzzOK();
    // Flipper Zero gerçek Level-Up animasyonu (Levelup1 veya Levelup2)
    uint8_t luAnim = (newLevel % 2 == 0) ? 2 : 1;
    playLevelUpAnim(luAnim);
    // Flipper imzası: 3 hızlı invert flash
    for (uint8_t f = 0; f < 3; f++) {
        display.invertDisplay(true);  delay(70);
        display.invertDisplay(false); delay(50);
    }
    String lvStr = "Lv." + String(newLevel);
    String rank  = xpRankName(newLevel);
    // Slide-in animasyonu (yukarıdan iner)
    for (int16_t y = -50; y <= 8; y += 5) {
        dispClear();
        display.fillRoundRect(4, y, OLED_W - 8, 50, 4, SSD1306_WHITE);
        display.fillRect(4, y, OLED_W - 8, 11, SSD1306_BLACK);
        display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
        int16_t hx = (OLED_W - 9 * 6) / 2;
        display.setCursor(hx, y + 2); display.print("LEVEL  UP!");
        display.setTextColor(SSD1306_BLACK); display.setTextSize(2);
        int16_t lw = lvStr.length() * 12;
        display.setCursor((OLED_W - lw) / 2, y + 14); display.print(lvStr.c_str());
        display.setTextSize(1);
        int16_t rw = rank.length() * 6;
        display.setCursor((OLED_W - rw) / 2, y + 33); display.print(rank);
        // XP çubuğu
        uint8_t pct = xpLevelPct();
        display.drawRect(8, y + 44, OLED_W - 16, 6, SSD1306_BLACK);
        uint8_t bw = (uint8_t)((uint16_t)(OLED_W - 18) * pct / 100);
        if (bw) display.fillRect(9, y + 45, bw, 4, SSD1306_BLACK);
        display.setTextColor(SSD1306_WHITE);
        dispCommit(); delay(15);
    }
    delay(2000);
    // Slide-out (aşağıya kayar)
    for (int16_t y = 8; y <= OLED_H + 10; y += 7) {
        dispClear();
        display.fillRoundRect(4, y, OLED_W - 8, 50, 4, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK); display.setTextSize(2);
        int16_t lw = lvStr.length() * 12;
        display.setCursor((OLED_W - lw) / 2, y + 14); display.print(lvStr.c_str());
        display.setTextColor(SSD1306_WHITE); dispCommit(); delay(12);
    }
    dispClear(); dispCommit();
}

// ─── xpAdd() — Ana XP Ekleme Fonksiyonu (v2) ──────────────────────────
// Flipper Zero tarzı: floating animasyon + level-up kontrolü
static void xpAdd(uint16_t amount) {
    if (g_level >= XP_MAX_LEVEL) return;
    uint8_t oldLevel = g_level;
    g_xp += amount;
    // Level-up kontrolü (birden fazla level atlama desteği)
    while (g_level < XP_MAX_LEVEL && g_xp >= xpForLevel(g_level + 1)) {
        g_level++;
    }
    xpSave();
    // Floating "+XP" animasyonu (level-up değilse)
    if (g_level == oldLevel) {
        xpFloatAnim(amount);
    } else {
        xpLevelUpAnim(g_level);
    }
    logPush("[XP] +" + String(amount) + " total=" + String(g_xp) + " Lv=" + String(g_level));
}

// ─── xpAddSilent() — Animasyonsuz XP Ekle (boot vb için) ─────────────
static void xpAddSilent(uint16_t amount) {
    if (g_level >= XP_MAX_LEVEL) return;
    g_xp += amount;
    while (g_level < XP_MAX_LEVEL && g_xp >= xpForLevel(g_level + 1)) {
        g_level++;
    }
    xpSave();
}

// ─── XP Sıfırla ───────────────────────────────────────────────────────
static void xpReset() {
    g_xp = 0; g_level = 1;
    g_xpRfidCnt = g_xpWifiCnt = g_xpBleCnt = 0;
    g_xpRfCnt   = g_xpGameCnt = g_xpSessionCnt = 0;
    g_xpIrCnt   = g_xpIrSendCnt = g_xpBadusbCnt = 0;
    g_xpHandshakeCnt = g_xpPmkidCnt = g_xpRollingCnt = 0;
    g_xpMitmCnt = g_xpPortsCnt = g_xpCloneCnt = 0;
    g_xpSubReplCnt = g_xpAnalCnt = g_xpBeaconCnt = g_xpBleSpamCnt = 0;
    xpSave();
}

// ─── Hacker Profile Ekranı v2 — Flipper Zero Passport tarzı ──────────
static void xp_profile_run() {
    uint8_t page = 0;
    while (true) {
        potApplyBrightness();
        uint8_t  pct    = xpLevelPct();
        uint32_t toNext = (g_level < XP_MAX_LEVEL)
                        ? (xpForLevel(g_level + 1) - g_xp) : 0;

        dispClear();
        // ── Sayfa 1: Genel profil ──
        if (page == 0) {
            // Flipper Zero Passport header
            display.fillRect(0, 0, OLED_W, 11, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK); display.setTextSize(1);
            display.setCursor(16, 2); display.print("M5Sharkino Passport");
            display.setTextColor(SSD1306_WHITE);

            // Dolphin animasyonu (küçük — 32x16 bölge sağ üst)
            // Sadece level göster, animasyon ana ekranda
            dispText(0, 13, "Lv." + String(g_level) + "  " + xpRankName(g_level), 1);

            // XP çubuğu — Flipper Zero tarzı ince bar
            uint8_t barW = (uint8_t)((uint16_t)(OLED_W - 2) * pct / 100);
            display.drawRect(0, 22, OLED_W, 5, SSD1306_WHITE);
            if (barW > 0) display.fillRect(1, 23, barW, 3, SSD1306_WHITE);

            // XP rakamları
            char xpBuf[32];
            snprintf(xpBuf, sizeof(xpBuf), "XP: %lu", (unsigned long)g_xp);
            dispText(0, 29, String(xpBuf), 1);
            if (g_level < XP_MAX_LEVEL) {
                snprintf(xpBuf, sizeof(xpBuf), "Next: %lu xp", (unsigned long)toNext);
                dispText(0, 38, String(xpBuf), 1);
            } else {
                dispText(0, 38, "~~~ MAX LEVEL ~~~", 1);
            }

            // Temel istatistikler
            dispText(0, 48, "RFID:" + String(g_xpRfidCnt)
                          + " WiFi:" + String(g_xpWifiCnt)
                          + " RF:" + String(g_xpRfCnt), 1);
            dispText(0, 56, "BLE:"  + String(g_xpBleCnt)
                          + " IR:"  + String(g_xpIrCnt)
                          + " [J]next", 1);

        // ── Sayfa 2: Saldırı istatistikleri ──
        } else if (page == 1) {
            display.fillRect(0, 0, OLED_W, 11, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK); display.setTextSize(1);
            display.setCursor(26, 2); display.print("Attack Stats");
            display.setTextColor(SSD1306_WHITE);

            dispText(0, 13, "Deauth:  " + String(g_xpWifiCnt), 1);
            dispText(0, 21, "BadUSB:  " + String(g_xpBadusbCnt), 1);
            dispText(0, 29, "Handshk: " + String(g_xpHandshakeCnt), 1);
            dispText(0, 37, "PMKID:   " + String(g_xpPmkidCnt), 1);
            dispText(0, 45, "BLE MITM:" + String(g_xpMitmCnt), 1);
            dispText(0, 53, "BeaconSp:" + String(g_xpBeaconCnt) + " [J]next", 1);

        // ── Sayfa 3: Analiz istatistikleri ──
        } else if (page == 2) {
            display.fillRect(0, 0, OLED_W, 11, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK); display.setTextSize(1);
            display.setCursor(22, 2); display.print("Analysis Stats");
            display.setTextColor(SSD1306_WHITE);

            dispText(0, 13, "RollingC:" + String(g_xpRollingCnt), 1);
            dispText(0, 21, "Portscan:" + String(g_xpPortsCnt), 1);
            dispText(0, 29, "IR Sends:" + String(g_xpIrSendCnt), 1);
            dispText(0, 37, "Clones:  " + String(g_xpCloneCnt), 1);
            dispText(0, 45, "Analysis:" + String(g_xpAnalCnt), 1);
            dispText(0, 53, "Sessions:" + String(g_xpSessionCnt) + " [J]next", 1);

        // ── Sayfa 4: Oyun & toplam ──
        } else {
            display.fillRect(0, 0, OLED_W, 11, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK); display.setTextSize(1);
            display.setCursor(28, 2); display.print("Game & Total");
            display.setTextColor(SSD1306_WHITE);

            dispText(0, 13, "Games won: " + String(g_xpGameCnt), 1);
            dispText(0, 21, "RF replay: " + String(g_xpSubReplCnt), 1);
            dispText(0, 29, "BLE spam:  " + String(g_xpBleSpamCnt), 1);
            // Toplam XP büyük göster
            display.setTextSize(1);
            dispText(0, 39, "Total XP:", 1);
            display.setTextSize(2);
            display.setCursor(0, 48);
            display.print(g_xp);
            display.setTextSize(1);
            display.setCursor(80, 56); display.print("[J]back");
        }

        dispCommit();

        // Joystick ile sayfa değiştir
        JoyDir jd = readJoystick();
        if (jd == JoyDir::RIGHT || jd == JoyDir::DOWN) {
            page = (page + 1) % 4;
            delay(200);
        } else if (jd == JoyDir::LEFT || jd == JoyDir::UP) {
            page = (page + 3) % 4;  // geri
            delay(200);
        }
        if (readButton() || readButtonLong()) break;
        delay(60);
    }
}

// ─── XP Sıfırla Onay Menüsü ───────────────────────────────────────────
static void xp_reset_run() {
    OptionList opts = {
        {"Evet, sifirla", []() {
            xpReset();
            dispBannerOK("XP & Stats sifirlandi!");
        }},
        {"Vazgec", [](){}},
    };
    int sel = loopOptions(opts, "XP Reset?");
    if (sel == 0) opts[0].action();
}

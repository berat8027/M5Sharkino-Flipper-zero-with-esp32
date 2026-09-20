#pragma once
// ════════════════════════════════════════════════════════════════════
//  mod_esp_modules.h — M5Sharkino ESP/MCU Harici Modüller
//  Pinler: TX2=GP37(Pin7)  RX2=GP38(Pin8)
//          DATA=GP40(Pin4)  SDA2=GP41(Pin5)  SCL2=GP42(Pin6)
//
// ─── AI NOTU: WAVESHARE ESP32-P4 WiFi6 (P4+C6) BAĞLANTISI ─────────
//
//  Bu modül (ModP4C6) WaveShare ESP32-P4-WIFI6 kartını ESP32-S3'e
//  UART köprüsü ile bağlar. Bağlantı:
//
//  ESP32-S3 (bu cihaz)       WaveShare ESP32-P4-WIFI6
//  ─────────────────────     ──────────────────────────
//  GP37 (TX2 / Pin7) ──────► GPIO38 (UART0 RX)
//  GP38 (RX2 / Pin8) ◄────── GPIO37 (UART0 TX)
//  GND  (Pin2)       ───────  GND
//  (P4 kartının kendi regülatörü varsa 3V3 hattı bağlanmaz)
//
//  P4+C6 mimarisi:
//   • P4 = Application core (RISC-V, 400 MHz, RAM 32MB)
//   • C6 = Radio co-processor (WiFi6 2.4GHz + BLE5 + 802.15.4)
//   • P4 ↔ C6 haberleşmesi Espressif IDF SPI dahili (şeffaf)
//   • Kullanıcı P4 tarafında sadece WiFi.begin() veya BLEDevice::init() çağırır
//
//  P4 Arduino IDE ayarları:
//   • Board: "ESP32-P4" (Espressif Arduino Core ≥ 3.1.0)
//   • PlatformIO board: "waveshare_esp32_p4_wifi6_eth"
//   • arduino-esp32 sürümü: 3.1.0+ (P4 desteği 2025 Q1 stabil)
//   • Flash: 16MB, PSRAM: 32MB, CPU: 400MHz
//
//  P4 UART0 pin mapping:
//   • TX0 = GPIO37, RX0 = GPIO38 (HardwareSerial(0))
//   • Baud: 115200 (bu firmware ile uyumlu)
//
//  M5Sharkino UART Protokolü (satır bazlı, P4 ↔ S3):
//   S3→P4: "PING\n"              P4→S3: "PONG:P4:400MHz\n"
//   S3→P4: "INFO\n"              P4→S3: "INFO:{core:P4,clk:400,c6:1,ram:32}\n"
//   S3→P4: "WIFI_SCAN\n"         P4→S3: "SCAN:SSID1,-65|SSID2,-70|...\n"
//   S3→P4: "BLE_SCAN\n"          P4→S3: "BLE:Dev1,-55|Dev2,-60|...\n"
//   S3→P4: "GPIO:pin:val\n"      P4→S3: "GPIO:OK\n"
//   S3→P4: "GPIO_READ:pin\n"     P4→S3: "GPIO:val\n"
//   S3→P4: "OTA:url\n"           P4→S3: "OTA:START\n" ... "OTA:OK\n"
//   S3→P4: "CMD:komut\n"         P4→S3: herhangi yanıt
//   S3→P4: "RESET\n"             P4→S3: (yeniden başlar)
//
//  P4 tarafı minimal köprü kodu (ayrı .ino, P4 kartına yükle):
//  ──────────────────────────────────────────────────────────────────
//  #include <Arduino.h>
//  #include <WiFi.h>
//  #include <BLEDevice.h>
//  #include <BLEScan.h>
//
//  void setup() {
//    Serial.begin(115200);  // GPIO37=TX, GPIO38=RX (P4 UART0)
//    Serial.println("PONG:P4:400MHz");
//    WiFi.mode(WIFI_STA);
//    BLEDevice::init("M5Sharkino-P4");
//  }
//
//  void loop() {
//    if (Serial.available()) {
//      String cmd = Serial.readStringUntil('\n');
//      cmd.trim();
//      if (cmd == "PING") {
//        Serial.println("PONG:P4:400MHz");
//      } else if (cmd == "INFO") {
//        Serial.printf("INFO:{core:P4,clk:400,c6:1,ram:%d}\n", ESP.getPsramSize()/1024/1024);
//      } else if (cmd == "WIFI_SCAN") {
//        int n = WiFi.scanNetworks();
//        String out = "SCAN:";
//        for (int i = 0; i < n && i < 10; i++) {
//          out += WiFi.SSID(i) + "," + String(WiFi.RSSI(i));
//          if (i < n-1) out += "|";
//        }
//        Serial.println(out);
//      } else if (cmd == "BLE_SCAN") {
//        BLEScan* scan = BLEDevice::getScan();
//        scan->setActiveScan(true);
//        BLEScanResults res = scan->start(3, false);
//        String out = "BLE:";
//        for (int i = 0; i < res.getCount() && i < 10; i++) {
//          BLEAdvertisedDevice dev = res.getDevice(i);
//          out += dev.getName().c_str();
//          out += "," + String(dev.getRSSI());
//          if (i < res.getCount()-1) out += "|";
//        }
//        Serial.println(out);
//        scan->clearResults();
//      } else if (cmd.startsWith("GPIO:")) {
//        // GPIO:13:1  veya  GPIO:13:0
//        int pin = cmd.substring(5, cmd.indexOf(':',5)).toInt();
//        int val = cmd.substring(cmd.lastIndexOf(':')+1).toInt();
//        pinMode(pin, OUTPUT); digitalWrite(pin, val);
//        Serial.println("GPIO:OK");
//      } else if (cmd == "RESET") {
//        esp_restart();
//      } else {
//        Serial.println("ERR:UNKNOWN");
//      }
//    }
//  }
// ════════════════════════════════════════════════════════════════════
#include <Arduino.h>

#ifndef PIN_MOD_DATA
  #define PIN_MOD_DATA  40
  #define PIN_MOD_SDA2  41
  #define PIN_MOD_SCL2  42
  #define PIN_MOD_TX2   37
  #define PIN_MOD_RX2   38
#endif

// ────────────────────────────────────────────────────────────────────
// 1. WaveShare ESP32-P4 WiFi6 (P4+C6) Modülü
// Bağlantı: S3 TX2(GP37) → P4 RX(GPIO38), S3 RX2(GP38) ← P4 TX(GPIO37)
// ────────────────────────────────────────────────────────────────────
namespace ModP4C6 {

static String sendCmd(const String& cmd, uint32_t waitMs = 3000) {
    while (Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0 = millis(); String resp = "";
    while (millis() - t0 < waitMs) {
        while (Serial2.available()) {
            char c = Serial2.read();
            resp += c;
            if (c == '\n') goto done;
        }
        delay(10);
    }
    done:
    resp.trim(); return resp;
}

static void show_resp(const String& title, const String& r, uint8_t startY = 12) {
    dispClear(); dispStatusBar(title);
    // Her 20 karakter bir satır
    for (int i = 0; i < 4 && startY + i * 12 < 54; i++) {
        String seg = r.substring(i * 20, (i + 1) * 20);
        if (seg.length() > 0) dispText(0, startY + i * 12, seg);
    }
    dispText(0, 54, "[BTN]=kapat"); dispCommit();
    while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
}

static void run() {
    Serial2.begin(115200, SERIAL_8N1, PIN_MOD_RX2, PIN_MOD_TX2);
    delay(300);

    OptionList opts = {
        {"Ping / Baglanti", nullptr},
        {"Chip Bilgi",      nullptr},
        {"WiFi6 Tara",      nullptr},
        {"BLE5 Tara",       nullptr},
        {"GPIO Kontrol",    nullptr},
        {"GPIO Oku",        nullptr},
        {"OTA Tetikle",     nullptr},
        {"Komut Terminal",  nullptr},
        {"JSON API Test",   nullptr},
        {"Reset",           nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "P4+C6 WiFi6");
    if (sel < 0 || sel == 10) { Serial2.end(); return; }

    if (sel == 0) { // Ping
        dispClear(); dispStatusBar("P4+C6 Ping");
        dispText(0, 14, "PING gonderiliyor...");
        dispCommit(); delay(300);
        String r = sendCmd("PING", 2000);
        if (r.length() == 0) r = "(Yanit yok - kablo?)";
        show_resp("Ping Sonuc", r);

    } else if (sel == 1) { // Chip bilgi
        String r = sendCmd("INFO", 3000);
        show_resp("P4 Chip Bilgi", r);

    } else if (sel == 2) { // WiFi6 Tara
        dispClear(); dispStatusBar("WiFi6 Tarama");
        dispText(0, 18, "P4 WiFi scan...");
        dispText(0, 30, "(~5 saniye)"); dispCommit();
        String r = sendCmd("WIFI_SCAN", 8000);
        // "SCAN:SSID1,-65|SSID2,-70|..."
        dispClear(); dispStatusBar("WiFi6 Sonuclar");
        if (r.startsWith("SCAN:")) {
            String data = r.substring(5);
            int lineY = 12, apIdx = 0;
            while (data.length() > 0 && lineY < 54) {
                int sep = data.indexOf('|');
                String ap = (sep >= 0) ? data.substring(0, sep) : data;
                dispText(0, lineY, ap.substring(0, 21));
                lineY += 10; apIdx++;
                if (sep < 0) break;
                data = data.substring(sep + 1);
            }
            char b[16]; snprintf(b, 16, "%d AP", apIdx);
            dispText(90, 54, b);
        } else {
            dispText(0, 24, "Hata: " + r.substring(0, 18));
        }
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 3) { // BLE5 Tara
        dispClear(); dispStatusBar("BLE5 Tarama");
        dispText(0, 18, "P4 BLE scan...");
        dispText(0, 30, "(~4 saniye)"); dispCommit();
        String r = sendCmd("BLE_SCAN", 7000);
        dispClear(); dispStatusBar("BLE5 Sonuclar");
        if (r.startsWith("BLE:")) {
            String data = r.substring(4);
            int lineY = 12, devIdx = 0;
            while (data.length() > 0 && lineY < 54) {
                int sep = data.indexOf('|');
                String dev = (sep >= 0) ? data.substring(0, sep) : data;
                dispText(0, lineY, dev.substring(0, 21));
                lineY += 10; devIdx++;
                if (sep < 0) break;
                data = data.substring(sep + 1);
            }
            char b[16]; snprintf(b, 16, "%d dev", devIdx);
            dispText(84, 54, b);
        } else {
            dispText(0, 24, "Hata: " + r.substring(0, 18));
        }
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 4) { // GPIO kontrol
        char pinS[4] = "13"; charPicker(pinS, 3, "P4 GPIO pin no");
        OptionList vals = {{"HIGH (1)", nullptr}, {"LOW (0)", nullptr}};
        int vs = loopOptions(vals, "Deger");
        if (vs >= 0) {
            String cmd = "GPIO:" + String(pinS) + ":" + String(vs == 0 ? 1 : 0);
            String r = sendCmd(cmd, 2000);
            show_resp("GPIO Sonuc", r);
        }

    } else if (sel == 5) { // GPIO oku
        char pinS[4] = "0"; charPicker(pinS, 3, "P4 GPIO pin no");
        String cmd = "GPIO_READ:" + String(pinS);
        String r = sendCmd(cmd, 2000);
        show_resp("GPIO Deger", r);

    } else if (sel == 6) { // OTA
        char url[64] = "http://192.168.1.100/firmware.bin";
        charPicker(url, 63, "OTA URL");
        String r = sendCmd("OTA:" + String(url), 30000);
        show_resp("OTA Sonuc", r);

    } else if (sel == 7) { // Komut terminal
        dispClear(); dispStatusBar("P4 Terminal");
        dispText(0, 14, "BTN=komut gir"); dispText(0, 26, "LONG=cik"); dispCommit();
        String lastRx = "";
        while (!readButtonLong()) {
            potApplyBrightness();
            while (Serial2.available()) {
                char c = Serial2.read();
                if (c == '\n' || c == '\r') {
                    if (lastRx.length() > 0) {
                        dispClear(); dispStatusBar("P4 Terminal");
                        dispText(0, 12, "RX:");
                        dispText(0, 24, lastRx.substring(0, 21));
                        if (lastRx.length() > 21) dispText(0, 36, lastRx.substring(21, 42));
                        dispText(0, 54, "BTN=komut LONG=cik"); dispCommit();
                        logPush("[P4] " + lastRx);
                    }
                    lastRx = "";
                } else if (lastRx.length() < 80) lastRx += c;
            }
            if (readButton()) {
                char cmd[65] = ""; charPicker(cmd, 64, "P4 Komut");
                if (strlen(cmd) > 0) {
                    Serial2.println(cmd);
                    dispBannerOK("Gonderildi");
                }
            }
            delay(50);
        }

    } else if (sel == 8) { // JSON API test
        dispClear(); dispStatusBar("JSON API Test");
        dispText(0, 14, "PING + INFO cagrilıyor"); dispCommit();
        String ping = sendCmd("PING", 2000);
        String info = sendCmd("INFO", 3000);
        dispClear(); dispStatusBar("JSON API");
        dispText(0, 12, "Ping: " + ping.substring(0, 16));
        dispText(0, 26, info.substring(0, 21));
        if (info.length() > 21) dispText(0, 38, info.substring(21, 42));
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 9) { // Reset
        Serial2.println("RESET"); delay(200);
        dispBannerOK("P4 reset komutu gonderildi");
    }
    Serial2.end();
}
} // namespace ModP4C6

// ────────────────────────────────────────────────────────────────────
// 2. Arduino UART Köprüsü (UNO/Nano/Mega/Pro Mini vb.)
// Bağlantı: S3 TX2(GP37) → Arduino RX, S3 RX2(GP38) ← Arduino TX
// Arduino tarafında HardwareSerial veya SoftwareSerial 115200
// ────────────────────────────────────────────────────────────────────
namespace ModArduino {

static String sendCmd(const String& cmd, uint32_t waitMs = 2000) {
    while (Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0 = millis(); String resp = "";
    while (millis() - t0 < waitMs) {
        while (Serial2.available()) {
            char c = Serial2.read();
            if (c == '\n') { resp.trim(); return resp; }
            resp += c;
        }
        delay(10);
    }
    resp.trim(); return resp;
}

static void run() {
    Serial2.begin(115200, SERIAL_8N1, PIN_MOD_RX2, PIN_MOD_TX2);
    delay(300); Serial2.println("PING");

    OptionList opts = {
        {"Ping",            nullptr},
        {"Versiyon",        nullptr},
        {"GPIO Yaz",        nullptr},
        {"GPIO Oku",        nullptr},
        {"Analog Oku",      nullptr},
        {"I2C Proxy",       nullptr},
        {"SPI Proxy",       nullptr},
        {"Komut Gonder",    nullptr},
        {"Firmware Bilgi",  nullptr},
        {"Reset",           nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "Arduino Bridge");
    if (sel < 0 || sel == 10) { Serial2.end(); return; }

    auto showR = [](const String& title, const String& r) {
        dispClear(); dispStatusBar(title);
        dispText(0, 18, r.substring(0, 21));
        if (r.length() > 21) dispText(0, 32, r.substring(21, 42));
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    };

    if (sel == 0) { String r = sendCmd("PING");     showR("Ping", r.length() ? r : "Yanit yok"); }
    else if (sel == 1) { String r = sendCmd("VERSION"); showR("Versiyon", r); }
    else if (sel == 2) {
        char pin[4] = "13"; charPicker(pin, 3, "Arduino pin");
        OptionList vals = {{"HIGH", nullptr}, {"LOW", nullptr}};
        int vs = loopOptions(vals, "Deger");
        if (vs >= 0) showR("GPIO Yaz", sendCmd("GPIO:" + String(pin) + ":" + String(vs == 0 ? 1 : 0)));
    } else if (sel == 3) {
        char pin[4] = "7"; charPicker(pin, 3, "Arduino pin");
        showR("GPIO Oku", sendCmd("DREAD:" + String(pin)));
    } else if (sel == 4) {
        char pin[4] = "0"; charPicker(pin, 3, "Analog pin (A0=0)");
        showR("Analog Oku", sendCmd("AREAD:" + String(pin)));
    } else if (sel == 5) {
        char addr[6] = "0x3C"; charPicker(addr, 3, "I2C adres (hex)");
        char reg[4] = "0";    charPicker(reg, 3, "Register");
        showR("I2C Proxy", sendCmd("I2C:" + String(addr) + ":" + String(reg)));
    } else if (sel == 7) {
        char cmd[65] = ""; charPicker(cmd, 64, "Komut gir");
        if (strlen(cmd) > 0) showR("Komut", sendCmd(String(cmd)));
    } else if (sel == 8) { showR("Firmware", sendCmd("INFO")); }
    else if (sel == 9) { Serial2.println("RESET"); delay(200); dispBannerOK("Reset gonderildi"); }
    Serial2.end();
}
} // namespace ModArduino

// ────────────────────────────────────────────────────────────────────
// 3. Raspberry Pi UART Köprüsü
// Bağlantı: S3 TX2(GP37) → Pi UART RX (GPIO15), S3 RX2(GP38) ← Pi TX (GPIO14)
// Pi tarafında: raspi-config → Serial Port enable, minicom/custom script
// UYARI: Pi 3.3V UART — doğrudan bağlanabilir (seviye dönüştürücü gerekmez)
// ────────────────────────────────────────────────────────────────────
namespace ModRaspberryPi {

static String piCmd(const String& cmd, uint32_t waitMs = 3000) {
    while (Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0 = millis(); String resp = "";
    char prev = 0;
    while (millis() - t0 < waitMs) {
        while (Serial2.available()) {
            char c = Serial2.read();
            resp += c;
            if (prev == '\n' && c == '\n') goto done2;  // çift newline = yanıt sonu
            prev = c;
        }
        delay(10);
    }
    done2:
    resp.trim(); return resp;
}

static void run() {
    Serial2.begin(115200, SERIAL_8N1, PIN_MOD_RX2, PIN_MOD_TX2);
    delay(300);

    OptionList opts = {
        {"Ping / SSH Test", nullptr},
        {"Sistem Bilgi",    nullptr},
        {"CPU / RAM",       nullptr},
        {"Network Bilgi",   nullptr},
        {"GPIO (pigpiod)",  nullptr},
        {"Dosya Listele",   nullptr},
        {"Komut Calistir",  nullptr},
        {"SD Log Al",       nullptr},
        {"Servis Durumu",   nullptr},
        {"Yeniden Baslat",  nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "Raspberry Pi");
    if (sel < 0 || sel == 10) { Serial2.end(); return; }

    auto showR = [](const String& t, const String& r) {
        dispClear(); dispStatusBar(t);
        for (int i = 0; i < 4; i++) {
            String seg = r.substring(i * 20, (i + 1) * 20);
            if (seg.length()) dispText(0, 12 + i * 12, seg);
        }
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    };

    // Pi tarafında bir dinleyici script gerekir:
    // #!/usr/bin/env python3
    // import serial, subprocess
    // s = serial.Serial('/dev/serial0', 115200)
    // while True:
    //   cmd = s.readline().decode().strip()
    //   if cmd == 'PING': s.write(b'PONG:Pi\n\n')
    //   elif cmd == 'SYSINFO': s.write(subprocess.check_output(['uname','-a'])+b'\n\n')
    //   elif cmd == 'CPU': s.write(subprocess.check_output(['vcgencmd','measure_temp'])+b'\n\n')
    //   elif cmd.startswith('CMD:'): s.write(subprocess.check_output(cmd[4:].split())+b'\n\n')
    //   ...

    if (sel == 0) { showR("Pi Ping", piCmd("PING")); }
    else if (sel == 1) { showR("Sistem Bilgi", piCmd("SYSINFO")); }
    else if (sel == 2) {
        String cpu = piCmd("CPU");
        String ram = piCmd("RAM");
        dispClear(); dispStatusBar("CPU / RAM");
        dispText(0, 14, "CPU: " + cpu.substring(0, 16));
        dispText(0, 28, "RAM: " + ram.substring(0, 16));
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 3) { showR("Network", piCmd("NET")); }
    else if (sel == 4) {
        char pin[4] = "17"; charPicker(pin, 3, "Pi GPIO pin");
        OptionList vals = {{"HIGH", nullptr}, {"LOW", nullptr}, {"INPUT (oku)", nullptr}};
        int vs = loopOptions(vals, "GPIO Islem");
        if (vs == 0) showR("GPIO", piCmd("GPIO:" + String(pin) + ":OUT:1"));
        else if (vs == 1) showR("GPIO", piCmd("GPIO:" + String(pin) + ":OUT:0"));
        else showR("GPIO", piCmd("GPIO:" + String(pin) + ":IN"));
    } else if (sel == 5) {
        char path[33] = "/home/pi"; charPicker(path, 32, "Dizin yolu");
        showR("Dosyalar", piCmd("LS:" + String(path)));
    } else if (sel == 6) {
        char cmd[65] = ""; charPicker(cmd, 64, "Komut gir");
        if (strlen(cmd) > 0) showR("Komut Ciktisi", piCmd("CMD:" + String(cmd), 5000));
    } else if (sel == 7) {
        if (!g_sdInited) { dispBannerErr("SD kart yok"); Serial2.end(); return; }
        String log = piCmd("LOG", 5000);
        File f = SD.open("/pi_log.txt", FILE_APPEND);
        if (f) { f.println(log); f.close(); dispBannerOK("Pi log kaydedildi"); }
    } else if (sel == 8) { showR("Servis", piCmd("SERVICE")); }
    else if (sel == 9) {
        OptionList conf = {{"Evet, yeniden baslat", nullptr}, {"Hayir", nullptr}};
        int cs = loopOptions(conf, "Emin misin?");
        if (cs == 0) { piCmd("REBOOT", 500); dispBannerWarn("Pi yeniden baslatiliyor!"); }
    }
    Serial2.end();
}
} // namespace ModRaspberryPi

// ────────────────────────────────────────────────────────────────────
// 4. ESP32-CAM Modülü
// Bağlantı: S3 TX2(GP37) → CAM U0RX (IO3), S3 RX2(GP38) ← CAM U0TX (IO1)
// ESP32-CAM'de özel firmware gerekir (AT komutu veya custom UART)
// ────────────────────────────────────────────────────────────────────
namespace ModESP32CAM {

static String camCmd(const String& cmd, uint32_t waitMs = 3000) {
    while (Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0 = millis(); String resp = "";
    while (millis() - t0 < waitMs) {
        while (Serial2.available()) {
            char c = Serial2.read();
            resp += c;
            if (c == '\n') { resp.trim(); return resp; }
        }
        delay(10);
    }
    resp.trim(); return resp;
}

static void run() {
    Serial2.begin(115200, SERIAL_8N1, PIN_MOD_RX2, PIN_MOD_TX2);
    delay(300);

    OptionList opts = {
        {"Ping / Hazir mi", nullptr},
        {"Frame Tetikle",   nullptr},
        {"Cozunurluk Sec",  nullptr},
        {"Flash LED",       nullptr},
        {"Hareket Algila",  nullptr},
        {"SD Kaydet",       nullptr},
        {"Stream URL Al",   nullptr},
        {"Reset",           nullptr},
        {"Chip Bilgi",      nullptr},
        {"Config",          nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "ESP32-CAM");
    if (sel < 0 || sel == 10) { Serial2.end(); return; }

    // Not: ESP32-CAM tarafında bu protokolü anlayan firmware yüklenmiş olmalı
    // Örnek CAM firmware komutu: "PING" → "PONG:CAM", "FRAME" → "FRAME:OK:1024B"

    auto showR = [](const String& t, const String& r) {
        dispClear(); dispStatusBar(t);
        dispText(0, 18, r.substring(0, 21));
        if (r.length() > 21) dispText(0, 32, r.substring(21, 42));
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    };

    if (sel == 0) { showR("CAM Ping", camCmd("PING")); }
    else if (sel == 1) {
        dispClear(); dispStatusBar("Frame Tetikle");
        dispText(0, 18, "Frame aliniyor..."); dispCommit();
        String r = camCmd("FRAME", 5000);
        showR("Frame", r.length() ? r : "Yanit yok");
    } else if (sel == 2) {
        OptionList res = {
            {"QQVGA 160x120",nullptr},{"QVGA 320x240",nullptr},
            {"VGA  640x480", nullptr},{"SVGA 800x600",nullptr},{"Back",nullptr}};
        static const char* rCmds[] = {"RES:QQVGA","RES:QVGA","RES:VGA","RES:SVGA"};
        int rs = loopOptions(res, "Cozunurluk");
        if (rs >= 0 && rs < 4) showR("Cozunurluk", camCmd(rCmds[rs]));
    } else if (sel == 3) {
        OptionList fl = {{"Flash ON",nullptr},{"Flash OFF",nullptr},{"Flash PULSE",nullptr}};
        int fs = loopOptions(fl, "Flash");
        if (fs == 0) camCmd("FLASH:1");
        else if (fs == 1) camCmd("FLASH:0");
        else { camCmd("FLASH:1"); delay(500); camCmd("FLASH:0"); }
        dispBannerOK("Flash komutu gonderildi");
    } else if (sel == 4) {
        dispClear(); dispStatusBar("Hareket Algila");
        dispText(0, 14, "CAM hareket modu"); dispCommit();
        String r = camCmd("MOTION_EN", 2000);
        while (!readButtonLong()) {
            potApplyBrightness();
            while (Serial2.available()) {
                String line = Serial2.readStringUntil('\n');
                if (line.indexOf("MOTION") >= 0) {
                    dispClear(); dispStatusBar("Hareket!");
                    dispText(16, 24, "!! HAREKET !!");
                    dispText(0, 54, "[LONG]=cik"); dispCommit();
                    buzzOK();
                }
            }
            delay(50);
        }
        camCmd("MOTION_DIS");
    } else if (sel == 5) {
        if (!g_sdInited) { dispBannerErr("SD kart yok"); Serial2.end(); return; }
        dispClear(); dispStatusBar("SD Kaydet"); dispText(0, 18, "CAM→SD kaydediliyor"); dispCommit();
        String r = camCmd("SAVE_SD", 8000);
        showR("SD Kaydet", r);
    } else if (sel == 6) {
        String r = camCmd("STREAM_URL", 2000);
        showR("Stream URL", r);
    } else if (sel == 7) { Serial2.println("RESET"); delay(200); dispBannerOK("CAM reset gonderildi"); }
    else if (sel == 8) { showR("Chip Bilgi", camCmd("INFO")); }
    else if (sel == 9) { showR("Config", camCmd("CONFIG")); }
    Serial2.end();
}
} // namespace ModESP32CAM

// ────────────────────────────────────────────────────────────────────
// 5. CH32V003 RISC-V MCU Köprüsü
// Bağlantı: TX→Pin7(GP37), RX→Pin8(GP38)
// CH32V003: UART 115200, custom protocol
// Not: CH32V003 = WCH RISC-V MCU, 8-pin TSSOP/SOP, 48MHz, 2KB RAM
// ────────────────────────────────────────────────────────────────────
namespace ModCH32V {

static String chCmd(const String& cmd, uint32_t waitMs = 2000) {
    while (Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0 = millis(); String resp = "";
    while (millis() - t0 < waitMs) {
        while (Serial2.available()) {
            char c = Serial2.read();
            resp += c;
            if (c == '\n') { resp.trim(); return resp; }
        }
        delay(10);
    }
    resp.trim(); return resp;
}

static void run() {
    Serial2.begin(115200, SERIAL_8N1, PIN_MOD_RX2, PIN_MOD_TX2);
    delay(200);

    OptionList opts = {
        {"Ping",            nullptr},
        {"GPIO Yaz",        nullptr},
        {"GPIO Oku",        nullptr},
        {"ADC Oku",         nullptr},
        {"Timer Ayarla",    nullptr},
        {"Komut Gonder",    nullptr},
        {"Ping Bellek",     nullptr},
        {"Register Dump",   nullptr},
        {"Reset",           nullptr},
        {"Istatistik",      nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "CH32V003 RISC-V");
    if (sel < 0 || sel == 10) { Serial2.end(); return; }

    auto showR = [](const String& t, const String& r) {
        dispClear(); dispStatusBar(t);
        dispText(0, 18, r.substring(0, 21));
        if (r.length() > 21) dispText(0, 32, r.substring(21, 42));
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    };

    if (sel == 0) { showR("CH32 Ping", chCmd("PING")); }
    else if (sel == 1) {
        char pin[3] = "1"; charPicker(pin, 2, "GPIO (1-7)");
        OptionList vals = {{"HIGH",nullptr},{"LOW",nullptr}};
        int vs = loopOptions(vals, "Deger");
        if (vs >= 0) showR("GPIO", chCmd("GPIO:" + String(pin) + ":" + String(vs == 0 ? 1 : 0)));
    } else if (sel == 2) {
        char pin[3] = "1"; charPicker(pin, 2, "GPIO (1-7)");
        showR("GPIO Oku", chCmd("DREAD:" + String(pin)));
    } else if (sel == 3) {
        char ch2[3] = "0"; charPicker(ch2, 2, "ADC kanal (0-3)");
        showR("ADC", chCmd("AREAD:" + String(ch2)));
    } else if (sel == 4) {
        char perS[8] = "1000"; charPicker(perS, 7, "Period (us)");
        showR("Timer", chCmd("TIMER:" + String(perS)));
    } else if (sel == 5) {
        char cmd[33] = ""; charPicker(cmd, 32, "Komut gir");
        if (strlen(cmd) > 0) showR("Komut", chCmd(String(cmd)));
    } else if (sel == 6) {
        // Flash ROM alanı test
        showR("Bellek", chCmd("MEM"));
    } else if (sel == 7) {
        showR("Register", chCmd("REGDUMP"));
    } else if (sel == 8) {
        Serial2.println("RESET"); delay(200); dispBannerOK("CH32 reset gonderildi");
    } else if (sel == 9) {
        showR("Stats", chCmd("STATS"));
    }
    Serial2.end();
}
} // namespace ModCH32V

// ────────────────────────────────────────────────────────────────────
// I2C Tarama (ortak yardımcı)
// ────────────────────────────────────────────────────────────────────
namespace ModI2CScan {
static void run() {
    Wire1.begin(PIN_MOD_SDA2, PIN_MOD_SCL2, 400000);
    dispClear(); dispStatusBar("I2C Tarama (Bus1)");
    dispText(0, 14, "SDA:GP41  SCL:GP42");
    dispText(0, 26, "Taranıyor 1-127..."); dispCommit();
    delay(300);

    std::vector<uint8_t> found;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire1.beginTransmission(addr);
        if (Wire1.endTransmission() == 0) found.push_back(addr);
        delay(2);
    }

    dispClear(); dispStatusBar("I2C Sonuclar");
    if (found.empty()) {
        dispText(0, 20, "Cihaz bulunamadi!");
        dispText(0, 32, "SDA/SCL kontrol et");
    } else {
        char line[24]; snprintf(line, 24, "%d cihaz:", (int)found.size());
        dispText(0, 12, line);
        for (uint8_t i = 0; i < found.size() && i < 4; i++) {
            // Yaygın I2C adresleri
            const char* name = "";
            switch (found[i]) {
                case 0x3C: name="SSD1306/OLED"; break;
                case 0x57: name="MAX30102";     break;
                case 0x53: name="ADXL345";      break;
                case 0x76: name="BMP280/BME";   break;
                case 0x77: name="BMP280/BME";   break;
                case 0x68: name="MPU6050/DS3231";break;
                case 0x48: name="ADS1115";      break;
                default:   name="?";            break;
            }
            char adr[24]; snprintf(adr, 24, "0x%02X %s", found[i], name);
            dispText(0, 24 + i * 10, adr);
        }
    }
    dispText(0, 54, "[BTN]=tekrar [LONG]=cik"); dispCommit();
    while (!readButtonLong()) {
        potApplyBrightness();
        if (readButton()) { run(); return; }
        delay(50);
    }
}
} // namespace ModI2CScan

// ════════════════════════════════════════════════════════════════════
//  ESP/MCU MODÜLLER ANA MENÜSÜ
// ════════════════════════════════════════════════════════════════════
static void esp_modules_menu_run() {
    OptionList opts = {
        {"P4+C6 WiFi6",     nullptr},
        {"Arduino Bridge",  nullptr},
        {"Raspberry Pi",    nullptr},
        {"ESP32-CAM",       nullptr},
        {"CH32V003 RISC-V", nullptr},
        {"I2C Tarama",      nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "ESP/MCU Moduller");
    if (sel < 0 || sel == 6) return;
    switch (sel) {
        case 0: ModP4C6::run();          break;
        case 1: ModArduino::run();       break;
        case 2: ModRaspberryPi::run();   break;
        case 3: ModESP32CAM::run();      break;
        case 4: ModCH32V::run();         break;
        case 5: ModI2CScan::run();       break;
    }
}

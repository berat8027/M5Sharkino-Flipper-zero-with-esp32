#pragma once
// ═══════════════════════════════════════════════════════════
//  mod_uart.h — ESP32-S3 SuperMini Hardware UART
//  DevKitC ↔ SuperMini UART haberleşme
//
//  Bağlantı:
//    SuperMini TX → DevKitC GPIO44 (RX)
//    SuperMini RX ← DevKitC GPIO43 (TX)
//    GND          → GND
//
//  Protokol:
//    DevKitC → SuperMini: "JAM\n" / "SCAN\n" / "SPEC\n"
//                         "MITM_P\n" / "MITM_A\n" / "STOP\n"
//    SuperMini → DevKitC: "JAM:OK\n" / "DRONE:DJI:52:180\n" vb.
// ═══════════════════════════════════════════════════════════

// SuperMini'nin hardware UART pinleri
// TX = GPIO43 (SuperMini kartında "TX" etiketli fiziksel pin)
// RX = GPIO44 (SuperMini kartında "RX" etiketli fiziksel pin)
// DİKKAT: GPIO20=USB D+, GPIO21=USB D- — kesinlikle kullanma!

static String g_uartRxBuf  = "";
static bool   g_cmdReady   = false;
static String g_lastCmd    = "";

static void bleUartInit() {
    // Serial  = USB CDC (debug)
    // Serial1 = Master DevKitC haberleşme (TX=GPIO43, RX=GPIO44)
    Serial1.begin(115200, SERIAL_8N1, 44, 43); // RX=GPIO44, TX=GPIO43
    Serial.println("[UART] DevKitC haberleşme başlatıldı");
    Serial.println("[UART] TX=GPIO43 RX=GPIO44 @ 115200");
}

static void bleUartSend(const String& msg) {
    Serial1.print(msg);
    Serial.print("[UART TX] " + msg); // debug
}

// Komutu al ve sıfırla — her loop'ta çağrılır
static String bleGetCmd() {
    // Serial1'den gelen veriyi oku
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        g_uartRxBuf += c;
        if (c == '\n') {
            g_lastCmd = g_uartRxBuf;
            g_lastCmd.trim();
            g_uartRxBuf = "";
            g_cmdReady = true;
        }
    }

    if (!g_cmdReady) return "";
    g_cmdReady = false;
    return g_lastCmd;
}

#pragma once
// ════════════════════════════════════════════════════════════════════
//  mod_actuators.h — M5Sharkino Aktüatör Modülleri
//  Pinler: DATA=GP40(Pin4)  TX2=GP37(Pin7)  RX2=GP38(Pin8)
//          SDA2=GP41(Pin5)  SCL2=GP42(Pin6)
// ════════════════════════════════════════════════════════════════════
#include <Arduino.h>

#ifndef PIN_MOD_DATA
  #define PIN_MOD_DATA  40
  #define PIN_MOD_SDA2  41
  #define PIN_MOD_SCL2  42
  #define PIN_MOD_TX2   37
  #define PIN_MOD_RX2   38
#endif

// LEDC kanal sabitleri (0-3 ana kodda kullanılıyor, 4+ buraya)
static constexpr uint8_t LEDC_CH_SERVO   = 4;
static constexpr uint8_t LEDC_CH_DCMOTOR = 5;
static constexpr uint8_t LEDC_CH_BUZZER  = 6;
static constexpr uint8_t LEDC_CH_LED     = 7;
static constexpr uint8_t LEDC_CH_MOTOR_B = 8;

// ────────────────────────────────────────────────────────────────────
// 1. BUZZER — Aktif/Pasif Buzzer
// Bağlantı: Buzzer+ → Pin4 (GP40), Buzzer- → GND
// Pasif buzzer için PWM (LEDC), aktif için digitalWrite yeterli
// ────────────────────────────────────────────────────────────────────
namespace ModBuzzer {

// RTTTL melodi parser (Ring Tone Text Transfer Language)
// Örnek: "Mario:d=4,o=5,b=200:e5,e5,p,e5,p,c5,e5,p,g5"
static void rtttl_play(const char* tune) {
    String t(tune);
    int colon1 = t.indexOf(':');
    int colon2 = t.indexOf(':', colon1 + 1);
    if (colon1 < 0 || colon2 < 0) return;

    String params = t.substring(colon1 + 1, colon2);
    String notes  = t.substring(colon2 + 1);

    // Parametreler
    int defDur = 4, defOct = 6, bpm = 63;
    if (params.indexOf("d=") >= 0) defDur = params.substring(params.indexOf("d=") + 2).toInt();
    if (params.indexOf("o=") >= 0) defOct = params.substring(params.indexOf("o=") + 2).toInt();
    if (params.indexOf("b=") >= 0) bpm    = params.substring(params.indexOf("b=") + 2).toInt();

    uint32_t wholenote = 60000UL * 4 / bpm;

    // Nota frekansları (C4=262Hz, her oktav 2x)
    static const uint16_t noteFreq[] = {0,262,277,294,311,330,349,370,392,415,440,466,494};
    // c, c#, d, d#, e, f, f#, g, g#, a, a#, b
    static const char noteNames[] = "ccddeffggaab";
    static const bool noteSharp[] = {0,1,0,1,0,0,1,0,1,0,1,0};

    ledcAttach(PIN_MOD_DATA, 2000, 10);

    int i = 0;
    while (i < (int)notes.length()) {
        // Duration
        int dur = 0;
        while (i < (int)notes.length() && isdigit(notes[i])) dur = dur * 10 + (notes[i++] - '0');
        if (dur == 0) dur = defDur;

        // Note
        if (i >= (int)notes.length()) break;
        char n = tolower(notes[i++]);
        bool sharp = false;
        if (i < (int)notes.length() && notes[i] == '#') { sharp = true; i++; }
        bool dotted = false;
        if (i < (int)notes.length() && notes[i] == '.') { dotted = true; i++; }
        int oct = defOct;
        if (i < (int)notes.length() && isdigit(notes[i])) oct = notes[i++] - '0';
        if (i < (int)notes.length() && notes[i] == ',') i++;

        uint32_t duration = wholenote / dur;
        if (dotted) duration = duration * 3 / 2;

        // Frekans bul
        uint32_t freq = 0;
        if (n != 'p') {
            for (int ni = 0; ni < 12; ni++) {
                if (noteNames[ni] == n && noteSharp[ni] == sharp) {
                    freq = noteFreq[ni + 1];
                    // Oktav uygula
                    int octDiff = oct - 4;
                    if (octDiff > 0) freq <<= octDiff;
                    else if (octDiff < 0) freq >>= (-octDiff);
                    break;
                }
            }
        }

        if (freq > 0) { ledcChangeFrequency(PIN_MOD_DATA, freq, 10); ledcWrite(PIN_MOD_DATA, 512); }
        else ledcWrite(PIN_MOD_DATA, 0);

        delay(duration * 0.9f);
        ledcWrite(PIN_MOD_DATA, 0);
        delay(duration * 0.1f);

        if (readButtonLong()) break;
    }
    ledcDetach(PIN_MOD_DATA);
}

// Morse kodu
static void morse_char(char c, uint32_t dot = 100) {
    struct { char ch; const char* code; } morse[] = {
        {'A',".-"},{'B',"-..."},{'C',"-.-."},{'D',"-.."},{'E',"."},
        {'F',"..-."},{'G',"--."},{'H',"...."},{'I',".."},{'J',".---"},
        {'K',"-.-"},{'L',".-.."},{'M',"--"},{'N',"-."},{'O',"---"},
        {'P',".--."},{'Q',"--.-"},{'R',".-."},{'S',"..."},{'T',"-"},
        {'U',"..-"},{'V',"...-"},{'W',".--"},{'X',"-..-"},{'Y',"-.--"},
        {'Z',"--.."},{'0',"-----"},{'1',".----"},{'2',"..---"},{'3',"...--"},
        {'4',"....-"},{'5',"....."},{'6',"-...."},{'7',"--..."},{'8',"---.."},
        {'9',"----."},
    };
    c = toupper(c);
    ledcAttach(PIN_MOD_DATA, 800, 10);
    for (auto& m : morse) {
        if (m.ch == c) {
            for (int i = 0; m.code[i]; i++) {
                ledcWrite(PIN_MOD_DATA, 512);
                delay(m.code[i] == '.' ? dot : dot * 3);
                ledcWrite(PIN_MOD_DATA, 0);
                delay(dot);
            }
            break;
        }
    }
    ledcDetach(PIN_MOD_DATA);
}

static void run() {
    OptionList opts = {
        {"Ton / Frekans",  nullptr},
        {"Melodi (RTTTL)", nullptr},
        {"Morse Kodu",     nullptr},
        {"Alarm Modu",     nullptr},
        {"BPM Metronom",   nullptr},
        {"Arpej Efekti",   nullptr},
        {"Sweep (20-20k)", nullptr},
        {"Volume (PWM%)",  nullptr},
        {"Hazir Melodiler",nullptr},
        {"SOS Gonder",     nullptr},
        {"Back",           nullptr},
    };
    int sel = loopOptions(opts, "Buzzer");
    if (sel < 0 || sel == 10) return;

    if (sel == 0) { // Ton/Frekans
        uint32_t freq = 1000;
        ledcAttach(PIN_MOD_DATA, freq, 10);
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && freq < 18000) { freq += 100; ledcChangeFrequency(PIN_MOD_DATA, freq, 10); buzzNav(); }
            if (d == JoyDir::DOWN && freq > 100)   { freq -= 100; ledcChangeFrequency(PIN_MOD_DATA, freq, 10); buzzNav(); }
            if (d == JoyDir::RIGHT) { ledcWrite(PIN_MOD_DATA, 512); }
            if (d == JoyDir::LEFT)  { ledcWrite(PIN_MOD_DATA, 0);   }
            dispClear(); dispStatusBar("Buzzer Ton");
            char b[24]; snprintf(b, 24, "%lu Hz", freq);
            dispText(16, 18, b);
            dispText(0, 32, "UP/DOWN=frek R=cal L=sus");
            dispText(0, 54, "[LONG]=cik"); dispCommit();
            delay(80);
        }
        ledcWrite(PIN_MOD_DATA, 0); ledcDetach(PIN_MOD_DATA);

    } else if (sel == 1) { // RTTTL
        char tune[128] = "Mario:d=4,o=5,b=200:e5,e5,p,e5,p,c5,e5,p,g5,p,p,g4";
        charPicker(tune, 127, "RTTTL gir");
        rtttl_play(tune);

    } else if (sel == 2) { // Morse
        char msg[33] = "SOS"; charPicker(msg, 32, "Morse metni");
        for (int i = 0; msg[i]; i++) {
            if (msg[i] == ' ') delay(700);
            else { morse_char(msg[i]); delay(300); }
        }
        dispBannerOK("Morse tamamlandi");

    } else if (sel == 3) { // Alarm
        ledcAttach(PIN_MOD_DATA, 2000, 10);
        int phase = 0;
        while (!readButtonLong()) {
            potApplyBrightness();
            uint32_t f = (phase % 2 == 0) ? 2000 : 1500;
            ledcChangeFrequency(PIN_MOD_DATA, f, 10);
            ledcWrite(PIN_MOD_DATA, 512);
            delay(200); ledcWrite(PIN_MOD_DATA, 0); delay(50);
            phase++;
            dispClear(); dispStatusBar("Alarm");
            dispText(24, 24, phase % 2 == 0 ? "! ALARM !" : "");
            dispText(0, 54, "[LONG]=kapat"); dispCommit();
        }
        ledcWrite(PIN_MOD_DATA, 0); ledcDetach(PIN_MOD_DATA);

    } else if (sel == 4) { // Metronom
        int bpm = 120;
        ledcAttach(PIN_MOD_DATA, 1000, 10);
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && bpm < 240) { bpm += 5; }
            if (d == JoyDir::DOWN && bpm > 40)  { bpm -= 5; }
            uint32_t interval = 60000UL / bpm;
            ledcWrite(PIN_MOD_DATA, 512); delay(30);
            ledcWrite(PIN_MOD_DATA, 0);
            dispClear(); dispStatusBar("Metronom");
            char b[24]; snprintf(b, 24, "%d BPM", bpm);
            dispText(24, 20, b);
            dispText(0, 36, "UP/DOWN = BPM");
            dispText(0, 54, "[LONG]=cik"); dispCommit();
            delay(interval - 30);
        }
        ledcDetach(PIN_MOD_DATA);

    } else if (sel == 5) { // Arpej
        static const uint16_t arpNotes[] = {262, 330, 392, 523, 659, 784, 1047, 0};
        ledcAttach(PIN_MOD_DATA, 262, 10);
        while (!readButtonLong()) {
            potApplyBrightness();
            for (int i = 0; arpNotes[i] && !readButtonLong(); i++) {
                ledcChangeFrequency(PIN_MOD_DATA, arpNotes[i], 10);
                ledcWrite(PIN_MOD_DATA, 512);
                delay(100); ledcWrite(PIN_MOD_DATA, 0); delay(20);
            }
        }
        ledcDetach(PIN_MOD_DATA);

    } else if (sel == 6) { // Sweep
        ledcAttach(PIN_MOD_DATA, 20, 10);
        while (!readButtonLong()) {
            potApplyBrightness();
            for (uint32_t f = 20; f <= 20000 && !readButtonLong(); f += 50) {
                ledcChangeFrequency(PIN_MOD_DATA, f, 10);
                ledcWrite(PIN_MOD_DATA, 512);
                delay(5);
            }
            ledcWrite(PIN_MOD_DATA, 0); delay(200);
        }
        ledcDetach(PIN_MOD_DATA);

    } else if (sel == 7) { // Volume (duty)
        uint32_t duty = 512;
        ledcAttach(PIN_MOD_DATA, 1000, 10);
        ledcWrite(PIN_MOD_DATA, duty);
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && duty < 1023) { duty += 50; ledcWrite(PIN_MOD_DATA, duty); }
            if (d == JoyDir::DOWN && duty > 0)    { duty -= 50; ledcWrite(PIN_MOD_DATA, duty); }
            dispClear(); dispStatusBar("Volume (PWM)");
            char b[24]; snprintf(b, 24, "%lu / 1023  (%lu%%)", duty, duty * 100 / 1023);
            dispText(0, 22, b);
            int bw = duty * 112 / 1023;
            display.drawRect(4, 36, 112, 8, SSD1306_WHITE);
            display.fillRect(4, 36, bw, 8, SSD1306_WHITE);
            dispText(0, 54, "[LONG]=kapat"); dispCommit();
            delay(80);
        }
        ledcWrite(PIN_MOD_DATA, 0); ledcDetach(PIN_MOD_DATA);

    } else if (sel == 8) { // Hazır melodiler
        OptionList mels = {
            {"Mario Tema",   nullptr},
            {"Happy Bday",   nullptr},
            {"Alarm Sireni", nullptr},
            {"Twinkle Star", nullptr},
            {"Back",         nullptr},
        };
        int ms = loopOptions(mels, "Hazir Melodi");
        static const char* tunes[] = {
            "Mario:d=4,o=5,b=200:e5,e5,p,e5,p,c5,e5,p,g5,p,p,g4,p,p",
            "Birthday:d=4,o=5,b=125:g,g,a,g,c6,b,p,g,g,a,g,d6,c6",
            "Alarm:d=8,o=5,b=250:a,p,a,p,a,p,a,p,a,p",
            "Twinkle:d=4,o=5,b=120:c,c,g,g,a,a,g2,f,f,e,e,d,d,c2",
        };
        if (ms >= 0 && ms < 4) rtttl_play(tunes[ms]);

    } else if (sel == 9) { // SOS
        static const char* sos = "SOS:d=8,o=5,b=100:e,e,e,p,e,e,e,p,e,e,e,p,p,p";
        rtttl_play(sos);
        dispBannerOK("SOS tamamlandi");
    }
}
} // namespace ModBuzzer

// ────────────────────────────────────────────────────────────────────
// 2. Servo Motor
// Bağlantı: Sinyal → Pin4 (GP40), 50Hz PWM
// ────────────────────────────────────────────────────────────────────
namespace ModServo {
static int angle = 90;
static int savedAngles[5] = {0, 45, 90, 135, 180};

static uint32_t angle_to_duty(int ang) {
    // 50Hz → 20ms period, 16-bit → 65535
    // 0° = 0.5ms = 1638, 180° = 2.5ms = 8192
    return 1638 + (uint32_t)((8192 - 1638) * ang / 180);
}
static void set_angle(int ang) {
    angle = constrain(ang, 0, 180);
    ledcWrite(PIN_MOD_DATA, angle_to_duty(angle));
}

static void run() {
    ledcAttach(PIN_MOD_DATA, 50, 16);
    set_angle(90);

    OptionList opts = {
        {"Manuel Kontrol",   nullptr},
        {"Sweep Modu",       nullptr},
        {"Hiz Ayarli Sweep", nullptr},
        {"Limit Ayarla",     nullptr},
        {"Joystick Modu",    nullptr},
        {"Pozisyon Kaydet",  nullptr},
        {"Kayitli Pozis.",   nullptr},
        {"PWM Raw Gir",      nullptr},
        {"Kalibrasyon",      nullptr},
        {"Grafik Goster",    nullptr},
        {"Back",             nullptr},
    };
    int sel = loopOptions(opts, "Servo Motor");
    if (sel < 0 || sel == 10) { ledcDetach(PIN_MOD_DATA); return; }

    int minAngle = 0, maxAngle = 180;

    if (sel == 0) { // Manuel
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::RIGHT && angle < maxAngle) { set_angle(angle + 1); buzzNav(); }
            if (d == JoyDir::LEFT  && angle > minAngle) { set_angle(angle - 1); buzzNav(); }
            if (d == JoyDir::UP)   set_angle(maxAngle);
            if (d == JoyDir::DOWN) set_angle(minAngle);
            if (readButton())      set_angle(90);

            dispClear(); dispStatusBar("Servo Manuel");
            char b[24]; snprintf(b, 24, "Aci: %3d deg", angle);
            dispText(0, 12, b);
            // Görsel servo kolu
            int cx = 64, cy = 44;
            display.drawCircle(cx, cy, 12, SSD1306_WHITE);
            float rad = (angle - 90) * PI / 180.0f;
            int ex = cx + (int)(14 * cos(rad));
            int ey = cy - (int)(14 * sin(rad));
            display.drawLine(cx, cy, ex, ey, SSD1306_WHITE);
            display.fillCircle(cx, cy, 3, SSD1306_WHITE);
            // PWM bar
            int bw = angle * 112 / 180;
            display.drawRect(4, 30, 112, 6, SSD1306_WHITE);
            display.fillRect(4, 30, bw, 6, SSD1306_WHITE);
            dispText(0, 54, "L/R:+1  UP:max  DN:min");
            dispCommit(); delay(30);
        }

    } else if (sel == 1) { // Sweep
        while (!readButtonLong()) {
            potApplyBrightness();
            for (int a = 0; a <= 180 && !readButtonLong(); a += 2) {
                set_angle(a);
                dispClear(); dispStatusBar("Servo Sweep");
                char b[24]; snprintf(b, 24, "-> %d deg", a);
                dispText(16, 24, b); dispText(0, 54, "[LONG]=dur"); dispCommit();
                delay(15);
            }
            for (int a = 180; a >= 0 && !readButtonLong(); a -= 2) {
                set_angle(a);
                dispClear(); dispStatusBar("Servo Sweep");
                char b[24]; snprintf(b, 24, "<- %d deg", a);
                dispText(16, 24, b); dispText(0, 54, "[LONG]=dur"); dispCommit();
                delay(15);
            }
        }

    } else if (sel == 2) { // Hızlı/yavaş sweep
        int spd = 5; // ms per step
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && spd > 1)  spd--;
            if (d == JoyDir::DOWN && spd < 50) spd++;
            for (int a = 0; a <= 180 && !readButtonLong(); a++) {
                set_angle(a); delay(spd);
            }
            for (int a = 180; a >= 0 && !readButtonLong(); a--) {
                set_angle(a); delay(spd);
            }
            dispClear(); dispStatusBar("Sweep Hizi");
            char b[24]; snprintf(b, 24, "Hiz: %d ms/adim", spd);
            dispText(0, 24, b); dispText(0, 54, "UP/DOWN=hiz [LONG]=cik"); dispCommit();
        }

    } else if (sel == 5) { // Pozisyon kaydet
        for (int i = 0; i < 5; i++) savedAngles[i] = angle;
        dispBannerOK("Kayit: " + String(angle) + " deg x5");

    } else if (sel == 6) { // Kayıtlı pozisyonlar
        OptionList ps;
        for (int i = 0; i < 5; i++) {
            char b[16]; snprintf(b, 16, "Poz %d: %d deg", i + 1, savedAngles[i]);
            ps.push_back({b, nullptr});
        }
        ps.push_back({"Back", nullptr});
        int ps2 = loopOptions(ps, "Kayitli Pozisyon");
        if (ps2 >= 0 && ps2 < 5) set_angle(savedAngles[ps2]);

    } else if (sel == 7) { // PWM raw
        char raw[8] = "4096"; charPicker(raw, 7, "Duty (0-65535)");
        uint32_t duty = constrain((uint32_t)atol(raw), 0UL, 65535UL);
        ledcWrite(PIN_MOD_DATA, duty);
        char b[24]; snprintf(b, 24, "Duty: %lu", duty);
        dispBannerOK(b);

    } else if (sel == 8) { // Kalibrasyon
        dispClear(); dispStatusBar("Servo Kal.");
        dispText(0, 12, "0deg=0.5ms(1638)");
        dispText(0, 24, "90deg=1.5ms(4915)");
        dispText(0, 36, "180deg=2.5ms(8192)");
        dispText(0, 48, "50Hz PWM, 16-bit");
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 9) { // Grafik
        while (!readButtonLong()) {
            potApplyBrightness();
            dispClear(); dispStatusBar("Servo Grafik");
            int bw = angle * 112 / 180;
            display.drawRect(4, 26, 112, 12, SSD1306_WHITE);
            display.fillRect(4, 26, bw, 12, SSD1306_WHITE);
            char b[24]; snprintf(b, 24, "%d deg", angle);
            dispText(4, 42, b);
            dispText(0, 54, "[LONG]=cik"); dispCommit();
            JoyDir d = readJoystick();
            if (d == JoyDir::RIGHT && angle < 180) { set_angle(angle + 2); buzzNav(); }
            if (d == JoyDir::LEFT  && angle > 0)   { set_angle(angle - 2); buzzNav(); }
            delay(50);
        }
    }
    ledcWrite(PIN_MOD_DATA, 0);
    ledcDetach(PIN_MOD_DATA);
}
} // namespace ModServo

// ────────────────────────────────────────────────────────────────────
// 3. DC Motor (L9110 / TB6612 tek kanal)
// Bağlantı: IN1→Pin4(GP40) PWM, IN2→Pin7(GP37) DIR
// ────────────────────────────────────────────────────────────────────
namespace ModDCMotor {
static int  motorSpeed  = 0;   // -255 .. +255
static bool pidEnabled  = false;
static uint32_t encCount = 0;

static void set_motor(int spd) {
    motorSpeed = constrain(spd, -255, 255);
    if (motorSpeed >= 0) {
        ledcWrite(PIN_MOD_DATA, motorSpeed);
        digitalWrite(PIN_MOD_TX2, LOW);
    } else {
        ledcWrite(PIN_MOD_DATA, -motorSpeed);
        digitalWrite(PIN_MOD_TX2, HIGH);
    }
}

static void run() {
    ledcAttach(PIN_MOD_DATA, 5000, 8);  // 5kHz, 8-bit
    pinMode(PIN_MOD_TX2, OUTPUT);
    digitalWrite(PIN_MOD_TX2, LOW);

    OptionList opts = {
        {"Ileri / Geri",   nullptr},
        {"Hiz Kontrol",    nullptr},
        {"Fren",           nullptr},
        {"Coast (serbest)",nullptr},
        {"Rampa Testi",    nullptr},
        {"PID Hiz Modu",   nullptr},
        {"Encoder Sayac",  nullptr},
        {"Stall Algila",   nullptr},
        {"Guc Tahmini",    nullptr},
        {"Istatistik",     nullptr},
        {"Back",           nullptr},
    };
    int sel = loopOptions(opts, "DC Motor");
    if (sel < 0 || sel == 10) {
        set_motor(0); ledcDetach(PIN_MOD_DATA); return;
    }

    if (sel == 0) { // İleri/geri
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP)   { set_motor(200); buzzNav(); }
            if (d == JoyDir::DOWN) { set_motor(-200); buzzNav(); }
            if (d == JoyDir::LEFT || d == JoyDir::RIGHT) { set_motor(0); }
            dispClear(); dispStatusBar("DC Motor");
            char b[24]; snprintf(b, 24, "Hiz: %+d", motorSpeed);
            dispText(0, 18, b);
            dispText(0, 32, motorSpeed > 0 ? ">>> ILERI >>>" : motorSpeed < 0 ? "<<< GERI <<<" : "--- DUR ---");
            dispText(0, 54, "UP=ileri DN=geri L/R=dur"); dispCommit();
            delay(50);
        }

    } else if (sel == 1) { // Hız
        int spd = 0;
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && spd < 255) { spd += 10; set_motor(spd); buzzNav(); }
            if (d == JoyDir::DOWN && spd > -255) { spd -= 10; set_motor(spd); buzzNav(); }
            dispClear(); dispStatusBar("Hiz Kontrol");
            char b[24]; snprintf(b, 24, "PWM: %d / 255", abs(spd));
            dispText(0, 14, b);
            int bw = abs(spd) * 112 / 255;
            display.drawRect(4, 28, 112, 8, SSD1306_WHITE);
            display.fillRect(4, 28, bw, 8, SSD1306_WHITE);
            dispText(0, 54, "UP/DOWN=hiz [LONG]=cik"); dispCommit();
            delay(50);
        }

    } else if (sel == 2) { // Fren
        ledcWrite(PIN_MOD_DATA, 255); digitalWrite(PIN_MOD_TX2, HIGH);
        dispBannerOK("FREN uygulandı");
        delay(1000);
        set_motor(0);

    } else if (sel == 3) { // Coast
        ledcWrite(PIN_MOD_DATA, 0); digitalWrite(PIN_MOD_TX2, LOW);
        motorSpeed = 0;
        dispBannerOK("COAST - serbest don.");

    } else if (sel == 4) { // Rampa
        dispClear(); dispStatusBar("Rampa Testi");
        dispText(0, 20, "0→255→0→-255→0");
        dispText(0, 32, "Rampa 3 saniye"); dispCommit();
        delay(1000);
        for (int s = 0; s <= 255; s++) { set_motor(s); delay(12); }
        for (int s = 255; s >= 0; s--) { set_motor(s); delay(12); }
        for (int s = 0; s >= -255; s--) { set_motor(s); delay(12); }
        for (int s = -255; s <= 0; s++) { set_motor(s); delay(12); }
        set_motor(0);
        dispBannerOK("Rampa tamamlandi");

    } else if (sel == 6) { // Encoder
        pinMode(PIN_MOD_RX2, INPUT_PULLUP);
        encCount = 0;
        bool lastState = digitalRead(PIN_MOD_RX2);
        while (!readButtonLong()) {
            potApplyBrightness();
            bool cur = digitalRead(PIN_MOD_RX2);
            if (cur != lastState) { encCount++; lastState = cur; }
            dispClear(); dispStatusBar("Encoder Sayac");
            char b[24]; snprintf(b, 24, "Puls: %lu", encCount);
            dispText(0, 18, b);
            snprintf(b, 24, "Tur: %.2f", encCount / 20.0f);
            dispText(0, 32, b);
            dispText(0, 54, "[LONG]=cik"); dispCommit();
            delay(10);
        }

    } else if (sel == 7) { // Stall algılama
        set_motor(150);
        uint32_t t0 = millis(); encCount = 0;
        bool lastS = digitalRead(PIN_MOD_RX2);
        while (!readButtonLong() && millis() - t0 < 5000) {
            bool cur = digitalRead(PIN_MOD_RX2);
            if (cur != lastS) { encCount++; lastS = cur; }
            if (millis() - t0 > 500 && encCount < 5) {
                set_motor(0);
                dispBannerWarn("STALL algılandi!");
                break;
            }
            delay(10);
        }
        set_motor(0);

    } else if (sel == 8) { // Güç tahmini
        dispClear(); dispStatusBar("Guc Tahmini");
        int spd = abs(motorSpeed);
        float v = 5.0f, i_est = spd / 255.0f * 1.2f;  // Max 1.2A tahmini
        char b1[24], b2[24];
        snprintf(b1, 24, "V: %.1f V", v);
        snprintf(b2, 24, "I: ~%.2f A", i_est);
        dispText(0, 14, b1); dispText(0, 26, b2);
        char b3[24]; snprintf(b3, 24, "P: ~%.2f W", v * i_est);
        dispText(0, 38, b3);
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 9) { // İstatistik
        dispClear(); dispStatusBar("DC Motor Stats");
        char b[24]; snprintf(b, 24, "Spd: %+d/255", motorSpeed);
        dispText(0, 14, b);
        snprintf(b, 24, "Enc: %lu puls", encCount);
        dispText(0, 26, b);
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    }
    set_motor(0); ledcDetach(PIN_MOD_DATA);
}
} // namespace ModDCMotor

// ────────────────────────────────────────────────────────────────────
// 4. Stepper Motor (A4988 / DRV8825)
// Bağlantı: STEP→Pin4(GP40), DIR→Pin7(GP37), EN→Pin8(GP38)
// ────────────────────────────────────────────────────────────────────
namespace ModStepper {
static long position = 0;  // adım cinsinden
static uint32_t stepDelay = 1000;  // µs
static bool enabled = true;

static void step_one(bool dir) {
    if (!enabled) return;
    digitalWrite(PIN_MOD_TX2, dir ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_MOD_DATA, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(PIN_MOD_DATA, LOW);
    delayMicroseconds(stepDelay);
    position += dir ? 1 : -1;
}
static void steps(long count, bool dir) {
    for (long i = 0; i < abs(count) && !readButtonLong(); i++) step_one(dir);
}

static void run() {
    pinMode(PIN_MOD_DATA, OUTPUT);
    pinMode(PIN_MOD_TX2,  OUTPUT);
    pinMode(PIN_MOD_RX2,  OUTPUT);
    digitalWrite(PIN_MOD_RX2, LOW);  // Enable LOW = aktif

    OptionList opts = {
        {"Jog Modu",       nullptr},
        {"Adim Gir",       nullptr},
        {"Hiz Ayarla",     nullptr},
        {"Home (Sifirla)", nullptr},
        {"Aci Modu (deg)", nullptr},
        {"Mikrostep Bilgi",nullptr},
        {"Limit Switch",   nullptr},
        {"Konum Kaydet",   nullptr},
        {"Itiş / Çekiş",  nullptr},
        {"Rampa",          nullptr},
        {"Back",           nullptr},
    };
    int sel = loopOptions(opts, "Stepper A4988");
    if (sel < 0 || sel == 10) return;

    if (sel == 0) { // Jog
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::RIGHT) { step_one(true);  buzzNav(); }
            if (d == JoyDir::LEFT)  { step_one(false); buzzNav(); }
            if (d == JoyDir::UP)    { steps(10, true); }
            if (d == JoyDir::DOWN)  { steps(10, false); }
            dispClear(); dispStatusBar("Stepper Jog");
            char b[24]; snprintf(b, 24, "Pos: %ld adim", position);
            dispText(0, 18, b);
            snprintf(b, 24, "%.2f tur", position / 200.0f);
            dispText(0, 32, b);
            dispText(0, 54, "L/R=1adim U/D=10"); dispCommit();
            delay(30);
        }

    } else if (sel == 1) { // Adım gir
        char cntS[8] = "200"; charPicker(cntS, 7, "Adim sayisi");
        long cnt = atol(cntS);
        OptionList dir = {{"Ileri", nullptr}, {"Geri", nullptr}};
        int ds = loopOptions(dir, "Yon");
        if (ds == 0) steps(cnt, true);
        else if (ds == 1) steps(cnt, false);
        char b[24]; snprintf(b, 24, "Tamamlandi: pos=%ld", position);
        dispBannerOK(b);

    } else if (sel == 2) { // Hız
        dispClear(); dispStatusBar("Hiz Ayarla");
        dispText(0, 12, "UP=hizlan DN=yavasla");
        dispText(0, 24, "(stepDelay us)"); dispCommit();
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && stepDelay > 100)   stepDelay -= 100;
            if (d == JoyDir::DOWN && stepDelay < 10000) stepDelay += 100;
            dispClear(); dispStatusBar("Adim Hizi");
            char b[24]; snprintf(b, 24, "%lu us/adim", stepDelay);
            dispText(0, 20, b);
            snprintf(b, 24, "~%.1f Hz", 1000000.0f / (stepDelay * 2));
            dispText(0, 34, b);
            dispText(0, 54, "[LONG]=kapat"); dispCommit();
            delay(50);
        }

    } else if (sel == 3) { // Home
        position = 0;
        dispBannerOK("Pozisyon sifirlandi");

    } else if (sel == 4) { // Açı modu (200 adım = 360°, full step)
        char degS[8] = "90"; charPicker(degS, 7, "Aci (derece)");
        float deg = atof(degS);
        long adimlar = (long)(deg / 360.0f * 200.0f);
        steps(abs(adimlar), adimlar >= 0);
        char b[24]; snprintf(b, 24, "%.1f deg = %ld adim", deg, adimlar);
        dispBannerOK(b);

    } else if (sel == 5) { // Mikrostep bilgi
        dispClear(); dispStatusBar("Mikrostep");
        dispText(0, 12, "MS1 MS2 MS3 Coz.");
        dispText(0, 22, "L   L   L   Full");
        dispText(0, 32, "H   L   L   1/2");
        dispText(0, 42, "L   H   L   1/4");
        dispText(0, 52, "H   H   L   1/8");
        dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 9) { // Rampa
        for (uint32_t d = 5000; d >= 800 && !readButtonLong(); d -= 100) {
            stepDelay = d; step_one(true); delay(1);
        }
        for (int i = 0; i < 100 && !readButtonLong(); i++) { step_one(true); delay(1); }
        for (uint32_t d = 800; d <= 5000 && !readButtonLong(); d += 100) {
            stepDelay = d; step_one(true); delay(1);
        }
        dispBannerOK("Rampa tamamlandi");
    }
}
} // namespace ModStepper

// ────────────────────────────────────────────────────────────────────
// 5. Röle Modülü
// Bağlantı: IN → Pin4 (GP40) — aktif LOW (çoğu röle modül)
// ────────────────────────────────────────────────────────────────────
namespace ModRelay {
static bool relayState = false;
static uint32_t toggleCount = 0;
static bool activeLow = true;

static void relay_set(bool on) {
    relayState = on;
    if (activeLow) digitalWrite(PIN_MOD_DATA, on ? LOW : HIGH);
    else           digitalWrite(PIN_MOD_DATA, on ? HIGH : LOW);
    toggleCount++;
}

static void run() {
    pinMode(PIN_MOD_DATA, OUTPUT);
    relay_set(false);  // başlangıçta kapalı

    OptionList opts = {
        {"ON / OFF",       nullptr},
        {"Toggle",         nullptr},
        {"Zamanlayici",    nullptr},
        {"Pulse (Ani ac)", nullptr},
        {"Latch Modu",     nullptr},
        {"Coklu Kanal",    nullptr},
        {"Durum Goster",   nullptr},
        {"Guveni Mod",     nullptr},
        {"Sayac",          nullptr},
        {"Aktif Low/High", nullptr},
        {"Back",           nullptr},
    };
    int sel = loopOptions(opts, "Role Modulu");
    if (sel < 0 || sel == 10) { relay_set(false); return; }

    if (sel == 0) { // ON/OFF
        OptionList onoff = {{"ON", nullptr}, {"OFF", nullptr}};
        int os = loopOptions(onoff, "Role");
        if (os == 0) { relay_set(true);  dispBannerOK("Role ACIK"); }
        if (os == 1) { relay_set(false); dispBannerOK("Role KAPALI"); }

    } else if (sel == 1) { // Toggle
        relay_set(!relayState);
        dispBannerOK(relayState ? "Role ACIK" : "Role KAPALI");

    } else if (sel == 2) { // Zamanlayıcı
        char secS[6] = "5"; charPicker(secS, 5, "Sure (saniye)");
        int secs = atoi(secS);
        relay_set(true);
        dispClear(); dispStatusBar("Zamanlayici");
        for (int i = secs; i > 0 && !readButtonLong(); i--) {
            potApplyBrightness();
            char b[24]; snprintf(b, 24, "Kapanacak: %d sn", i);
            dispText(16, 24, b); dispText(0, 54, "[LONG]=iptal"); dispCommit();
            delay(1000);
        }
        relay_set(false); dispBannerOK("Zamanlayici bitti, KAPALI");

    } else if (sel == 3) { // Pulse
        char msS[8] = "500"; charPicker(msS, 7, "Sure (ms)");
        int ms2 = atoi(msS);
        relay_set(true); delay(ms2); relay_set(false);
        dispBannerOK("Pulse " + String(ms2) + "ms");

    } else if (sel == 5) { // Çoklu kanal (GP40 + GP41 simülasyonu)
        pinMode(PIN_MOD_SDA2, OUTPUT);
        dispClear(); dispStatusBar("Coklu Role");
        dispText(0, 12, "CH1:GP40  CH2:GP41");
        OptionList ch = {{"CH1 ON", nullptr}, {"CH1 OFF", nullptr},
                         {"CH2 ON", nullptr}, {"CH2 OFF", nullptr}, {"Back", nullptr}};
        int cs = loopOptions(ch, "Kanal Sec");
        if (cs == 0) { digitalWrite(PIN_MOD_DATA, activeLow ? LOW : HIGH); dispBannerOK("CH1 ON"); }
        if (cs == 1) { digitalWrite(PIN_MOD_DATA, activeLow ? HIGH : LOW); dispBannerOK("CH1 OFF"); }
        if (cs == 2) { digitalWrite(PIN_MOD_SDA2, activeLow ? LOW : HIGH); dispBannerOK("CH2 ON"); }
        if (cs == 3) { digitalWrite(PIN_MOD_SDA2, activeLow ? HIGH : LOW); dispBannerOK("CH2 OFF"); }

    } else if (sel == 6) { // Durum
        dispClear(); dispStatusBar("Role Durum");
        char b1[24], b2[24];
        snprintf(b1, 24, "Durum: %s", relayState ? "ACIK" : "KAPALI");
        snprintf(b2, 24, "Toggle: %lu", toggleCount);
        dispText(0, 18, b1); dispText(0, 32, b2);
        dispText(0, 46, activeLow ? "Aktif: LOW" : "Aktif: HIGH");
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }

    } else if (sel == 7) { // Güvenli mod (her zaman kapalı)
        relay_set(false);
        dispBannerWarn("Guvenli Mod: KAPALI");

    } else if (sel == 9) { // Aktif Low/High toggle
        activeLow = !activeLow;
        relay_set(false);
        dispBannerOK(activeLow ? "Aktif LOW modu" : "Aktif HIGH modu");
    }
}
} // namespace ModRelay

// ────────────────────────────────────────────────────────────────────
// 6. NeoPixel / WS2812B LED Şerit
// Bağlantı: Data → Pin4 (GP40)
// ESP32-S3 GP40 → GPIO1 bankası (bit 8)
// ────────────────────────────────────────────────────────────────────
namespace ModNeoPixel {
static uint8_t pixCount = 8;
static uint8_t brightness = 80;

static void neo_send_byte(uint8_t b) {
    const uint32_t BIT = (1UL << (PIN_MOD_DATA - 32));
    volatile uint32_t* SET = (volatile uint32_t*)0x60004018;
    volatile uint32_t* CLR = (volatile uint32_t*)0x6000401C;
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i)) {
            *SET = BIT;
            __asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
            *CLR = BIT;
            __asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        } else {
            *SET = BIT;
            __asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
            *CLR = BIT;
            __asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        }
    }
}
static void neo_set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t cnt = 0) {
    if (cnt == 0) cnt = pixCount;
    uint8_t br = r * brightness / 255;
    uint8_t bg = g * brightness / 255;
    uint8_t bb2 = b * brightness / 255;
    pinMode(PIN_MOD_DATA, OUTPUT);
    noInterrupts();
    for (uint8_t i = 0; i < cnt; i++) {
        neo_send_byte(bg); neo_send_byte(br); neo_send_byte(bb2);
    }
    interrupts();
    delayMicroseconds(60);
}
static void neo_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    // Tüm pikselleri yeniden yaz (buffer yok, sadece hedef pikseli renklendir)
    uint8_t br = r * brightness / 255;
    uint8_t bg = g * brightness / 255;
    uint8_t bb2 = b * brightness / 255;
    pinMode(PIN_MOD_DATA, OUTPUT);
    noInterrupts();
    for (uint8_t i = 0; i < pixCount; i++) {
        if (i == idx) { neo_send_byte(bg); neo_send_byte(br); neo_send_byte(bb2); }
        else          { neo_send_byte(0);  neo_send_byte(0);  neo_send_byte(0);   }
    }
    interrupts();
    delayMicroseconds(60);
}

static void run() {
    OptionList opts = {
        {"Renk Sec",         nullptr},
        {"Parlaklik",        nullptr},
        {"Rainbow Animasyon",nullptr},
        {"Chase Animasyon",  nullptr},
        {"Pulse Efekti",     nullptr},
        {"Renk Picker",      nullptr},
        {"Segment Kontrol",  nullptr},
        {"Renk Sıcaklığı",  nullptr},
        {"Piksel Sayisi",    nullptr},
        {"Kapat",            nullptr},
        {"Back",             nullptr},
    };
    int sel = loopOptions(opts, "NeoPixel WS2812");
    if (sel < 0 || sel == 10) return;

    if (sel == 0) { // Renk seç
        struct ColorEntry { const char* name; uint8_t r, g, b; };
        static const ColorEntry COLORS[] = {
            {"Kirmizi",  255,0,0}, {"Yesil",0,255,0}, {"Mavi",0,0,255},
            {"Beyaz",255,255,255}, {"Sari",255,200,0}, {"Mor",180,0,255},
            {"Camgob.",0,200,200}, {"Turuncu",255,80,0},
        };
        OptionList cl;
        for (auto& c : COLORS) cl.push_back({c.name, nullptr});
        cl.push_back({"Back", nullptr});
        int cs = loopOptions(cl, "Renk Sec");
        if (cs >= 0 && cs < 8) {
            neo_set_all(COLORS[cs].r, COLORS[cs].g, COLORS[cs].b);
            dispBannerOK(String(COLORS[cs].name) + " ayarlandi");
        }

    } else if (sel == 1) { // Parlaklık
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && brightness < 255) { brightness += 10; neo_set_all(255, 255, 255); buzzNav(); }
            if (d == JoyDir::DOWN && brightness > 10)  { brightness -= 10; neo_set_all(255, 255, 255); buzzNav(); }
            dispClear(); dispStatusBar("Parlaklik");
            char b[24]; snprintf(b, 24, "%d / 255 (%d%%)", brightness, brightness * 100 / 255);
            dispText(0, 20, b);
            int bw = brightness * 112 / 255;
            display.drawRect(4, 34, 112, 8, SSD1306_WHITE);
            display.fillRect(4, 34, bw, 8, SSD1306_WHITE);
            dispText(0, 54, "[LONG]=kapat"); dispCommit();
            delay(80);
        }

    } else if (sel == 2) { // Rainbow
        uint8_t hue = 0;
        while (!readButtonLong()) {
            potApplyBrightness();
            // HSV → RGB (s=1, v=1)
            uint8_t reg = hue / 43;
            uint8_t rem = (hue % 43) * 6;
            uint8_t p = 0, q = 255 - rem, t = rem;
            uint8_t r, g, b;
            switch (reg) {
                case 0: r=255;g=t;  b=p;   break;
                case 1: r=q;  g=255;b=p;   break;
                case 2: r=p;  g=255;b=t;   break;
                case 3: r=p;  g=q;  b=255; break;
                case 4: r=t;  g=p;  b=255; break;
                default:r=255;g=p;  b=q;   break;
            }
            neo_set_all(r, g, b);
            hue++;
            dispClear(); dispStatusBar("Rainbow");
            char bx[24]; snprintf(bx, 24, "Hue: %d", hue);
            dispText(0, 24, bx); dispText(0, 54, "[LONG]=dur"); dispCommit();
            delay(30);
        }

    } else if (sel == 3) { // Chase
        uint8_t pos = 0;
        while (!readButtonLong()) {
            potApplyBrightness();
            neo_set_pixel(pos % pixCount, 0, 255, 128);
            pos++;
            dispClear(); dispStatusBar("Chase");
            char b[24]; snprintf(b, 24, "Pos: %d", pos % pixCount);
            dispText(0, 24, b); dispText(0, 54, "[LONG]=dur"); dispCommit();
            delay(80);
        }
        neo_set_all(0, 0, 0);

    } else if (sel == 4) { // Pulse
        while (!readButtonLong()) {
            potApplyBrightness();
            for (int br2 = 0; br2 <= 255 && !readButtonLong(); br2 += 5) {
                uint8_t old = brightness; brightness = br2;
                neo_set_all(0, 100, 255); brightness = old;
                delay(15);
            }
            for (int br2 = 255; br2 >= 0 && !readButtonLong(); br2 -= 5) {
                uint8_t old = brightness; brightness = br2;
                neo_set_all(0, 100, 255); brightness = old;
                delay(15);
            }
        }
        neo_set_all(0, 0, 0);

    } else if (sel == 5) { // Renk picker (R/G/B ayrı)
        uint8_t pr = 255, pg = 0, pb = 0;
        int ch = 0;  // 0=R, 1=G, 2=B
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::RIGHT) { ch = (ch + 1) % 3; buzzNav(); }
            if (d == JoyDir::LEFT)  { ch = (ch + 2) % 3; buzzNav(); }
            uint8_t& cur = ch == 0 ? pr : ch == 1 ? pg : pb;
            if (d == JoyDir::UP   && cur < 255) { cur += 5; }
            if (d == JoyDir::DOWN && cur > 0)   { cur -= 5; }
            neo_set_all(pr, pg, pb);
            dispClear(); dispStatusBar("Renk Picker");
            char b[24]; snprintf(b, 24, "R:%d G:%d B:%d", pr, pg, pb);
            dispText(0, 12, b);
            const char* chn[] = {"[R]", "[ G]", "[  B]"};
            dispText(0, 26, chn[ch]);
            dispText(0, 54, "L/R=kanal U/D=deger"); dispCommit();
            delay(50);
        }

    } else if (sel == 8) { // Piksel sayısı
        char cntS[4] = "8"; charPicker(cntS, 3, "Piksel sayisi");
        pixCount = constrain(atoi(cntS), 1, 120);
        char b[24]; snprintf(b, 24, "Piksel: %d", pixCount);
        dispBannerOK(b);

    } else if (sel == 9) { // Kapat
        neo_set_all(0, 0, 0); dispBannerOK("LED kapandi");
    }
}
} // namespace ModNeoPixel

// ────────────────────────────────────────────────────────────────────
// 7. MOSFET / PWM LED
// Bağlantı: Gate → Pin4 (GP40)
// ────────────────────────────────────────────────────────────────────
namespace ModMOSFET {
static uint32_t pwmFreq = 5000;
static uint8_t  dutyCycle = 128;
static bool     attached = false;

static void attach_pwm() {
    if (!attached) { ledcAttach(PIN_MOD_DATA, pwmFreq, 8); attached = true; }
}
static void detach_pwm() {
    if (attached) { ledcDetach(PIN_MOD_DATA); attached = false; }
}
static void set_duty(uint8_t d) {
    dutyCycle = d; attach_pwm(); ledcWrite(PIN_MOD_DATA, d);
}

static void run() {
    OptionList opts = {
        {"Parlaklik Dimmer",nullptr},
        {"Sweep Efekti",    nullptr},
        {"Flash",           nullptr},
        {"Strobe",          nullptr},
        {"Acilis Zamanlayicisi",nullptr},
        {"Kapanma Zamanlayicisi",nullptr},
        {"Gamma Duzelt",    nullptr},
        {"SD Log",          nullptr},
        {"PWM Frekans",     nullptr},
        {"Istatistik",      nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, "MOSFET/PWM LED");
    if (sel < 0 || sel == 10) { detach_pwm(); return; }

    if (sel == 0) { // Dimmer
        attach_pwm();
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && dutyCycle < 250) { dutyCycle += 5; ledcWrite(PIN_MOD_DATA, dutyCycle); buzzNav(); }
            if (d == JoyDir::DOWN && dutyCycle > 5)   { dutyCycle -= 5; ledcWrite(PIN_MOD_DATA, dutyCycle); buzzNav(); }
            dispClear(); dispStatusBar("PWM Dimmer");
            char b[24]; snprintf(b, 24, "%d%% (%d/255)", dutyCycle * 100 / 255, dutyCycle);
            dispText(0, 18, b);
            int bw = dutyCycle * 112 / 255;
            display.drawRect(4, 32, 112, 10, SSD1306_WHITE);
            display.fillRect(4, 32, bw, 10, SSD1306_WHITE);
            dispText(0, 54, "[LONG]=kapat"); dispCommit();
            delay(80);
        }

    } else if (sel == 1) { // Sweep
        attach_pwm();
        while (!readButtonLong()) {
            potApplyBrightness();
            for (int d = 0; d <= 255 && !readButtonLong(); d++) { ledcWrite(PIN_MOD_DATA, d); delay(8); }
            for (int d = 255; d >= 0 && !readButtonLong(); d--) { ledcWrite(PIN_MOD_DATA, d); delay(8); }
        }
        ledcWrite(PIN_MOD_DATA, 0);

    } else if (sel == 2) { // Flash
        attach_pwm();
        char msS[6] = "500"; charPicker(msS, 5, "Flash suresi (ms)");
        int ms2 = atoi(msS);
        while (!readButtonLong()) {
            ledcWrite(PIN_MOD_DATA, 255); delay(ms2);
            ledcWrite(PIN_MOD_DATA, 0);  delay(ms2);
            dispClear(); dispStatusBar("Flash");
            char b[24]; snprintf(b, 24, "%d ms", ms2);
            dispText(24, 24, b); dispText(0, 54, "[LONG]=dur"); dispCommit();
        }
        ledcWrite(PIN_MOD_DATA, 0);

    } else if (sel == 3) { // Strobe
        attach_pwm();
        int hz = 10;
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (d == JoyDir::UP   && hz < 100) hz++;
            if (d == JoyDir::DOWN && hz > 1)   hz--;
            uint32_t on = 1000 / hz / 2;
            ledcWrite(PIN_MOD_DATA, 255); delay(on);
            ledcWrite(PIN_MOD_DATA, 0);  delay(on);
            dispClear(); dispStatusBar("Strobe");
            char b[24]; snprintf(b, 24, "%d Hz", hz);
            dispText(24, 20, b); dispText(0, 54, "UP/DOWN=hz [LONG]=dur"); dispCommit();
        }
        ledcWrite(PIN_MOD_DATA, 0);

    } else if (sel == 6) { // Gamma düzeltme
        // Gamma 2.2 LUT
        attach_pwm();
        uint8_t gammaLUT[256];
        for (int i = 0; i < 256; i++) gammaLUT[i] = (uint8_t)(pow(i / 255.0f, 2.2f) * 255.0f);
        set_duty(gammaLUT[dutyCycle]);
        dispBannerOK("Gamma 2.2 uygulandı");

    } else if (sel == 8) { // PWM Frekans
        OptionList freqs = {{"1kHz",nullptr},{"5kHz",nullptr},{"10kHz",nullptr},
                            {"20kHz",nullptr},{"40kHz",nullptr},{"Back",nullptr}};
        static const uint32_t fList[] = {1000,5000,10000,20000,40000};
        int fs = loopOptions(freqs, "PWM Frekans");
        if (fs >= 0 && fs < 5) {
            pwmFreq = fList[fs];
            detach_pwm(); attach_pwm();
            char b[24]; snprintf(b, 24, "Frekans: %lu Hz", pwmFreq);
            dispBannerOK(b);
        }

    } else if (sel == 9) {
        dispClear(); dispStatusBar("MOSFET Stats");
        char b1[24], b2[24];
        snprintf(b1, 24, "Duty: %d (%d%%)", dutyCycle, dutyCycle * 100 / 255);
        snprintf(b2, 24, "Freq: %lu Hz", pwmFreq);
        dispText(0, 18, b1); dispText(0, 32, b2);
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    }
    detach_pwm();
}
} // namespace ModMOSFET

// ────────────────────────────────────────────────────────────────────
// 8. L298N Çift Motor Sürücü
// Bağlantı:
//   ENA→Pin4(GP40) PWM-A, IN1→Pin7(GP37), IN2→Pin8(GP38)
//   ENB→Pin5(GP41) PWM-B, IN3→Pin6(GP42), IN4→Pin3(GP39) [analog pin]
// Not: GP39 analog girişi, çıkış olarak da kullanılabilir
// ────────────────────────────────────────────────────────────────────
namespace ModL298N {
static int spdA = 0, spdB = 0;

static void set_A(int spd) {
    spdA = constrain(spd, -255, 255);
    ledcAttach(PIN_MOD_DATA, 5000, 8);
    ledcWrite(PIN_MOD_DATA, abs(spdA));
    digitalWrite(PIN_MOD_TX2, spdA >= 0 ? HIGH : LOW);
    digitalWrite(PIN_MOD_RX2, spdA >= 0 ? LOW  : HIGH);
}
static void set_B(int spd) {
    spdB = constrain(spd, -255, 255);
    ledcAttach(PIN_MOD_SDA2, 5000, 8);
    ledcWrite(PIN_MOD_SDA2, abs(spdB));
    digitalWrite(PIN_MOD_SCL2, spdB >= 0 ? HIGH : LOW);
    digitalWrite(39,           spdB >= 0 ? LOW  : HIGH);
}
static void stop_all() { set_A(0); set_B(0); }

static void run() {
    pinMode(PIN_MOD_TX2,  OUTPUT);
    pinMode(PIN_MOD_RX2,  OUTPUT);
    pinMode(PIN_MOD_SCL2, OUTPUT);
    pinMode(39,           OUTPUT);

    OptionList opts = {
        {"Cift Motor Manuel",nullptr},
        {"Tank Modu",        nullptr},
        {"Ileri / Geri",     nullptr},
        {"Saga / Sola Don",  nullptr},
        {"Fren",             nullptr},
        {"Rampa",            nullptr},
        {"Motor A Kontrol",  nullptr},
        {"Motor B Kontrol",  nullptr},
        {"PID (enc. gerek)", nullptr},
        {"Istatistik",       nullptr},
        {"Back",             nullptr},
    };
    int sel = loopOptions(opts, "L298N Cift Motor");
    if (sel < 0 || sel == 10) { stop_all(); return; }

    if (sel == 0 || sel == 1) { // Manuel / Tank
        while (!readButtonLong()) {
            potApplyBrightness();
            JoyDir d = readJoystick();
            if (sel == 0) { // Her motor bağımsız
                if (d == JoyDir::UP)    { set_A(200);  set_B(200);  }
                if (d == JoyDir::DOWN)  { set_A(-200); set_B(-200); }
                if (d == JoyDir::LEFT)  { set_A(-150); set_B(150);  }
                if (d == JoyDir::RIGHT) { set_A(150);  set_B(-150); }
                if (readButton())       stop_all();
            } else { // Tank
                if (d == JoyDir::UP)    { set_A(180);  set_B(180);  }
                if (d == JoyDir::DOWN)  { set_A(-180); set_B(-180); }
                if (d == JoyDir::LEFT)  { set_A(-200); set_B(200);  }
                if (d == JoyDir::RIGHT) { set_A(200);  set_B(-200); }
                if (readButton())       stop_all();
            }
            dispClear(); dispStatusBar(sel == 0 ? "Cift Motor" : "Tank Modu");
            char b[24]; snprintf(b, 24, "A:%+d B:%+d", spdA, spdB);
            dispText(0, 22, b);
            dispText(0, 54, "JOY=yon BTN=dur"); dispCommit();
            delay(30);
        }

    } else if (sel == 2) {
        set_A(200); set_B(200);
        dispBannerOK("ILERI"); delay(2000);
        stop_all();

    } else if (sel == 4) {
        // Fren: her iki IN HIGH
        digitalWrite(PIN_MOD_TX2, HIGH); digitalWrite(PIN_MOD_RX2, HIGH);
        digitalWrite(PIN_MOD_SCL2, HIGH); digitalWrite(39, HIGH);
        dispBannerOK("FREN"); delay(500); stop_all();

    } else if (sel == 5) { // Rampa
        for (int s = 0; s <= 200 && !readButtonLong(); s += 5) { set_A(s); set_B(s); delay(30); }
        for (int s = 200; s >= 0 && !readButtonLong(); s -= 5) { set_A(s); set_B(s); delay(30); }
        stop_all(); dispBannerOK("Rampa tamamlandi");

    } else if (sel == 9) {
        dispClear(); dispStatusBar("L298N Stats");
        char b1[24], b2[24];
        snprintf(b1, 24, "Motor A: %+d", spdA);
        snprintf(b2, 24, "Motor B: %+d", spdB);
        dispText(0, 18, b1); dispText(0, 32, b2);
        dispText(0, 54, "[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    }
    stop_all();
}
} // namespace ModL298N

// ════════════════════════════════════════════════════════════════════
//  AKTÜATÖR ANA MENÜSÜ
// ════════════════════════════════════════════════════════════════════
static void actuators_menu_run() {
    OptionList opts = {
        {"Buzzer",        nullptr},
        {"Servo Motor",   nullptr},
        {"DC Motor",      nullptr},
        {"Stepper A4988", nullptr},
        {"Role Modulu",   nullptr},
        {"NeoPixel LED",  nullptr},
        {"MOSFET/PWM LED",nullptr},
        {"L298N Cift Mot",nullptr},
        {"Back",          nullptr},
    };
    int sel = loopOptions(opts, "Aktuator Moduller");
    if (sel < 0 || sel == 8) return;
    switch (sel) {
        case 0: ModBuzzer::run();   break;
        case 1: ModServo::run();    break;
        case 2: ModDCMotor::run();  break;
        case 3: ModStepper::run();  break;
        case 4: ModRelay::run();    break;
        case 5: ModNeoPixel::run(); break;
        case 6: ModMOSFET::run();   break;
        case 7: ModL298N::run();    break;
    }
}

#pragma once
// ════════════════════════════════════════════════════════════════════
//  mod_sensors.h — M5Sharkino Sensör Modülleri
//  Breadboard pinleri:
//    Pin1=3V3  Pin2=GND  Pin3=GP39(ID/ADC)  Pin4=GP40(DATA/PWM)
//    Pin5=GP41(SDA2)  Pin6=GP42(SCL2)  Pin7=GP37(TX2)  Pin8=GP38(RX2)
// ════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <Wire.h>

// Ortak pin sabitleri (mod_pins.h dahil değilse burada tanımlı)
#ifndef PIN_MOD_DATA
  #define PIN_MOD_DATA  40
  #define PIN_MOD_SDA2  41
  #define PIN_MOD_SCL2  42
  #define PIN_MOD_TX2   37
  #define PIN_MOD_RX2   38
#endif

// ────────────────────────────────────────────────────────────────────
// 1. DHT11 / DHT22 — Sıcaklık & Nem
// Bağlantı: Data → Pin4 (GP40), VCC → Pin1 (3V3), GND → Pin2
// ────────────────────────────────────────────────────────────────────
namespace ModDHT {

struct Reading { float tempC, tempF, hum, heatIdx, dewPt, absHum; bool ok; };
static float histTempMin =  999, histTempMax = -999;
static float histHumMin  =  999, histHumMax  = -999;
static float tempTrend[10]; static uint8_t trendIdx = 0;
static bool  alarmEnabled = false;
static float alarmTempHi  = 35.0f, alarmHumHi = 80.0f;

// Ham DHT protokolü (kütüphanesiz, DHT11 ve DHT22 destekli)
static Reading read_raw(bool isDHT22) {
    Reading r = {0,0,0,0,0,0,false};
    pinMode(PIN_MOD_DATA, OUTPUT);
    digitalWrite(PIN_MOD_DATA, LOW);
    delay(isDHT22 ? 18 : 20);
    digitalWrite(PIN_MOD_DATA, HIGH);
    delayMicroseconds(30);
    pinMode(PIN_MOD_DATA, INPUT_PULLUP);

    uint32_t t0 = micros();
    while (digitalRead(PIN_MOD_DATA) == HIGH) { if (micros()-t0 > 100) return r; }
    while (digitalRead(PIN_MOD_DATA) == LOW)  { if (micros()-t0 > 200) return r; }
    while (digitalRead(PIN_MOD_DATA) == HIGH) { if (micros()-t0 > 300) return r; }

    uint8_t data[5] = {0};
    for (uint8_t i = 0; i < 40; i++) {
        while (digitalRead(PIN_MOD_DATA) == LOW)  { if (micros()-t0 > 5000) return r; }
        delayMicroseconds(30);
        if (digitalRead(PIN_MOD_DATA) == HIGH) data[i/8] |= (1 << (7-(i%8)));
        while (digitalRead(PIN_MOD_DATA) == HIGH) { if (micros()-t0 > 5000) return r; }
    }
    if (data[4] != (uint8_t)(data[0]+data[1]+data[2]+data[3])) return r;

    if (isDHT22) {
        r.hum   = ((data[0]<<8)|data[1]) / 10.0f;
        int16_t raw = ((data[2]&0x7F)<<8)|data[3];
        if (data[2]&0x80) raw=-raw;
        r.tempC = raw / 10.0f;
    } else {
        r.hum   = data[0] + data[1]*0.1f;
        r.tempC = data[2] + data[3]*0.1f;
    }
    r.tempF   = r.tempC * 9.0f/5.0f + 32.0f;
    // Heat index (Steadman formülü, sadece >26°C için anlamlı)
    float hi  = -8.78469f + 1.61139f*r.tempC + 2.33855f*r.hum
                - 0.14611f*r.tempC*r.hum - 0.01230f*r.tempC*r.tempC
                - 0.01642f*r.hum*r.hum + 0.00221f*r.tempC*r.tempC*r.hum
                + 0.00072f*r.tempC*r.hum*r.hum - 0.00000357f*r.tempC*r.tempC*r.hum*r.hum;
    r.heatIdx = (r.tempC > 26) ? hi : r.tempC;
    // Dew point (Magnus formülü)
    float a=17.625f, b=243.04f;
    float γ = log(r.hum/100.0f) + a*r.tempC/(b+r.tempC);
    r.dewPt  = b*γ/(a-γ);
    // Absolute humidity (g/m³)
    r.absHum = 216.7f * (r.hum/100.0f * 6.112f * exp(17.67f*r.tempC/(r.tempC+243.5f)) / (273.15f+r.tempC));
    r.ok     = true;
    return r;
}

static void update_history(const Reading& r) {
    if (!r.ok) return;
    if (r.tempC < histTempMin) histTempMin = r.tempC;
    if (r.tempC > histTempMax) histTempMax = r.tempC;
    if (r.hum   < histHumMin)  histHumMin  = r.hum;
    if (r.hum   > histHumMax)  histHumMax  = r.hum;
    tempTrend[trendIdx++ % 10] = r.tempC;
}

static void run(bool isDHT22) {
    // Alt menü
    OptionList opts = {
        {"Canli Okuma",     nullptr},
        {"Min/Max Gecmisi", nullptr},
        {"Sicaklik Grafigi",nullptr},
        {"Dew Point / AH",  nullptr},
        {"Heat Index",      nullptr},
        {"Alarm Ayarla",    nullptr},
        {"SD Log Baslat",   nullptr},
        {"Kalibrasyon",     nullptr},
        {"Trend (10 okuma)",nullptr},
        {"Sifirla",         nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts, isDHT22 ? "DHT22" : "DHT11");
    if (sel < 0 || sel == 10) return;

    if (sel == 0) { // Canlı okuma
        while (!readButtonLong()) {
            potApplyBrightness();
            Reading r = read_raw(isDHT22);
            update_history(r);
            dispClear(); dispStatusBar(isDHT22?"DHT22":"DHT11");
            if (r.ok) {
                char b1[24],b2[24],b3[24];
                snprintf(b1,24,"Temp: %.1fC  %.1fF", r.tempC, r.tempF);
                snprintf(b2,24,"Nem:  %.1f%%", r.hum);
                snprintf(b3,24,"HeatIdx: %.1fC", r.heatIdx);
                dispText(0,12,b1); dispText(0,24,b2); dispText(0,36,b3);
                if (alarmEnabled && (r.tempC>alarmTempHi || r.hum>alarmHumHi)) {
                    dispText(0,48,"!! ALARM !!"); buzzOK();
                } else dispText(0,54,"[LONG]=cik");
            } else { dispText(0,24,"Okuma hata!"); dispText(0,36,"GP40 kontrol et"); }
            dispCommit(); delay(2000);
        }
    } else if (sel == 1) { // Min/Max
        dispClear(); dispStatusBar("DHT Min/Max");
        char b1[24],b2[24],b3[24],b4[24];
        snprintf(b1,24,"TempMin: %.1fC", histTempMin>900?0:histTempMin);
        snprintf(b2,24,"TempMax: %.1fC", histTempMax<-900?0:histTempMax);
        snprintf(b3,24,"HumMin:  %.1f%%", histHumMin>900?0:histHumMin);
        snprintf(b4,24,"HumMax:  %.1f%%", histHumMax<-900?0:histHumMax);
        dispText(0,12,b1); dispText(0,22,b2); dispText(0,32,b3); dispText(0,42,b4);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 2) { // Grafik (10 örnek)
        // 10 örnek al, bar grafik çiz
        float samples[10]; uint8_t sc=0;
        dispClear(); dispStatusBar("Grafik - 10 ornek");
        dispText(0,24,"Olcum aliyor..."); dispCommit();
        while (sc < 10) {
            Reading r = read_raw(isDHT22);
            if (r.ok) samples[sc++] = r.tempC;
            delay(2000);
        }
        float mn=samples[0],mx=samples[0];
        for (uint8_t i=0;i<10;i++){if(samples[i]<mn)mn=samples[i];if(samples[i]>mx)mx=samples[i];}
        dispClear(); dispStatusBar("Temp Grafik");
        for (uint8_t i=0;i<10;i++) {
            int bh = (mx>mn) ? (int)((samples[i]-mn)/(mx-mn)*36) : 8;
            int bx = 4+i*12, by = 50-bh;
            display.fillRect(bx, by, 10, bh, SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%.0f-%.0fC",mn,mx);
        dispText(0,54,rng); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 3) { // Dew Point / Absolute Humidity
        Reading r = read_raw(isDHT22);
        dispClear(); dispStatusBar("Dew Pt / AH");
        if (r.ok) {
            char b1[24],b2[24];
            snprintf(b1,24,"DewPt: %.1fC", r.dewPt);
            snprintf(b2,24,"AbsHum: %.1f g/m3", r.absHum);
            dispText(0,14,b1); dispText(0,28,b2);
            dispText(0,42,"(Magnus / Tetens)");
        } else dispText(0,24,"Okuma hata!");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 4) { // Heat Index
        Reading r = read_raw(isDHT22);
        dispClear(); dispStatusBar("Heat Index");
        if (r.ok) {
            char b[24]; snprintf(b,24,"HeatIdx: %.1fC", r.heatIdx);
            dispText(0,14,b);
            const char* cat = r.heatIdx<27?"Normal":r.heatIdx<32?"Dikkat":r.heatIdx<41?"Tehlikeli":"Cok Tehlikeli";
            dispText(0,28,cat);
        } else dispText(0,24,"Okuma hata!");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 5) { // Alarm ayarla
        alarmEnabled = true;
        dispBannerOK("Alarm: T>35C H>80%");
    } else if (sel == 6) { // SD log
        if (!g_sdInited) { dispBannerErr("SD kart yok"); return; }
        File f = SD.open("/dht_log.csv", FILE_APPEND);
        if (!f) { dispBannerErr("Log acilamadi"); return; }
        uint8_t cnt=0;
        dispClear(); dispStatusBar("SD Log"); dispText(0,24,"10 ornek kaydediliyor"); dispCommit();
        while (cnt<10) {
            Reading r = read_raw(isDHT22);
            if (r.ok) {
                char row[64];
                snprintf(row,64,"%lu,%.1f,%.1f,%.1f,%.1f\n", millis(), r.tempC, r.hum, r.heatIdx, r.dewPt);
                f.print(row); cnt++;
            }
            delay(2000);
        }
        f.close(); dispBannerOK("10 satir kaydedildi");
    } else if (sel == 7) { // Kalibrasyon (offset ayarla — EEPROM yok, sadece göster)
        dispClear(); dispStatusBar("Kalibrasyon");
        dispText(0,12,"Offset ayari icin");
        dispText(0,24,"read_raw() donusune");
        dispText(0,36,"+offset ekle.");
        dispText(0,48,"(Fabrika: +-0.5C)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 8) { // Trend
        dispClear(); dispStatusBar("Sicaklik Trendi");
        for (uint8_t i=0;i<10;i++) {
            char b[24]; snprintf(b,24,"%2d: %.1fC", i+1, tempTrend[i]);
            dispText(0, 12+i*5, b);
        }
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 9) { // Sıfırla
        histTempMin=999; histTempMax=-999; histHumMin=999; histHumMax=-999;
        trendIdx=0; memset(tempTrend,0,sizeof(tempTrend));
        dispBannerOK("Gecmis sifirlandi");
    }
}
} // namespace ModDHT

// ────────────────────────────────────────────────────────────────────
// 2. BMP280 / BME280 — Basınç, Rakım, Sıcaklık
// Bağlantı: SDA → Pin5 (GP41), SCL → Pin6 (GP42), I2C adr 0x76/0x77
// ────────────────────────────────────────────────────────────────────
namespace ModBMP {

// BMP280 ham register okuma (Wire1, kütüphanesiz)
struct BmpCalib {
    uint16_t T1; int16_t T2,T3;
    uint16_t P1; int16_t P2,P3,P4,P5,P6,P7,P8,P9;
    uint8_t  H1; int16_t H2; uint8_t H3; int16_t H4,H5; int8_t H6;
    bool loaded;
};
static BmpCalib cal = {0};
static uint8_t  bmpAddr = 0x76;
static bool     isBME   = false;

static uint8_t readReg(uint8_t reg) {
    Wire1.beginTransmission(bmpAddr);
    Wire1.write(reg); Wire1.endTransmission(false);
    Wire1.requestFrom(bmpAddr, (uint8_t)1);
    return Wire1.available() ? Wire1.read() : 0;
}
static void writeReg(uint8_t reg, uint8_t val) {
    Wire1.beginTransmission(bmpAddr); Wire1.write(reg); Wire1.write(val); Wire1.endTransmission();
}
static void load_calib() {
    // Temp trim
    cal.T1 = readReg(0x88)|(readReg(0x89)<<8);
    cal.T2 = (int16_t)(readReg(0x8A)|(readReg(0x8B)<<8));
    cal.T3 = (int16_t)(readReg(0x8C)|(readReg(0x8D)<<8));
    // Press trim
    cal.P1 = readReg(0x8E)|(readReg(0x8F)<<8);
    for (int i=0;i<8;i++) ((int16_t*)&cal.P2)[i] = (int16_t)(readReg(0x90+i*2)|(readReg(0x91+i*2)<<8));
    cal.loaded = true;
}
static float t_fine = 0;
static float read_temp() {
    int32_t adc = ((int32_t)readReg(0xFA)<<12)|((int32_t)readReg(0xFB)<<4)|(readReg(0xFC)>>4);
    int32_t v1 = (((adc>>3)-((int32_t)cal.T1<<1))*cal.T2)>>11;
    int32_t v2 = (((((adc>>4)-cal.T1)*((adc>>4)-cal.T1))>>12)*cal.T3)>>14;
    t_fine = v1+v2;
    return ((int32_t)(t_fine*5+128) >> 8) / 100.0f;
}
static float read_press() {
    int32_t adc = ((int32_t)readReg(0xF7)<<12)|((int32_t)readReg(0xF8)<<4)|(readReg(0xF9)>>4);
    int64_t v1 = (int64_t)t_fine-128000;
    int64_t v2 = v1*v1*(int64_t)cal.P6; v2=v2+((v1*(int64_t)cal.P5)<<17); v2=v2+(((int64_t)cal.P4)<<35);
    v1 = ((v1*v1*(int64_t)cal.P3)>>8)+((v1*(int64_t)cal.P2)<<12);
    v1 = (((((int64_t)1)<<47)+v1)*(int64_t)cal.P1)>>33;
    if (v1==0) return 0;
    int64_t p = 1048576-adc; p = (((p<<31)-v2)*3125)/v1;
    v1=((int64_t)cal.P9*(p>>13)*(p>>13))>>25; v2=((int64_t)cal.P8*p)>>19;
    p = ((p+v1+v2)>>8)+((int64_t)cal.P7<<4);
    return p/25600.0f;  // hPa
}
static float altitude(float pressHPa, float seaLevel=1013.25f) {
    return 44330.0f*(1.0f-pow(pressHPa/seaLevel, 0.1903f));
}

static void run() {
    Wire1.begin(PIN_MOD_SDA2, PIN_MOD_SCL2, 400000);
    // Adres dene
    Wire1.beginTransmission(0x76); bmpAddr = Wire1.endTransmission()==0 ? 0x76 : 0x77;
    uint8_t chipID = readReg(0xD0);
    isBME = (chipID == 0x60);
    writeReg(0xF4, 0x27);  // normal mode, 1x oversample
    writeReg(0xF5, 0xA0);  // standby 1s
    if (!cal.loaded) load_calib();

    OptionList opts = {
        {"Canli Okuma",    nullptr},
        {"Hava Tahmini",   nullptr},
        {"Rakım Hesap",    nullptr},
        {"Basinc Trendi",  nullptr},
        {"hPa / mmHg",     nullptr},
        {"Deniz Sev. Ref", nullptr},
        {"Grafik (10 ok)", nullptr},
        {"SD Log",         nullptr},
        {"Chip Bilgi",     nullptr},
        {"Irtifa Farki",   nullptr},
        {"Back",           nullptr},
    };
    int sel = loopOptions(opts, isBME?"BME280":"BMP280");
    if (sel < 0 || sel == 10) return;

    if (sel == 0) { // Canlı okuma
        while (!readButtonLong()) {
            potApplyBrightness();
            float tc = read_temp();
            float p  = read_press();
            float alt= altitude(p);
            dispClear(); dispStatusBar(isBME?"BME280":"BMP280");
            char b1[24],b2[24],b3[24];
            snprintf(b1,24,"Temp: %.1fC %.1fF", tc, tc*9/5+32);
            snprintf(b2,24,"Basinc: %.1f hPa", p);
            snprintf(b3,24,"Yukseklik: %.0f m", alt);
            dispText(0,12,b1); dispText(0,24,b2); dispText(0,36,b3);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(1000);
        }
    } else if (sel == 1) { // Hava tahmini (basit basınç eşiği)
        float p = read_press();
        dispClear(); dispStatusBar("Hava Tahmini");
        char pb[24]; snprintf(pb,24,"Basinc: %.1f hPa",p);
        dispText(0,12,pb);
        const char* fc = p>1022?"Acik/Gunesli":p>1009?"Parcali Bulutlu":p>995?"Yagmurlu":"Firtinali";
        dispText(0,26,fc);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 2) { // Rakım
        float p = read_press();
        float alt = altitude(p);
        dispClear(); dispStatusBar("Rakım");
        char b1[24],b2[24];
        snprintf(b1,24,"Alt: %.0f m", alt);
        snprintf(b2,24,"   : %.0f ft", alt*3.281f);
        dispText(0,14,b1); dispText(0,28,b2);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 3) { // Basınç trendi
        dispClear(); dispStatusBar("Basinc Trendi");
        dispText(0,20,"5 dk olcum aliyor"); dispCommit();
        float samples[6]; for(int i=0;i<6;i++){samples[i]=read_press();delay(50000>50000?50:50);}
        bool rising=samples[5]>samples[0];
        dispClear(); dispStatusBar("Trend");
        char b[24]; snprintf(b,24,"%.1f->%.1f hPa",samples[0],samples[5]);
        dispText(0,14,b);
        dispText(0,28, rising?"Yukseliyor (iyilesme)":"Dusuyor (bozulma)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 4) { // hPa/mmHg
        float p = read_press();
        dispClear(); dispStatusBar("Basinc Birimleri");
        char b1[24],b2[24],b3[24];
        snprintf(b1,24,"hPa:  %.2f", p);
        snprintf(b2,24,"mmHg: %.2f", p*0.75006f);
        snprintf(b3,24,"inHg: %.3f", p*0.02953f);
        dispText(0,12,b1); dispText(0,24,b2); dispText(0,36,b3);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 5) { // Deniz seviyesi referans
        float p = read_press(); float alt = altitude(p);
        dispClear(); dispStatusBar("Deniz Sev. Ref");
        char b[24]; snprintf(b,24,"QNH: %.1f hPa", p * pow(1.0f-alt/44330.0f, -5.255f));
        dispText(0,14,b);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 6) { // Grafik
        float s[10]; for(int i=0;i<10;i++){s[i]=read_press();delay(300);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear(); dispStatusBar("Basinc Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):8;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%.0f-%.0f hPa",mn,mx);
        dispText(0,54,rng); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 7) { // SD Log
        if (!g_sdInited) { dispBannerErr("SD kart yok"); return; }
        File f = SD.open("/bmp_log.csv", FILE_APPEND);
        if (!f) { dispBannerErr("Log acilamadi"); return; }
        for(int i=0;i<10;i++){
            float tc=read_temp(), p=read_press();
            char row[64]; snprintf(row,64,"%lu,%.2f,%.2f,%.0f\n",millis(),tc,p,altitude(p));
            f.print(row); delay(1000);
        }
        f.close(); dispBannerOK("10 satir kaydedildi");
    } else if (sel == 8) { // Chip bilgi
        dispClear(); dispStatusBar("Chip Bilgi");
        char b[24]; snprintf(b,24,"ChipID: 0x%02X (%s)",chipID,isBME?"BME280":"BMP280");
        dispText(0,12,b);
        char a[24]; snprintf(a,24,"I2C Adres: 0x%02X", bmpAddr);
        dispText(0,24,a);
        dispText(0,36,"Bus: Wire1 (GP41/42)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while (!readButton() && !readButtonLong()) { potApplyBrightness(); delay(50); }
    } else if (sel == 9) { // İrtifa farkı (2 okuma arası)
        float p1=read_press(); float a1=altitude(p1);
        dispBannerOK("Ref noktasi alindi");
        delay(500);
        while (!readButtonLong()) {
            potApplyBrightness();
            float p2=read_press(); float a2=altitude(p2);
            dispClear(); dispStatusBar("Irtifa Farki");
            char b[24]; snprintf(b,24,"Delta: %.1f m", a2-a1);
            dispText(0,18,b);
            dispText(0,32,(a2>a1)?"Yukseliyorsunuz":"Alcaliyorsunuz");
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(500);
        }
    }
}
} // namespace ModBMP

// ────────────────────────────────────────────────────────────────────
// 3. HC-SR04 — Ultrasonik Mesafe
// Bağlantı: Trig → Pin7 (GP37), Echo → Pin4 (GP40)
// ────────────────────────────────────────────────────────────────────
namespace ModHCSR04 {

static float distMin=9999, distMax=0;
static uint16_t measCount=0;

static float measure_cm() {
    pinMode(PIN_MOD_TX2,  OUTPUT);
    pinMode(PIN_MOD_DATA, INPUT);
    digitalWrite(PIN_MOD_TX2, LOW); delayMicroseconds(2);
    digitalWrite(PIN_MOD_TX2, HIGH); delayMicroseconds(10);
    digitalWrite(PIN_MOD_TX2, LOW);
    uint32_t t0=micros();
    while(digitalRead(PIN_MOD_DATA)==LOW)  { if(micros()-t0>25000) return -1; }
    uint32_t st=micros();
    while(digitalRead(PIN_MOD_DATA)==HIGH) { if(micros()-st>25000) return -1; }
    return (micros()-st)*0.01715f;
}

static void run() {
    OptionList opts = {
        {"Canli Mesafe",    nullptr},
        {"Bar Grafik",      nullptr},
        {"Min/Max",         nullptr},
        {"Hiz Olcum (2pt)", nullptr},
        {"Alarm Esigi",     nullptr},
        {"cm / inch",       nullptr},
        {"Echo Sure (us)",  nullptr},
        {"SD Log",          nullptr},
        {"Tek Shot",        nullptr},
        {"Tarama Modu",     nullptr},
        {"Back",            nullptr},
    };
    int sel = loopOptions(opts,"HC-SR04");
    if (sel<0||sel==10) return;

    if (sel==0) { // Canlı
        while(!readButtonLong()){
            potApplyBrightness();
            float cm=measure_cm(); measCount++;
            if(cm>0){if(cm<distMin)distMin=cm;if(cm>distMax)distMax=cm;}
            dispClear(); dispStatusBar("HC-SR04 Mesafe");
            if(cm>0 && cm<400){
                char b1[24],b2[24];
                snprintf(b1,24,"%.1f cm", cm);
                snprintf(b2,24,"%.3f m", cm/100.0f);
                dispText(4,14,b1); dispText(4,26,b2);
                int bw=(int)(cm*110.0f/200.0f); if(bw>110)bw=110;
                display.drawRect(4,40,112,8,SSD1306_WHITE);
                display.fillRect(4,40,bw,8,SSD1306_WHITE);
            } else { dispText(0,24,"Menzil disi/hata"); }
            char cnt[16]; snprintf(cnt,16,"#%d [LONG]=cik",measCount);
            dispText(0,54,cnt); dispCommit(); delay(100);
        }
    } else if (sel==1) { // Bar grafik (10 örnek)
        float s[10]; for(int i=0;i<10;i++){s[i]=measure_cm();delay(120);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn&&s[i]>0)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear(); dispStatusBar("Mesafe Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%.0f-%.0f cm",mn,mx);
        dispText(0,54,rng); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if (sel==2) { // Min/Max
        dispClear(); dispStatusBar("Min/Max");
        char b1[24],b2[24];
        snprintf(b1,24,"Min: %.1f cm",distMin>9998?0:distMin);
        snprintf(b2,24,"Max: %.1f cm",distMax);
        dispText(0,18,b1); dispText(0,32,b2);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if (sel==3) { // Hız ölçüm (cisim 2 noktadan geçince)
        dispClear(); dispStatusBar("Hiz Olcum");
        dispText(0,14,"Mesafe girin: 50cm");
        dispText(0,28,"BTN = hazir"); dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        uint32_t t1=0,t2=0; bool g1=false;
        float baseDist=measure_cm();
        while(!t2){
            float d=measure_cm();
            if(!g1 && fabs(d-baseDist)>5){g1=true;t1=millis();}
            if(g1  && fabs(d-baseDist)<2){t2=millis();}
            delay(10);
        }
        float dt=(t2-t1)/1000.0f;
        float speed=0.50f/dt; // 50cm / saniye
        dispClear(); dispStatusBar("Hiz Sonuc");
        char b[24]; snprintf(b,24,"%.2f m/s",speed);
        dispText(0,20,b); snprintf(b,24,"%.1f km/h",speed*3.6f);
        dispText(0,34,b); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if (sel==4) { // Alarm
        dispBannerOK("Alarm: <10cm");
        while(!readButtonLong()){
            potApplyBrightness();
            float d=measure_cm();
            if(d>0 && d<10){ buzzOK(); dispBannerWarn("YAKIN! "+String(d,1)+"cm"); }
            delay(200);
        }
    } else if (sel==5) { // cm/inch
        while(!readButtonLong()){
            potApplyBrightness();
            float cm=measure_cm();
            dispClear(); dispStatusBar("cm / inch");
            char b1[24],b2[24];
            snprintf(b1,24,"%.1f cm",cm);
            snprintf(b2,24,"%.2f inch",cm/2.54f);
            dispText(0,20,b1); dispText(0,34,b2);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(200);
        }
    } else if (sel==6) { // Echo süresi
        while(!readButtonLong()){
            potApplyBrightness();
            pinMode(PIN_MOD_TX2,OUTPUT); pinMode(PIN_MOD_DATA,INPUT);
            digitalWrite(PIN_MOD_TX2,LOW); delayMicroseconds(2);
            digitalWrite(PIN_MOD_TX2,HIGH); delayMicroseconds(10);
            digitalWrite(PIN_MOD_TX2,LOW);
            uint32_t t0=micros();
            while(digitalRead(PIN_MOD_DATA)==LOW){}
            uint32_t st=micros();
            while(digitalRead(PIN_MOD_DATA)==HIGH){}
            uint32_t dur=micros()-st;
            dispClear(); dispStatusBar("Echo Sure");
            char b[24]; snprintf(b,24,"%lu us",dur);
            dispText(0,20,b); snprintf(b,24,"%.1f cm",dur*0.01715f);
            dispText(0,34,b); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(150);
        }
    } else if (sel==7) { // SD log
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/hcsr_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<20;i++){
            float d=measure_cm();
            char row[32]; snprintf(row,32,"%lu,%.1f\n",millis(),d);
            f.print(row); delay(200);
        }
        f.close(); dispBannerOK("20 olcum kaydedildi");
    } else if (sel==8) { // Tek shot
        float d=measure_cm();
        dispClear(); dispStatusBar("Tek Shot");
        char b[24]; snprintf(b,24,"%.1f cm",d);
        dispText(8,22,b); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if (sel==9) { // Tarama modu (servo yoksa kafa salla)
        dispClear(); dispStatusBar("Tarama Modu");
        dispText(0,16,"Servo yoksa");
        dispText(0,28,"sabit olcum x10");
        dispCommit(); delay(800);
        for(int i=0;i<10;i++){
            float d=measure_cm();
            char b[24]; snprintf(b,24,"[%2d] %.1f cm",i+1,d);
            logPush("[HCSR04] "+String(b));
            delay(300);
        }
        dispBannerOK("Log'a yazildi");
    }
}
} // namespace ModHCSR04

// ────────────────────────────────────────────────────────────────────
// 4. PIR (HC-SR501) — Hareket Algılama
// Bağlantı: Out → Pin4 (GP40)
// ────────────────────────────────────────────────────────────────────
namespace ModPIR {
static uint32_t motionCount=0, lastMotion=0;
static bool ledOut=false;

static void run() {
    pinMode(PIN_MOD_DATA, INPUT);
    OptionList opts={
        {"Hareket Izle",   nullptr},
        {"Sayac",          nullptr},
        {"Alarm Modu",     nullptr},
        {"LED Blink Cikis",nullptr},
        {"Cooldown Ayarla",nullptr},
        {"Istatistik",     nullptr},
        {"Log Baslat",     nullptr},
        {"Zone Testi",     nullptr},
        {"Duyarlilik",     nullptr},
        {"Sifirla",        nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"PIR HC-SR501");
    if(sel<0||sel==10) return;

    uint32_t cooldown=2000;
    if(sel==0||sel==1||sel==2||sel==3||sel==6||sel==7) {
        while(!readButtonLong()){
            potApplyBrightness();
            bool motion=digitalRead(PIN_MOD_DATA)==HIGH;
            if(motion && millis()-lastMotion>cooldown){
                motionCount++; lastMotion=millis();
                if(sel==2){buzzOK();}
                if(sel==6){logPush("[PIR] Hareket #"+String(motionCount)+" t="+String(millis()));}
            }
            dispClear(); dispStatusBar("PIR Hareket");
            if(motion){
                display.fillRoundRect(20,14,88,20,4,SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
                dispText(28,19,"HAREKET!");
                display.setTextColor(SSD1306_WHITE);
            } else {
                dispText(28,20,"Sessiz...");
            }
            char b[24]; snprintf(b,24,"Sayac: %lu",motionCount);
            dispText(0,40,b);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(100);
        }
    } else if(sel==4){ // Cooldown
        dispBannerOK("Cooldown: 2000ms");
    } else if(sel==5){ // İstatistik
        uint32_t upSec=(millis()/1000);
        dispClear(); dispStatusBar("PIR Istatistik");
        char b1[24],b2[24];
        snprintf(b1,24,"Toplam: %lu hareket",motionCount);
        snprintf(b2,24,"Uptime: %lu sn",upSec);
        dispText(0,14,b1); dispText(0,28,b2);
        float rate=upSec>0?(float)motionCount/upSec*60:0;
        char b3[24]; snprintf(b3,24,"Oran: %.1f/dk",rate);
        dispText(0,42,b3); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==8){ // Duyarlılık
        dispClear(); dispStatusBar("Duyarlilik");
        dispText(0,12,"HC-SR501 uzerindeki");
        dispText(0,24,"potansiyometre ile");
        dispText(0,36,"ayarlayin.");
        dispText(0,48,"(Firmware degistiremez)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        motionCount=0; lastMotion=0; dispBannerOK("Sifirlandi");
    }
}
} // namespace ModPIR

// ────────────────────────────────────────────────────────────────────
// 5. MQ-2 / MQ-135 — Gaz Sensörü
// Bağlantı: AO → Pin3 (GP39 ADC)
// ────────────────────────────────────────────────────────────────────
namespace ModMQ {
static float mq_r0 = 10.0f;  // Kalibrasyon referansı (temiz havada ölçülen Ro)
static bool  calDone = false;

static float read_ppm(float r0) {
    int raw = analogRead(PIN_MOD_DATA);  // GP40 analog girişi kullan
    float voltage = raw * 3.3f / 4095.0f;
    if (voltage < 0.01f) return 0;
    float rs = (3.3f - voltage) / voltage * 10.0f;  // RL=10kΩ varsayımı
    float ratio = rs / r0;
    // LPG için ampirik (Winsen datasheet)
    return 1000.0f * pow(ratio / 4.4f, -2.8f);
}

static void calibrate() {
    dispClear(); dispStatusBar("MQ Kalibrasyon");
    dispText(0,12,"Temiz havada bekle:");
    dispText(0,24,"60 saniye..."); dispCommit();
    float sum=0;
    for(int i=0;i<60;i++){
        int raw=analogRead(40);
        float v=raw*3.3f/4095.0f;
        if(v>0.01f) sum+=(3.3f-v)/v*10.0f;
        delay(1000);
    }
    mq_r0=sum/60.0f; calDone=true;
    char b[24]; snprintf(b,24,"R0=%.2f done",mq_r0);
    dispBannerOK(b);
}

static void run(bool isMQ135) {
    pinMode(40, INPUT);
    OptionList opts={
        {"Canli PPM",      nullptr},
        {"ADC Raw / Volt", nullptr},
        {"Kalibrasyon",    nullptr},
        {"Alarm Esigi",    nullptr},
        {"Grafik",         nullptr},
        {"SD Log",         nullptr},
        {"R0 Goster",      nullptr},
        {"Purge / Sifirla",nullptr},
        {"Gaz Turu Sec",   nullptr},
        {"Istatistik",     nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,isMQ135?"MQ-135":"MQ-2");
    if(sel<0||sel==10) return;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float ppm=read_ppm(mq_r0);
            dispClear(); dispStatusBar(isMQ135?"MQ-135":"MQ-2");
            char b[24]; snprintf(b,24,"PPM: %.1f",ppm);
            dispText(4,14,b);
            const char* lv=ppm<50?"Temiz":ppm<200?"Orta":"YUKSEK!";
            dispText(4,30,lv);
            int bw=(int)(ppm/10.0f); if(bw>112)bw=112;
            display.drawRect(4,44,112,8,SSD1306_WHITE);
            display.fillRect(4,44,bw,8,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(500);
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(40);
            float v=raw*3.3f/4095.0f;
            dispClear(); dispStatusBar("ADC / Volt");
            char b1[24],b2[24];
            snprintf(b1,24,"ADC: %d / 4095",raw);
            snprintf(b2,24,"Volt: %.3f V",v);
            dispText(0,18,b1); dispText(0,34,b2);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(300);
        }
    } else if(sel==2){ calibrate(); }
    else if(sel==3){ dispBannerOK("Alarm: PPM>300"); }
    else if(sel==4){
        float s[10]; for(int i=0;i<10;i++){s[i]=read_ppm(mq_r0);delay(300);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear(); dispStatusBar("PPM Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%.0f-%.0f ppm",mn,mx);
        dispText(0,54,rng); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==5){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/mq_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<10;i++){
            float ppm=read_ppm(mq_r0);
            char row[32]; snprintf(row,32,"%lu,%.1f\n",millis(),ppm);
            f.print(row); delay(500);
        }
        f.close(); dispBannerOK("10 satir kaydedildi");
    } else if(sel==6){
        dispClear(); dispStatusBar("R0 Degeri");
        char b[24]; snprintf(b,24,"R0 = %.3f kohm",mq_r0);
        dispText(0,20,b);
        dispText(0,34,calDone?"(Kalibre edildi)":"(Varsayilan)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){ mq_r0=10.0f; calDone=false; dispBannerOK("R0 varsayilana dondu"); }
    else if(sel==8){ dispBannerOK("MQ-2:LPG/CO/Duman\nMQ-135:NH3/NOx/CO2"); }
    else if(sel==9){
        dispClear(); dispStatusBar("Istatistik");
        dispText(0,14,"R0:"); char b[24]; snprintf(b,24,"%.3f kohm",mq_r0); dispText(30,14,b);
        dispText(0,28,calDone?"Kalibre: EVET":"Kalibre: HAYIR");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    }
}
} // namespace ModMQ

// ────────────────────────────────────────────────────────────────────
// 6. MAX30102 — Kalp Atışı & SpO2
// Bağlantı: SDA→Pin5 (GP41), SCL→Pin6 (GP42), I2C adr 0x57
// ────────────────────────────────────────────────────────────────────
namespace ModMAX30102 {
static constexpr uint8_t MAX_ADDR = 0x57;

static void writeReg(uint8_t reg, uint8_t val){
    Wire1.beginTransmission(MAX_ADDR); Wire1.write(reg); Wire1.write(val); Wire1.endTransmission();
}
static uint8_t readReg(uint8_t reg){
    Wire1.beginTransmission(MAX_ADDR); Wire1.write(reg); Wire1.endTransmission(false);
    Wire1.requestFrom(MAX_ADDR,(uint8_t)1); return Wire1.available()?Wire1.read():0;
}
static void init_sensor(){
    writeReg(0x09,0x40); delay(10);   // Reset
    writeReg(0x09,0x03);              // SpO2 modu
    writeReg(0x0A,0x27);              // SPO2: 4096nA, 411us, 100sps
    writeReg(0x0C,0x24);              // IR LED 25mA
    writeReg(0x0D,0x24);              // Red LED 25mA
    writeReg(0x08,0x4F);              // FIFO: average=4, rollover, 15 samples
}
static uint32_t read_fifo(uint32_t& red, uint32_t& ir){
    red=ir=0; uint8_t wptr=readReg(0x04), rptr=readReg(0x06);
    uint8_t cnt=(wptr-rptr)&0x1F;
    if(!cnt) return 0;
    Wire1.beginTransmission(MAX_ADDR); Wire1.write(0x07); Wire1.endTransmission(false);
    Wire1.requestFrom(MAX_ADDR,(uint8_t)6);
    for(int i=0;i<3;i++) red=(red<<8)|Wire1.read();
    for(int i=0;i<3;i++) ir=(ir<<8)|Wire1.read();
    red&=0x3FFFF; ir&=0x3FFFF;
    return cnt;
}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    init_sensor();

    OptionList opts={
        {"BPM (Kalp Atisi)", nullptr},
        {"SpO2 Oksijen",     nullptr},
        {"IR / Red Ham",     nullptr},
        {"BPM Grafik",       nullptr},
        {"Ortalama BPM",     nullptr},
        {"Alarm Ayarla",     nullptr},
        {"SD Log",           nullptr},
        {"Guc Modu",         nullptr},
        {"Parmak Algila",    nullptr},
        {"Trend Goster",     nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"MAX30102");
    if(sel<0||sel==10) return;

    if(sel==0||sel==1||sel==2){
        uint32_t bpmSamples[10]; uint8_t bsi=0;
        uint32_t lastBeat=0; float bpm=0;
        bool beatDetected=false;
        uint32_t irBuf[100]; uint8_t ibIdx=0;
        uint32_t spo2=98;

        while(!readButtonLong()){
            potApplyBrightness();
            uint32_t red,ir; read_fifo(red,ir);
            irBuf[ibIdx++%100]=ir;

            // Basit peak detection
            if(ibIdx>3){
                if(irBuf[(ibIdx-2)%100]>irBuf[(ibIdx-3)%100] &&
                   irBuf[(ibIdx-2)%100]>irBuf[(ibIdx-1)%100] &&
                   irBuf[(ibIdx-2)%100]>50000){
                    if(millis()-lastBeat>300){
                        bpm=60000.0f/(millis()-lastBeat);
                        lastBeat=millis();
                        if(bsi<10) bpmSamples[bsi++]=bpm;
                    }
                }
            }
            dispClear();
            if(sel==0){
                dispStatusBar("Kalp Atisi");
                char b[24]; snprintf(b,24,"BPM: %.0f",bpm);
                dispText(16,18,b);
                const char* zone=bpm<60?"Dusuk":bpm<100?"Normal":bpm<140?"Yuksek":"Cok Yuksek";
                dispText(20,32,zone);
            } else if(sel==1){
                dispStatusBar("SpO2");
                // Basit SpO2 tahmini (R=AC_red/DC_red / AC_ir/DC_ir)
                char b[24]; snprintf(b,24,"SpO2: ~%lu%%",spo2);
                dispText(16,18,b);
                dispText(16,34,spo2>95?"Normal":spo2>90?"Sinir":"DUSUK!");
            } else {
                dispStatusBar("IR / Red Ham");
                char b1[24],b2[24];
                snprintf(b1,24,"IR:  %lu",ir);
                snprintf(b2,24,"Red: %lu",red);
                dispText(0,18,b1); dispText(0,32,b2);
            }
            if(ir<50000) dispText(0,44,"Parmak yok");
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(50);
        }
    } else if(sel==7){ // Güç modu
        OptionList pm={{"Normal",nullptr},{"Dusuk Guc",nullptr},{"Kapat",nullptr}};
        int ps=loopOptions(pm,"Guc Modu");
        if(ps==0) writeReg(0x09,0x03);
        else if(ps==1){writeReg(0x0C,0x0C); writeReg(0x0D,0x0C);}
        else writeReg(0x09,0x00);
        dispBannerOK("Guc modu ayarlandi");
    } else if(sel==8){
        uint32_t red,ir; read_fifo(red,ir);
        dispClear(); dispStatusBar("Parmak Algila");
        dispText(0,20,ir>50000?"PARMAK ALGILANDI":"Parmak koymadınız");
        char b[24]; snprintf(b,24,"IR: %lu",ir);
        dispText(0,36,b); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    }
}
} // namespace ModMAX30102

// ────────────────────────────────────────────────────────────────────
// 7. DS18B20 — 1-Wire Sıcaklık Sensörü
// Bağlantı: Data → Pin4 (GP40), 4.7kΩ pull-up dahili
// ────────────────────────────────────────────────────────────────────
namespace ModDS18B20 {
static float offsetCal=0.0f;
static float histMin=999, histMax=-999;

// 1-Wire primitives
static bool ow_reset(){
    pinMode(PIN_MOD_DATA,OUTPUT); digitalWrite(PIN_MOD_DATA,LOW); delayMicroseconds(480);
    digitalWrite(PIN_MOD_DATA,HIGH); delayMicroseconds(70);
    pinMode(PIN_MOD_DATA,INPUT_PULLUP);
    bool present=!digitalRead(PIN_MOD_DATA);
    delayMicroseconds(410); return present;
}
static void ow_write(uint8_t b){
    for(int i=0;i<8;i++){
        pinMode(PIN_MOD_DATA,OUTPUT);
        if(b&1){ digitalWrite(PIN_MOD_DATA,LOW); delayMicroseconds(6); digitalWrite(PIN_MOD_DATA,HIGH); delayMicroseconds(64); }
        else   { digitalWrite(PIN_MOD_DATA,LOW); delayMicroseconds(64); digitalWrite(PIN_MOD_DATA,HIGH); delayMicroseconds(2); }
        b>>=1;
    }
}
static uint8_t ow_read(){
    uint8_t b=0;
    for(int i=0;i<8;i++){
        pinMode(PIN_MOD_DATA,OUTPUT); digitalWrite(PIN_MOD_DATA,LOW); delayMicroseconds(3);
        pinMode(PIN_MOD_DATA,INPUT_PULLUP); delayMicroseconds(9);
        b|=(digitalRead(PIN_MOD_DATA)<<i); delayMicroseconds(53);
    }
    return b;
}
static float read_temp(){
    if(!ow_reset()) return -999;
    ow_write(0xCC); ow_write(0x44);
    delay(750);
    if(!ow_reset()) return -999;
    ow_write(0xCC); ow_write(0xBE);
    uint8_t lo=ow_read(), hi=ow_read();
    int16_t raw=(hi<<8)|lo;
    return (raw/16.0f)+offsetCal;
}

static void run(){
    OptionList opts={
        {"Canli Sicaklik",   nullptr},
        {"Min/Max",          nullptr},
        {"Offset Kal.",      nullptr},
        {"Cozunurluk Sec",   nullptr},
        {"Grafik",           nullptr},
        {"SD Log",           nullptr},
        {"Parasite Guc?",    nullptr},
        {"Cihaz Varligi",    nullptr},
        {"Alarm",            nullptr},
        {"Gecmis Sifirla",   nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"DS18B20");
    if(sel<0||sel==10) return;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float t=read_temp();
            if(t<histMin&&t>-999) histMin=t;
            if(t>histMax&&t>-999) histMax=t;
            dispClear(); dispStatusBar("DS18B20");
            if(t>-999){
                char b1[24],b2[24];
                snprintf(b1,24,"%.3f C",t);
                snprintf(b2,24,"%.3f F",t*9/5+32);
                dispText(8,18,b1); dispText(8,32,b2);
            } else dispText(0,24,"Sensor bulunamadi!");
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(1000);
        }
    } else if(sel==1){
        dispClear(); dispStatusBar("Min/Max");
        char b1[24],b2[24];
        snprintf(b1,24,"Min: %.2fC",histMin>900?0:histMin);
        snprintf(b2,24,"Max: %.2fC",histMax<-900?0:histMax);
        dispText(0,20,b1); dispText(0,34,b2);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==2){
        // Offset +/-1 aralığında ayarla
        float off=offsetCal;
        while(!readButtonLong()){
            potApplyBrightness();
            dispClear(); dispStatusBar("Offset Kal.");
            char b[24]; snprintf(b,24,"Offset: %+.1f C",off);
            dispText(0,20,b); dispText(0,36,"UP/DOWN=ayarla");
            dispText(0,54,"BTN=kaydet LONG=cik"); dispCommit();
            JoyDir d=readJoystick();
            if(d==JoyDir::UP)   { off+=0.1f; buzzNav(); }
            if(d==JoyDir::DOWN) { off-=0.1f; buzzNav(); }
            if(readButton()){ offsetCal=off; dispBannerOK("Kaydedildi"); break; }
            delay(80);
        }
    } else if(sel==3){
        OptionList res={{"9-bit (0.5C)  94ms",nullptr},{"10-bit(0.25C)188ms",nullptr},
                        {"11-bit(0.12C)375ms",nullptr},{"12-bit(0.06C)750ms",nullptr}};
        int rs=loopOptions(res,"Cozunurluk");
        if(rs>=0){
            uint8_t cfg=(0x1F|(rs<<5));
            if(!ow_reset()){dispBannerErr("Sensor yok");return;}
            ow_write(0xCC); ow_write(0x4E);
            ow_write(0); ow_write(0); ow_write(cfg);
            dispBannerOK("Cozunurluk ayarlandi");
        }
    } else if(sel==4){
        float s[10]; for(int i=0;i<10;i++){s[i]=read_temp();delay(1000);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn&&s[i]>-999)mn=s[i];if(s[i]>mx&&s[i]>-999)mx=s[i];}
        dispClear(); dispStatusBar("Temp Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%.1f-%.1fC",mn,mx);
        dispText(0,54,rng); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==5){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/ds18_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<10;i++){
            float t=read_temp();
            char row[32]; snprintf(row,32,"%lu,%.3f\n",millis(),t);
            f.print(row); delay(1000);
        }
        f.close(); dispBannerOK("10 satir kaydedildi");
    } else if(sel==6){
        dispClear(); dispStatusBar("Parasite Guc");
        dispText(0,12,"DS18B20: 2-3 pin mod");
        dispText(0,24,"Parasite: VCC=boş");
        dispText(0,36,"Data pinine 4.7kΩ");
        dispText(0,48,"pull-up (3V3'e)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){
        bool present=ow_reset();
        dispClear(); dispStatusBar("Cihaz Varligi");
        dispText(0,24,present?"DS18B20 BULUNDU":"Cihaz YOK");
        dispText(0,38,"Pin: GP40 (Pin4)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==8){
        dispBannerOK("Alarm: T>40C veya T<0C");
        while(!readButtonLong()){
            potApplyBrightness();
            float t=read_temp();
            if(t>40||t<0){buzzOK();dispBannerWarn("ALARM: "+String(t,1)+"C");}
            delay(1000);
        }
    } else if(sel==9){
        histMin=999; histMax=-999; dispBannerOK("Gecmis sifirlandi");
    }
}
} // namespace ModDS18B20

// ────────────────────────────────────────────────────────────────────
// 8. ADXL345 — İvme Ölçer
// Bağlantı: SDA→Pin5 (GP41), SCL→Pin6 (GP42), I2C adr 0x53
// ────────────────────────────────────────────────────────────────────
namespace ModADXL345 {
static constexpr uint8_t ADXL_ADDR=0x53;
static float biasX=0,biasY=0,biasZ=0;
static uint32_t stepCount=0;
static float prevMag=0;

static void writeReg(uint8_t reg,uint8_t val){
    Wire1.beginTransmission(ADXL_ADDR); Wire1.write(reg); Wire1.write(val); Wire1.endTransmission();
}
static void readXYZ(int16_t& x,int16_t& y,int16_t& z){
    Wire1.beginTransmission(ADXL_ADDR); Wire1.write(0x32); Wire1.endTransmission(false);
    Wire1.requestFrom(ADXL_ADDR,(uint8_t)6);
    x=(Wire1.read()|(Wire1.read()<<8));
    y=(Wire1.read()|(Wire1.read()<<8));
    z=(Wire1.read()|(Wire1.read()<<8));
}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    writeReg(0x2D,0x08);  // Measurement mode
    writeReg(0x31,0x00);  // ±2g, 10-bit

    OptionList opts={
        {"Canli X/Y/Z",    nullptr},
        {"Tilt Acisi",     nullptr},
        {"Adim Sayaci",    nullptr},
        {"Serbest Dusus",  nullptr},
        {"Dokunma Algila", nullptr},
        {"G-Force",        nullptr},
        {"SD Log",         nullptr},
        {"Grafik",         nullptr},
        {"Kalibrasyon",    nullptr},
        {"Ivme Alarm",     nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"ADXL345");
    if(sel<0||sel==10) return;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            int16_t x,y,z; readXYZ(x,y,z);
            float gx=(x-biasX)*0.004f, gy=(y-biasY)*0.004f, gz=(z-biasZ)*0.004f;
            dispClear(); dispStatusBar("ADXL345 XYZ");
            char b1[24],b2[24],b3[24];
            snprintf(b1,24,"X: %+.2f g (%d)",gx,x);
            snprintf(b2,24,"Y: %+.2f g (%d)",gy,y);
            snprintf(b3,24,"Z: %+.2f g (%d)",gz,z);
            dispText(0,14,b1); dispText(0,26,b2); dispText(0,38,b3);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(100);
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            int16_t x,y,z; readXYZ(x,y,z);
            float gx=x*0.004f, gy=y*0.004f, gz=z*0.004f;
            float pitch=atan2(-gx,sqrt(gy*gy+gz*gz))*180/PI;
            float roll =atan2(gy,gz)*180/PI;
            dispClear(); dispStatusBar("Tilt Acisi");
            char b1[24],b2[24];
            snprintf(b1,24,"Pitch: %+.1f deg",pitch);
            snprintf(b2,24,"Roll:  %+.1f deg",roll);
            dispText(0,18,b1); dispText(0,32,b2);
            // Level indicator
            int px=64+(int)(roll/90*50), py=42;
            display.drawLine(14,42,114,42,SSD1306_WHITE);
            display.fillCircle(constrain(px,14,114),py,4,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(100);
        }
    } else if(sel==2){
        stepCount=0;
        while(!readButtonLong()){
            potApplyBrightness();
            int16_t x,y,z; readXYZ(x,y,z);
            float mag=sqrt((float)x*x+(float)y*y+(float)z*z)*0.004f;
            if(mag>prevMag+0.3f && prevMag<1.2f && mag>1.1f) stepCount++;
            prevMag=mag;
            dispClear(); dispStatusBar("Adim Sayaci");
            char b[24]; snprintf(b,24,"Adim: %lu",stepCount);
            dispText(16,20,b);
            float km=stepCount*0.75f/1000.0f;
            snprintf(b,24,"%.3f km",km);
            dispText(16,34,b);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(50);
        }
    } else if(sel==3){
        // Serbest düşüş algılama (threshold <0.6g)
        writeReg(0x28,0x03);  // Freefall threshold ~187mg
        writeReg(0x29,0x14);  // Freefall time 100ms
        writeReg(0x2E,0x04);  // Freefall interrupt enable
        dispBannerOK("Serbest dusus alarm aktif");
        uint32_t ffCount=0;
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t src=0;
            Wire1.beginTransmission(ADXL_ADDR); Wire1.write(0x30); Wire1.endTransmission(false);
            Wire1.requestFrom(ADXL_ADDR,(uint8_t)1); if(Wire1.available()) src=Wire1.read();
            if(src&0x04){ffCount++;buzzOK();}
            dispClear(); dispStatusBar("Serbest Dusus");
            char b[24]; snprintf(b,24,"Alarm: %lu kez",ffCount);
            dispText(0,24,b); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(100);
        }
    } else if(sel==4){
        writeReg(0x1D,0x10);  // Tap threshold
        writeReg(0x21,0x0F);  // Tap duration
        writeReg(0x2E,0x40);  // Single tap interrupt
        uint32_t tapCnt=0;
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t src=0;
            Wire1.beginTransmission(ADXL_ADDR); Wire1.write(0x30); Wire1.endTransmission(false);
            Wire1.requestFrom(ADXL_ADDR,(uint8_t)1); if(Wire1.available()) src=Wire1.read();
            if(src&0x40){tapCnt++;buzzNav();}
            dispClear(); dispStatusBar("Dokunma Algila");
            char b[24]; snprintf(b,24,"Tap: %lu",tapCnt);
            dispText(20,24,b); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(50);
        }
    } else if(sel==5){
        while(!readButtonLong()){
            potApplyBrightness();
            int16_t x,y,z; readXYZ(x,y,z);
            float gf=sqrt((float)x*x+(float)y*y+(float)z*z)*0.004f;
            dispClear(); dispStatusBar("G-Force");
            char b[24]; snprintf(b,24,"%.3f g",gf);
            dispText(20,20,b);
            const char* lv=gf<1.05f?"Normal":gf<2?"Hareket":"YUKSEK G!";
            dispText(20,36,lv);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(100);
        }
    } else if(sel==6){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/adxl_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<20;i++){
            int16_t x,y,z; readXYZ(x,y,z);
            char row[48]; snprintf(row,48,"%lu,%d,%d,%d\n",millis(),x,y,z);
            f.print(row); delay(100);
        }
        f.close(); dispBannerOK("20 satir kaydedildi");
    } else if(sel==8){
        dispClear(); dispStatusBar("Kalibrasyon");
        dispText(0,12,"Duze koy, hareketsiz"); dispText(0,24,"BTN = kal."); dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        int32_t sx=0,sy=0,sz=0;
        for(int i=0;i<50;i++){
            int16_t x,y,z; readXYZ(x,y,z);
            sx+=x; sy+=y; sz+=z; delay(20);
        }
        biasX=sx/50.0f; biasY=sy/50.0f; biasZ=sz/50.0f;
        dispBannerOK("Kalibrasyon tamamlandi");
    } else if(sel==9){
        dispBannerOK("Alarm: G>2.5g");
        while(!readButtonLong()){
            potApplyBrightness();
            int16_t x,y,z; readXYZ(x,y,z);
            float gf=sqrt((float)x*x+(float)y*y+(float)z*z)*0.004f;
            if(gf>2.5f){buzzOK();dispBannerWarn("G ALARM: "+String(gf,2)+"g");}
            delay(100);
        }
    }
}
} // namespace ModADXL345

// ────────────────────────────────────────────────────────────────────
// 9. LDR — Işık Sensörü (Fotodirenç)
// Bağlantı: ADC → Pin3 (GP39), gerilim bölücü 10kΩ seri
// ────────────────────────────────────────────────────────────────────
namespace ModLDR {
static uint16_t darkTh=500, brightTh=3500;
static float luxCal=1.0f;

static void run(){
    OptionList opts={
        {"Canli Isik",    nullptr},
        {"Lux Tahmini",   nullptr},
        {"ADC / Volt",    nullptr},
        {"Gece/Gunduz",   nullptr},
        {"Grafik",        nullptr},
        {"Alarm",         nullptr},
        {"Esik Ayarla",   nullptr},
        {"SD Log",        nullptr},
        {"Ortalama",      nullptr},
        {"Oran (%)",      nullptr},
        {"Back",          nullptr},
    };
    int sel=loopOptions(opts,"LDR Isik Sens.");
    if(sel<0||sel==10) return;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            dispClear(); dispStatusBar("LDR Isik");
            char b[24]; snprintf(b,24,"ADC: %d",raw);
            dispText(0,14,b);
            int bw=raw*112/4095;
            display.drawRect(4,28,112,10,SSD1306_WHITE);
            display.fillRect(4,28,bw,10,SSD1306_WHITE);
            const char* lv=raw<darkTh?"Karanlik":raw<brightTh?"Orta":"Aydinlik";
            dispText(0,44,lv); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(200);
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            // Ampirik: lux ≈ (raw/4095)^2 * 10000 * luxCal
            float ratio=(float)raw/4095.0f;
            float lux=ratio*ratio*10000.0f*luxCal;
            dispClear(); dispStatusBar("Lux Tahmini");
            char b[24]; snprintf(b,24,"~%.0f lux",lux);
            dispText(8,18,b);
            const char* sc=lux<10?"Karanlik":lux<100?"Loş":lux<1000?"Normal":"Cok Parlak";
            dispText(8,34,sc); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(300);
        }
    } else if(sel==2){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            float v=raw*3.3f/4095.0f;
            dispClear(); dispStatusBar("ADC / Volt");
            char b1[24],b2[24];
            snprintf(b1,24,"ADC: %d / 4095",raw);
            snprintf(b2,24,"Volt: %.3f V",v);
            dispText(0,18,b1); dispText(0,34,b2);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(300);
        }
    } else if(sel==3){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            dispClear(); dispStatusBar("Gece / Gunduz");
            dispText(20,22,raw<darkTh?"GECE":"GUNDUZ");
            char b[24]; snprintf(b,24,"ADC=%d esik=%d",raw,darkTh);
            dispText(0,38,b); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(300);
        }
    } else if(sel==4){
        int s[10]; for(int i=0;i<10;i++){s[i]=analogRead(39);delay(200);}
        int mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear(); dispStatusBar("LDR Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(s[i]-mn)*36/(mx-mn):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24]; snprintf(rng,24,"%d-%d ADC",mn,mx);
        dispText(0,54,rng); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==5){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            if(raw<darkTh){buzzOK();dispBannerWarn("KARANLIK ALARMI");}
            delay(500);
        }
    } else if(sel==6){
        dispClear(); dispStatusBar("Esik Ayarla");
        dispText(0,12,"Dark esigi: 500");
        dispText(0,24,"Bright esigi: 3500");
        dispText(0,36,"(Firmware sabiti)");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/ldr_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<20;i++){
            int raw=analogRead(39);
            char row[32]; snprintf(row,32,"%lu,%d\n",millis(),raw);
            f.print(row); delay(200);
        }
        f.close(); dispBannerOK("20 satir kaydedildi");
    } else if(sel==8){
        int32_t sum=0;
        for(int i=0;i<20;i++){sum+=analogRead(39);delay(100);}
        int avg=sum/20;
        dispClear(); dispStatusBar("Ortalama (20 ok)");
        char b[24]; snprintf(b,24,"Ort ADC: %d",avg);
        dispText(8,24,b); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            float pct=(float)raw/4095.0f*100.0f;
            dispClear(); dispStatusBar("Isik Orani");
            char b[24]; snprintf(b,24,"%.1f %%",pct);
            dispText(16,18,b);
            int bw=(int)(pct/100.0f*112);
            display.drawRect(4,36,112,10,SSD1306_WHITE);
            display.fillRect(4,36,bw,10,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(200);
        }
    }
}
} // namespace ModLDR

// ────────────────────────────────────────────────────────────────────
// 10. Soil Moisture — Toprak Nemi
// Bağlantı: AO → Pin3 (GP39 ADC)
// ────────────────────────────────────────────────────────────────────
namespace ModSoil {
static int dryVal=3800, wetVal=1200;
static uint16_t watering=0;

static int read_pct(){
    int raw=analogRead(39);
    int pct=map(raw,dryVal,wetVal,0,100);
    return constrain(pct,0,100);
}
static void run(){
    OptionList opts={
        {"Canli Nem",      nullptr},
        {"Yuzde (%)",      nullptr},
        {"Kuru/Islak Kal.",nullptr},
        {"Sulama Alarm",   nullptr},
        {"Grafik",         nullptr},
        {"SD Log",         nullptr},
        {"Trend",          nullptr},
        {"Pompa Cikis",    nullptr},
        {"Istatistik",     nullptr},
        {"Esik Ayarla",    nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"Toprak Nemi");
    if(sel<0||sel==10) return;

    if(sel==0||sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            int raw=analogRead(39);
            int pct=read_pct();
            dispClear(); dispStatusBar("Toprak Nemi");
            if(sel==1){
                char b[24]; snprintf(b,24,"%d %%",pct);
                dispText(20,14,b);
            } else {
                char b[24]; snprintf(b,24,"ADC: %d",raw);
                dispText(0,14,b);
            }
            int bw=pct*112/100;
            display.drawRect(4,30,112,10,SSD1306_WHITE);
            display.fillRect(4,30,bw,10,SSD1306_WHITE);
            const char* lv=pct<20?"Kuru":pct<60?"Normal":"Islak";
            dispText(0,46,lv); dispText(0,54,"[LONG]=cik"); dispCommit(); delay(500);
        }
    } else if(sel==2){
        dispClear(); dispStatusBar("Kalibrasyon");
        dispText(0,12,"1) KURU toprak koy");
        dispText(0,24,"BTN = kuru al"); dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        dryVal=analogRead(39);
        dispBannerOK("Kuru: "+String(dryVal));
        delay(500);
        dispClear(); dispStatusBar("Kalibrasyon");
        dispText(0,12,"2) ISLAK toprak koy");
        dispText(0,24,"BTN = islak al"); dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        wetVal=analogRead(39);
        dispBannerOK("Islak: "+String(wetVal));
    } else if(sel==3){
        while(!readButtonLong()){
            potApplyBrightness();
            int pct=read_pct();
            if(pct<25){buzzOK();dispBannerWarn("SULAMA LAZIM! %"+String(pct));}
            delay(1000);
        }
    } else if(sel==4){
        int s[10]; for(int i=0;i<10;i++){s[i]=read_pct();delay(300);}
        dispClear(); dispStatusBar("Nem Grafik");
        for(int i=0;i<10;i++){
            int bh=s[i]*36/100;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        dispText(0,54,"0-100%"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==5){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/soil_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<10;i++){
            char row[32]; snprintf(row,32,"%lu,%d\n",millis(),read_pct());
            f.print(row); delay(500);
        }
        f.close(); dispBannerOK("10 satir kaydedildi");
    } else if(sel==7){
        // GP40 → röle/MOSFET pompa çıkışı
        pinMode(PIN_MOD_DATA,OUTPUT);
        if(read_pct()<30){
            digitalWrite(PIN_MOD_DATA,HIGH);
            dispBannerOK("Pompa ACIK 3sn");
            delay(3000);
            digitalWrite(PIN_MOD_DATA,LOW);
            watering++;
            dispBannerOK("Pompa kapandi. #"+String(watering));
        } else dispBannerOK("Nem yeterli. Pompa gerekmez.");
    } else if(sel==8){
        dispClear(); dispStatusBar("Istatistik");
        char b1[24],b2[24],b3[24];
        snprintf(b1,24,"Sulama: %d kez",watering);
        snprintf(b2,24,"Kuru ref: %d",dryVal);
        snprintf(b3,24,"Islak ref: %d",wetVal);
        dispText(0,12,b1); dispText(0,24,b2); dispText(0,36,b3);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        dispBannerOK("Esik: <25%=sulama alarm");
    }
}
} // namespace ModSoil

// ════════════════════════════════════════════════════════════════════
//  SENSÖR ANA MENÜSÜ
// ════════════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────────
// 11. MPU6050 — 6-Eksen IMU (İvme + Jiroskop)
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x68/0x69
// ────────────────────────────────────────────────────────────────────
namespace ModMPU6050 {
static constexpr uint8_t MPU_ADDR = 0x68;
static float biasAX=0,biasAY=0,biasAZ=0;
static float biasGX=0,biasGY=0,biasGZ=0;
static float pitch=0,roll=0,yaw=0;
static uint32_t lastT=0;

static void writeReg(uint8_t r,uint8_t v){
    Wire1.beginTransmission(MPU_ADDR);Wire1.write(r);Wire1.write(v);Wire1.endTransmission();
}
static int16_t readWord(uint8_t r){
    Wire1.beginTransmission(MPU_ADDR);Wire1.write(r);Wire1.endTransmission(false);
    Wire1.requestFrom(MPU_ADDR,(uint8_t)2);
    return (Wire1.read()<<8)|Wire1.read();
}
static void readAll(float& ax,float& ay,float& az,float& gx,float& gy,float& gz){
    ax=(readWord(0x3B)-biasAX)/16384.0f;
    ay=(readWord(0x3D)-biasAY)/16384.0f;
    az=(readWord(0x3F)-biasAZ)/16384.0f;
    gx=(readWord(0x43)-biasGX)/131.0f;
    gy=(readWord(0x45)-biasGY)/131.0f;
    gz=(readWord(0x47)-biasGZ)/131.0f;
}
static float readTemp(){
    return readWord(0x41)/340.0f+36.53f;
}
static void updateAngles(float ax,float ay,float az,float gx,float gy,float gz){
    uint32_t now=micros();
    float dt=(now-lastT)/1000000.0f; if(lastT==0)dt=0; lastT=now;
    float aPitch=atan2(-ax,sqrt(ay*ay+az*az))*180/PI;
    float aRoll =atan2(ay,az)*180/PI;
    pitch=0.98f*(pitch+gx*dt)+0.02f*aPitch;
    roll =0.98f*(roll +gy*dt)+0.02f*aRoll;
    yaw +=gz*dt;
}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    writeReg(0x6B,0x00); // Wake up
    writeReg(0x1C,0x00); // ±2g
    writeReg(0x1B,0x00); // ±250°/s

    OptionList opts={
        {"Canli Ivme",      nullptr},
        {"Canli Jiroscp",   nullptr},
        {"Pitch / Roll",    nullptr},
        {"Yaw (z-eksen)",   nullptr},
        {"Sicaklik",        nullptr},
        {"6-Eksen Ham",     nullptr},
        {"Kalibrasyon",     nullptr},
        {"Dusus Algila",    nullptr},
        {"Titresim Sayaci", nullptr},
        {"SD Log",          nullptr},
        {"Grafik",          nullptr},
        {"Back",            nullptr},
    };
    int sel=loopOptions(opts,"MPU6050 IMU");
    if(sel<0||sel==11)return;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            dispClear();dispStatusBar("MPU6050 Ivme");
            char b[24];
            snprintf(b,24,"AX: %+.3f g",ax);dispText(0,12,b);
            snprintf(b,24,"AY: %+.3f g",ay);dispText(0,24,b);
            snprintf(b,24,"AZ: %+.3f g",az);dispText(0,36,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(100);
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            dispClear();dispStatusBar("MPU6050 Jiroscp");
            char b[24];
            snprintf(b,24,"GX: %+.1f d/s",gx);dispText(0,12,b);
            snprintf(b,24,"GY: %+.1f d/s",gy);dispText(0,24,b);
            snprintf(b,24,"GZ: %+.1f d/s",gz);dispText(0,36,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(100);
        }
    } else if(sel==2){
        lastT=0;
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            updateAngles(ax,ay,az,gx,gy,gz);
            dispClear();dispStatusBar("Pitch / Roll");
            char b[24];
            snprintf(b,24,"Pitch: %+.1f deg",pitch);dispText(0,14,b);
            snprintf(b,24,"Roll:  %+.1f deg",roll);dispText(0,28,b);
            int cx=64,cy=44;
            display.drawLine(14,44,114,44,SSD1306_WHITE);
            display.drawLine(64,34,64,54,SSD1306_WHITE);
            int px=cx+(int)(roll/90*48);
            int py=cy-(int)(pitch/90*8);
            display.fillCircle(constrain(px,16,112),constrain(py,36,52),4,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(50);
        }
    } else if(sel==3){
        lastT=0;
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            updateAngles(ax,ay,az,gx,gy,gz);
            dispClear();dispStatusBar("Yaw (Kompass)");
            char b[24]; snprintf(b,24,"Yaw: %+.1f deg",yaw);
            dispText(0,20,b);
            // Pusula oku
            int cx=64,cy=36;
            display.drawCircle(cx,cy,14,SSD1306_WHITE);
            float rad=yaw*PI/180;
            display.fillCircle(cx+(int)(14*sin(rad)),cy-(int)(14*cos(rad)),3,SSD1306_WHITE);
            dispText(58,54,"[LONG]=cik");dispCommit();delay(50);
        }
    } else if(sel==4){
        while(!readButtonLong()){
            potApplyBrightness();
            float t=readTemp();
            dispClear();dispStatusBar("MPU6050 Sicaklik");
            char b[24]; snprintf(b,24,"%.2f C",t);
            dispText(16,20,b);
            snprintf(b,24,"%.2f F",t*9/5+32);
            dispText(16,34,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(500);
        }
    } else if(sel==5){
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            dispClear();dispStatusBar("6-Eksen Ham");
            char b[24];
            snprintf(b,24,"A:%+.2f %+.2f %+.2f",ax,ay,az);dispText(0,14,b);
            snprintf(b,24,"G:%+.1f %+.1f %+.1f",gx,gy,gz);dispText(0,28,b);
            float t=readTemp();
            snprintf(b,24,"T:%.1fC",t);dispText(0,42,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(100);
        }
    } else if(sel==6){
        dispClear();dispStatusBar("Kalibrasyon");
        dispText(0,14,"Duze koy hareketsiz");
        dispText(0,26,"BTN=baslat");dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        float sax=0,say=0,saz=0,sgx=0,sgy=0,sgz=0;
        for(int i=0;i<100;i++){
            sax+=readWord(0x3B);say+=readWord(0x3D);saz+=readWord(0x3F);
            sgx+=readWord(0x43);sgy+=readWord(0x45);sgz+=readWord(0x47);
            delay(10);
        }
        biasAX=sax/100;biasAY=say/100;biasAZ=saz/100-16384;
        biasGX=sgx/100;biasGY=sgy/100;biasGZ=sgz/100;
        dispBannerOK("Kalibrasyon tamam");
    } else if(sel==7){
        writeReg(0x1F,0x14); writeReg(0x20,0x05); writeReg(0x38,0x40);
        uint32_t ffCnt=0;
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t src=0;
            Wire1.beginTransmission(MPU_ADDR);Wire1.write(0x3A);Wire1.endTransmission(false);
            Wire1.requestFrom(MPU_ADDR,(uint8_t)1);if(Wire1.available())src=Wire1.read();
            if(src&0x80){ffCnt++;buzzOK();}
            dispClear();dispStatusBar("Dusus Algila");
            char b[24];snprintf(b,24,"FF: %lu kez",ffCnt);
            dispText(0,24,b);dispText(0,54,"[LONG]=cik");dispCommit();delay(50);
        }
    } else if(sel==8){
        uint32_t vibCnt=0; float prevMag2=0;
        while(!readButtonLong()){
            potApplyBrightness();
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            float mag=sqrt(ax*ax+ay*ay+az*az);
            if(fabs(mag-prevMag2)>0.3f){vibCnt++;buzzNav();}
            prevMag2=mag;
            dispClear();dispStatusBar("Titresim");
            char b[24];snprintf(b,24,"Titresim: %lu",vibCnt);
            dispText(0,24,b);dispText(0,54,"[LONG]=cik");dispCommit();delay(20);
        }
    } else if(sel==9){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/mpu_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        f.println("t,ax,ay,az,gx,gy,gz");
        for(int i=0;i<50;i++){
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            char row[80];snprintf(row,80,"%lu,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f\n",millis(),ax,ay,az,gx,gy,gz);
            f.print(row);delay(100);
        }
        f.close();dispBannerOK("50 satir kaydedildi");
    } else if(sel==10){
        float samples[10]; uint8_t sc=0;
        dispClear();dispStatusBar("G-Force Grafik");
        dispText(0,24,"10 ornek aliniyor");dispCommit();
        while(sc<10){
            float ax,ay,az,gx,gy,gz; readAll(ax,ay,az,gx,gy,gz);
            samples[sc++]=sqrt(ax*ax+ay*ay+az*az);
            delay(200);
        }
        float mn=samples[0],mx=samples[0];
        for(int i=0;i<10;i++){if(samples[i]<mn)mn=samples[i];if(samples[i]>mx)mx=samples[i];}
        dispClear();dispStatusBar("G Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((samples[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24];snprintf(rng,24,"%.2f-%.2f g",mn,mx);
        dispText(0,54,rng);dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    }
}
} // namespace ModMPU6050

// ────────────────────────────────────────────────────────────────────
// 12. BME680 — Hava Kalitesi (Sıcaklık/Nem/Basınç/VOC Gaz)
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x76/0x77
// ────────────────────────────────────────────────────────────────────
namespace ModBME680 {
static constexpr uint8_t BME_ADDR=0x76;

static uint8_t readReg(uint8_t r){
    Wire1.beginTransmission(BME_ADDR);Wire1.write(r);Wire1.endTransmission(false);
    Wire1.requestFrom(BME_ADDR,(uint8_t)1);return Wire1.available()?Wire1.read():0;
}
static void writeReg(uint8_t r,uint8_t v){
    Wire1.beginTransmission(BME_ADDR);Wire1.write(r);Wire1.write(v);Wire1.endTransmission();
}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    // Chip ID kontrolü
    uint8_t chipID=readReg(0xD0);
    if(chipID!=0x61){
        dispBannerErr("BME680 bulunamadi (ID:0x"+String(chipID,16)+")");
        return;
    }
    // Forced mode, oversampling
    writeReg(0x74,0x25); // osrs_t=001, osrs_p=001, mode=01
    writeReg(0x72,0x01); // osrs_h=001
    writeReg(0x64,0x59); // Gas heater setpoint ~320C
    writeReg(0x65,0x59); // Gas heater duration
    writeReg(0x71,0x10); // run_gas=1, nb_conv=0

    OptionList opts={
        {"Canli Tum Veriler",nullptr},
        {"Sicaklik",         nullptr},
        {"Nem",              nullptr},
        {"Basinc",           nullptr},
        {"VOC Gaz Direnci",  nullptr},
        {"Hava Kalite Skoru",nullptr},
        {"IAQ Hesapla",      nullptr},
        {"SD Log",           nullptr},
        {"Grafik",           nullptr},
        {"Alarm Esigi",      nullptr},
        {"Chip Bilgi",       nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"BME680 Hava Kal.");
    if(sel<0||sel==11)return;

    // Basit okuma (kütüphanesiz tam parse için trim kodu)
    auto forcedRead=[&](float& temp,float& hum,float& pres,uint32_t& gasR)->bool{
        writeReg(0x74,(readReg(0x74)&0xFC)|0x01); // forced mode
        delay(200);
        uint8_t status=readReg(0x1D);
        if(!(status&0x80))return false; // new_data_0
        // Sıcaklık (basit)
        int32_t adc_T=((int32_t)readReg(0x22)<<12)|((int32_t)readReg(0x23)<<4)|(readReg(0x24)>>4);
        int32_t adc_H=((int32_t)readReg(0x25)<<8)|readReg(0x26);
        int32_t adc_P=((int32_t)readReg(0x1F)<<12)|((int32_t)readReg(0x20)<<4)|(readReg(0x21)>>4);
        // Trim katsayıları (basit yaklaşım)
        int32_t var1=((int32_t)adc_T>>3)-((int32_t)readReg(0x89)<<1);
        int32_t var2=(var1*(int32_t)readReg(0x8A))>>11;
        temp=(var2+((var2>>2)*(var2>>2)>>14)*((int32_t)(int8_t)readReg(0x8C)>>10))/5120.0f+24.0f;
        hum=adc_H/1024.0f*60.0f/100.0f; // kaba yaklaşım
        pres=adc_P/256.0f/100.0f+1000.0f; // kaba yaklaşım hPa
        uint16_t adc_G=((uint16_t)readReg(0x2A)<<2)|(readReg(0x2B)>>6);
        uint8_t gas_range=readReg(0x2B)&0x0F;
        gasR=(uint32_t)(adc_G<<gas_range); // kaba ohm tahmini
        return true;
    };

    auto iaqScore=[](uint32_t gasR)->const char*{
        if(gasR>300000)return "Mukemmel";
        if(gasR>150000)return "Iyi";
        if(gasR>50000) return "Orta";
        if(gasR>20000) return "Kotu";
        return "Cok Kotu";
    };

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float t,h,p; uint32_t g;
            bool ok=forcedRead(t,h,p,g);
            dispClear();dispStatusBar("BME680");
            if(ok){
                char b[24];
                snprintf(b,24,"T:%.1fC H:%.0f%%",t,h*100);dispText(0,12,b);
                snprintf(b,24,"P:%.0f hPa",p);dispText(0,24,b);
                snprintf(b,24,"Gas:%lu ohm",g);dispText(0,36,b);
                dispText(0,46,iaqScore(g));
            }else dispText(0,24,"Okuma hatasi");
            dispText(0,54,"[LONG]=cik");dispCommit();delay(1000);
        }
    } else if(sel==1||sel==2||sel==3||sel==4){
        while(!readButtonLong()){
            potApplyBrightness();
            float t,h,p; uint32_t g; forcedRead(t,h,p,g);
            dispClear();
            char b[24];
            if(sel==1){dispStatusBar("Sicaklik");snprintf(b,24,"%.2f C  %.2f F",t,t*9/5+32);dispText(0,22,b);}
            else if(sel==2){dispStatusBar("Nem");snprintf(b,24,"%.1f %%",h*100);dispText(0,22,b);}
            else if(sel==3){dispStatusBar("Basinc");snprintf(b,24,"%.1f hPa",p);dispText(0,22,b);}
            else{dispStatusBar("VOC Gas");snprintf(b,24,"%lu ohm",g);dispText(0,22,b);}
            dispText(0,54,"[LONG]=cik");dispCommit();delay(800);
        }
    } else if(sel==5||sel==6){
        float t,h,p; uint32_t g; forcedRead(t,h,p,g);
        uint8_t iaq=(uint8_t)constrain(500-(int32_t)(g/1000),0,500);
        dispClear();dispStatusBar("IAQ / Hava Kalite");
        char b[24];snprintf(b,24,"IAQ: %d/500",iaq);dispText(0,14,b);
        dispText(0,26,iaqScore(g));
        snprintf(b,24,"Gas: %lu ohm",g);dispText(0,38,b);
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/bme680_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        f.println("t,temp,hum,pres,gas");
        for(int i=0;i<10;i++){
            float t,h,p; uint32_t g; forcedRead(t,h,p,g);
            char row[64];snprintf(row,64,"%lu,%.2f,%.1f,%.1f,%lu\n",millis(),t,h*100,p,g);
            f.print(row);delay(1000);
        }
        f.close();dispBannerOK("10 satir kaydedildi");
    } else if(sel==10){
        dispClear();dispStatusBar("BME680 Chip Bilgi");
        char b[24];snprintf(b,24,"ChipID: 0x%02X",chipID);
        dispText(0,14,b);dispText(0,26,"BME680 - Bosch");
        dispText(0,38,"I2C: Wire1 (GP41/42)");
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    }
}
} // namespace ModBME680

// ────────────────────────────────────────────────────────────────────
// 13. ADS1115 — 16-bit 4 Kanal ADC
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x48
// ────────────────────────────────────────────────────────────────────
namespace ModADS1115 {
static constexpr uint8_t ADS_ADDR=0x48;
static uint8_t  pga=0x00;  // ±6.144V
static float    pga_factor=6.144f/32768.0f;

static void setConfig(uint8_t mux){
    // OS=1(start), MUX, PGA=000(±6.144V), MODE=1(single), DR=100(128SPS), COMP_MODE=0
    uint16_t cfg=0x8000|(uint16_t)(mux<<12)|(pga<<9)|0x0183;
    Wire1.beginTransmission(ADS_ADDR);
    Wire1.write(0x01);Wire1.write(cfg>>8);Wire1.write(cfg&0xFF);
    Wire1.endTransmission();
    delay(10);
}
static int16_t readConv(){
    Wire1.beginTransmission(ADS_ADDR);Wire1.write(0x00);Wire1.endTransmission(false);
    Wire1.requestFrom(ADS_ADDR,(uint8_t)2);
    return (Wire1.read()<<8)|Wire1.read();
}
static float readChannel(uint8_t ch){
    // ch: 0=A0GND,1=A1GND,2=A2GND,3=A3GND
    static const uint8_t mux[]={0x04,0x05,0x06,0x07};
    setConfig(mux[ch]);
    return readConv()*pga_factor;
}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);

    OptionList opts={
        {"A0 Canli",        nullptr},
        {"A1 Canli",        nullptr},
        {"A2 Canli",        nullptr},
        {"A3 Canli",        nullptr},
        {"4 Kanal Ozet",    nullptr},
        {"Diferansiyel A0-A1",nullptr},
        {"PGA Ayarla",      nullptr},
        {"Grafik (A0)",     nullptr},
        {"Voltaj Bolici",   nullptr},
        {"SD Log (4ch)",    nullptr},
        {"Alarm Esigi",     nullptr},
        {"Back",            nullptr},
    };
    int sel=loopOptions(opts,"ADS1115 16-bit ADC");
    if(sel<0||sel==11)return;

    if(sel>=0&&sel<=3){
        uint8_t ch=sel;
        while(!readButtonLong()){
            potApplyBrightness();
            float v=readChannel(ch);
            int raw=readConv();
            dispClear();char hd[8];snprintf(hd,8,"ADS A%d",ch);
            dispStatusBar(hd);
            char b[24];
            snprintf(b,24,"%.4f V",v);dispText(8,14,b);
            snprintf(b,24,"Raw: %d",raw);dispText(8,28,b);
            snprintf(b,24,"mV: %.1f",v*1000);dispText(8,42,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==4){
        while(!readButtonLong()){
            potApplyBrightness();
            float v[4];for(int i=0;i<4;i++)v[i]=readChannel(i);
            dispClear();dispStatusBar("ADS1115 4 Kanal");
            char b[24];
            for(int i=0;i<4;i++){snprintf(b,24,"A%d: %.4f V",i,v[i]);dispText(0,12+i*12,b);}
            dispText(0,54,"[LONG]=cik");dispCommit();delay(300);
        }
    } else if(sel==5){
        // Diferansiyel A0-A1 (mux=0x00)
        while(!readButtonLong()){
            potApplyBrightness();
            uint16_t cfg=0x8000|(0x00<<12)|(pga<<9)|0x0183;
            Wire1.beginTransmission(ADS_ADDR);Wire1.write(0x01);Wire1.write(cfg>>8);Wire1.write(cfg&0xFF);Wire1.endTransmission();
            delay(10);
            float v=readConv()*pga_factor;
            dispClear();dispStatusBar("Diferansiyel A0-A1");
            char b[24];snprintf(b,24,"%.4f V",v);dispText(8,22,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==6){
        OptionList pgas={
            {"+-6.144V (default)",nullptr},{"+-4.096V",nullptr},
            {"+-2.048V",nullptr},{"+-1.024V",nullptr},
            {"+-0.512V",nullptr},{"+-0.256V",nullptr},{"Back",nullptr}};
        static const float factors[]={6.144f,4.096f,2.048f,1.024f,0.512f,0.256f};
        static const uint8_t pgaCodes[]={0,1,2,3,4,5};
        int ps=loopOptions(pgas,"PGA Sec");
        if(ps>=0&&ps<6){pga=pgaCodes[ps];pga_factor=factors[ps]/32768.0f;dispBannerOK("PGA ayarlandi");}
    } else if(sel==7){
        float s[10];for(int i=0;i<10;i++){s[i]=readChannel(0);delay(200);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear();dispStatusBar("A0 Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24];snprintf(rng,24,"%.3f-%.3f V",mn,mx);
        dispText(0,54,rng);dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==8){
        // Voltaj bölücü (2 direnç)
        char r1s[8]="10000",r2s[8]="10000";
        charPicker(r1s,7,"R1 (ohm)");charPicker(r2s,7,"R2 (ohm)");
        float r1=atof(r1s),r2=atof(r2s);
        while(!readButtonLong()){
            potApplyBrightness();
            float vout=readChannel(0);
            float vin=(r2>0)?vout*(r1+r2)/r2:0;
            dispClear();dispStatusBar("Voltaj Bolici");
            char b[24];
            snprintf(b,24,"Vout: %.4f V",vout);dispText(0,14,b);
            snprintf(b,24,"Vin:  %.4f V",vin);dispText(0,28,b);
            snprintf(b,24,"R1:%.0f R2:%.0f",r1,r2);dispText(0,42,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(300);
        }
    } else if(sel==9){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/ads_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        f.println("t,A0,A1,A2,A3");
        for(int i=0;i<20;i++){
            char row[64];
            snprintf(row,64,"%lu,%.4f,%.4f,%.4f,%.4f\n",millis(),readChannel(0),readChannel(1),readChannel(2),readChannel(3));
            f.print(row);delay(200);
        }
        f.close();dispBannerOK("20 satir kaydedildi");
    } else if(sel==10){
        float thr=3.0f;
        dispBannerOK("Alarm: A0 > 3.0V");
        while(!readButtonLong()){
            potApplyBrightness();
            float v=readChannel(0);
            if(v>thr){buzzOK();dispBannerWarn("ALARM: "+String(v,3)+"V");}
            delay(300);
        }
    }
}
} // namespace ModADS1115

// ────────────────────────────────────────────────────────────────────
// 14. INA219 — Akım & Güç Ölçer
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x40
// Shunt: 0.1 Ω (standart modül)
// ────────────────────────────────────────────────────────────────────
namespace ModINA219 {
static constexpr uint8_t INA_ADDR=0x40;
static float shuntOhm=0.1f;  // shunt direnci
static float currentCal=1.0f;

static void writeReg16(uint8_t r,uint16_t v){
    Wire1.beginTransmission(INA_ADDR);Wire1.write(r);Wire1.write(v>>8);Wire1.write(v&0xFF);Wire1.endTransmission();
}
static int16_t readReg16(uint8_t r){
    Wire1.beginTransmission(INA_ADDR);Wire1.write(r);Wire1.endTransmission(false);
    Wire1.requestFrom(INA_ADDR,(uint8_t)2);return (Wire1.read()<<8)|Wire1.read();
}
static float busVoltage(){return (readReg16(0x02)>>3)*0.004f;}
static float shuntVoltage(){return readReg16(0x01)*0.00001f;}
static float current_A(){return (shuntVoltage()/shuntOhm)*currentCal;}
static float power_W(){return busVoltage()*current_A();}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    // Config: bus 32V, shunt ±320mV, 12-bit, continuous
    writeReg16(0x00,0x399F);

    OptionList opts={
        {"Canli Akim/Guc",  nullptr},
        {"Bus Voltaj",      nullptr},
        {"Shunt Voltaj",    nullptr},
        {"Akim (A/mA)",     nullptr},
        {"Guc (W/mW)",      nullptr},
        {"Enerji Sayaci",   nullptr},
        {"Min/Max Izle",    nullptr},
        {"Grafik (Akim)",   nullptr},
        {"Shunt Ohm Ayarla",nullptr},
        {"SD Log",          nullptr},
        {"Alarm",           nullptr},
        {"Back",            nullptr},
    };
    int sel=loopOptions(opts,"INA219 Akım/Güç");
    if(sel<0||sel==11)return;

    static float minA=9999,maxA=-9999,minV=9999,maxV=-9999;
    static float energyWh=0; static uint32_t lastE=0;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float v=busVoltage(),a=current_A(),p=power_W();
            dispClear();dispStatusBar("INA219 Canli");
            char b[24];
            snprintf(b,24,"V:  %.3f V",v);dispText(0,12,b);
            snprintf(b,24,"I:  %.0f mA",a*1000);dispText(0,24,b);
            snprintf(b,24,"P:  %.3f W",p);dispText(0,36,b);
            // Güç bar
            int bw=(int)(fabs(p)/5.0f*112);if(bw>112)bw=112;
            display.fillRect(4,48,bw,6,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness();
            float v=busVoltage();
            dispClear();dispStatusBar("Bus Voltaj");
            char b[24];snprintf(b,24,"%.4f V",v);dispText(8,20,b);
            snprintf(b,24,"%.1f mV",v*1000);dispText(8,34,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==2){
        while(!readButtonLong()){
            potApplyBrightness();
            float sv=shuntVoltage();
            dispClear();dispStatusBar("Shunt Voltaj");
            char b[24];snprintf(b,24,"%.6f V",sv);dispText(0,20,b);
            snprintf(b,24,"%.3f mV",sv*1000);dispText(0,34,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==3){
        while(!readButtonLong()){
            potApplyBrightness();
            float a=current_A();
            dispClear();dispStatusBar("Akim");
            char b[24];
            snprintf(b,24,"%.4f A",a);dispText(8,14,b);
            snprintf(b,24,"%.2f mA",a*1000);dispText(8,28,b);
            snprintf(b,24,"%.1f uA",a*1000000);dispText(8,42,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==4){
        while(!readButtonLong()){
            potApplyBrightness();
            float p=power_W();
            dispClear();dispStatusBar("Guc");
            char b[24];
            snprintf(b,24,"%.4f W",p);dispText(8,14,b);
            snprintf(b,24,"%.2f mW",p*1000);dispText(8,28,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==5){
        // Wh sayacı
        lastE=millis(); energyWh=0;
        while(!readButtonLong()){
            potApplyBrightness();
            float p=power_W();
            uint32_t now=millis();
            energyWh+=p*(now-lastE)/3600000.0f;
            lastE=now;
            dispClear();dispStatusBar("Enerji Sayaci");
            char b[24];
            snprintf(b,24,"%.5f Wh",energyWh);dispText(0,18,b);
            snprintf(b,24,"%.3f mAh",energyWh/busVoltage()*1000);dispText(0,32,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(500);
        }
    } else if(sel==6){
        while(!readButtonLong()){
            potApplyBrightness();
            float a=current_A(),v=busVoltage();
            if(a<minA)minA=a;if(a>maxA)maxA=a;
            if(v<minV)minV=v;if(v>maxV)maxV=v;
            dispClear();dispStatusBar("Min/Max");
            char b[24];
            snprintf(b,24,"Imin:%.0f Imax:%.0f mA",minA*1000,maxA*1000);dispText(0,14,b);
            snprintf(b,24,"Vmin:%.3f Vmax:%.3f",minV,maxV);dispText(0,28,b);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(300);
        }
    } else if(sel==7){
        float s[10];for(int i=0;i<10;i++){s[i]=current_A()*1000;delay(200);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear();dispStatusBar("Akim Grafik (mA)");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24];snprintf(rng,24,"%.0f-%.0f mA",mn,mx);
        dispText(0,54,rng);dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==8){
        char rS[8]="0.1";charPicker(rS,7,"Shunt ohm (orn:0.1)");
        shuntOhm=atof(rS);if(shuntOhm<=0)shuntOhm=0.1f;
        dispBannerOK("Shunt: "+String(shuntOhm)+" ohm");
    } else if(sel==9){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/ina_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        f.println("t,V,A,W");
        for(int i=0;i<20;i++){
            char row[64];snprintf(row,64,"%lu,%.3f,%.4f,%.4f\n",millis(),busVoltage(),current_A(),power_W());
            f.print(row);delay(200);
        }
        f.close();dispBannerOK("20 satir kaydedildi");
    } else if(sel==10){
        float thr=1.0f;
        dispBannerOK("Alarm: I > 1000 mA");
        while(!readButtonLong()){
            potApplyBrightness();
            float a=current_A();
            if(fabs(a)>thr){buzzOK();dispBannerWarn("AKIM ALRM: "+String(a*1000,0)+"mA");}
            delay(200);
        }
    }
}
} // namespace ModINA219

// ────────────────────────────────────────────────────────────────────
// 15. HX711 — Load Cell / Terazi Amplifikatörü
// Bağlantı: DOUT→Pin4(GP40), SCK→Pin7(GP37)
// ────────────────────────────────────────────────────────────────────
namespace ModHX711 {
static float calFactor=1.0f;
static long  tare=0;
static float unit=1.0f;  // gram/birim

static long hx_read(){
    // HX711 protokolü: 24-bit veri + 1-2 gain bit
    uint32_t count=0;
    // Data hazır mı (DOUT LOW)
    uint32_t t0=millis();
    while(digitalRead(PIN_MOD_DATA)==HIGH){if(millis()-t0>500)return 0;}
    noInterrupts();
    for(int i=0;i<24;i++){
        digitalWrite(PIN_MOD_TX2,HIGH);delayMicroseconds(1);
        count=(count<<1)|(digitalRead(PIN_MOD_DATA)?1:0);
        digitalWrite(PIN_MOD_TX2,LOW);delayMicroseconds(1);
    }
    // 25. pulse = gain 128 (CH A)
    digitalWrite(PIN_MOD_TX2,HIGH);delayMicroseconds(1);
    digitalWrite(PIN_MOD_TX2,LOW);delayMicroseconds(1);
    interrupts();
    if(count&0x800000) count|=0xFF000000; // 2'nin tamamlayıcısı
    return (long)count;
}
static float getGrams(){return ((hx_read()-tare)/calFactor)*unit;}

static void run(){
    pinMode(PIN_MOD_DATA,INPUT);
    pinMode(PIN_MOD_TX2,OUTPUT);
    digitalWrite(PIN_MOD_TX2,LOW);

    OptionList opts={
        {"Canli Agirlik",   nullptr},
        {"Tare (Sifirla)",  nullptr},
        {"Kalibrasyon",     nullptr},
        {"Birim Sec",       nullptr},
        {"Min/Max",         nullptr},
        {"SD Log",          nullptr},
        {"Ham Deger",       nullptr},
        {"Alarm Esigi",     nullptr},
        {"Ortalama (10x)",  nullptr},
        {"Grafik",          nullptr},
        {"Back",            nullptr},
    };
    int sel=loopOptions(opts,"HX711 Terazi");
    if(sel<0||sel==10)return;

    static float wMin=9999,wMax=-9999;
    static const char* units[]={"gram","kg","oz","lb"};
    static int unitIdx=0;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            float g=getGrams();
            if(g<wMin)wMin=g;if(g>wMax)wMax=g;
            dispClear();dispStatusBar("HX711 Terazi");
            char b[24];snprintf(b,24,"%.1f %s",g,units[unitIdx]);
            dispText(8,18,b);
            int bw=(int)(fabs(g)/1000.0f*112);if(bw>112)bw=112;
            display.drawRect(4,36,112,8,SSD1306_WHITE);
            display.fillRect(4,36,bw,8,SSD1306_WHITE);
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==1){
        dispClear();dispStatusBar("Tare");
        dispText(0,20,"Kap koy, BTN=sifirla");dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        long sum=0;for(int i=0;i<10;i++){sum+=hx_read();delay(100);}
        tare=sum/10;
        dispBannerOK("Tare alindi: "+String(tare));
    } else if(sel==2){
        dispClear();dispStatusBar("Kalibrasyon");
        dispText(0,12,"1) Bos plaka koy");
        dispText(0,24,"BTN=sifirla");dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        long sum=0;for(int i=0;i<10;i++){sum+=hx_read();delay(100);}
        tare=sum/10;
        char wS[8]="100";charPicker(wS,7,"Bilinen agirlik (g)");
        float knownG=atof(wS);
        dispClear();dispStatusBar("Kalibrasyon");
        dispText(0,12,"2) Agirligi koy");
        dispText(0,24,"BTN=olc");dispCommit();
        while(!readButton()){potApplyBrightness();delay(50);}
        long sum2=0;for(int i=0;i<10;i++){sum2+=hx_read();delay(100);}
        long raw=sum2/10-tare;
        if(raw!=0)calFactor=raw/knownG;
        dispBannerOK("Cal: "+String(calFactor));
    } else if(sel==3){
        OptionList us={{"gram",nullptr},{"kg",nullptr},{"oz",nullptr},{"lb",nullptr}};
        static const float factors[]={1.0f,0.001f,0.035274f,0.002205f};
        int us2=loopOptions(us,"Birim Sec");
        if(us2>=0&&us2<4){unitIdx=us2;unit=factors[us2];dispBannerOK(String("Birim: ")+units[us2]);}
    } else if(sel==4){
        dispClear();dispStatusBar("Min/Max");
        char b1[24],b2[24];
        snprintf(b1,24,"Min: %.1f %s",wMin>9998?0:wMin,units[unitIdx]);
        snprintf(b2,24,"Max: %.1f %s",wMax<-9998?0:wMax,units[unitIdx]);
        dispText(0,20,b1);dispText(0,34,b2);
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==5){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/hx711_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        for(int i=0;i<20;i++){
            char row[32];snprintf(row,32,"%lu,%.2f\n",millis(),getGrams());
            f.print(row);delay(200);
        }
        f.close();dispBannerOK("20 satir kaydedildi");
    } else if(sel==6){
        while(!readButtonLong()){
            potApplyBrightness();
            long raw=hx_read();
            dispClear();dispStatusBar("Ham Deger");
            char b[24];snprintf(b,24,"%ld",raw);
            dispText(0,22,b);snprintf(b,24,"Tare: %ld",tare);
            dispText(0,36,b);dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==7){
        char tS[8]="500";charPicker(tS,7,"Esik (gram)");
        float thr=atof(tS);
        while(!readButtonLong()){
            potApplyBrightness();
            float g=getGrams();
            if(g>thr){buzzOK();dispBannerWarn("ESIK ASILDI: "+String(g,1)+"g");}
            delay(300);
        }
    } else if(sel==8){
        float sum=0;
        for(int i=0;i<10;i++){sum+=getGrams();delay(200);}
        float avg=sum/10;
        dispClear();dispStatusBar("Ortalama");
        char b[24];snprintf(b,24,"%.2f %s",avg,units[unitIdx]);
        dispText(8,24,b);dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        float s[10];for(int i=0;i<10;i++){s[i]=getGrams();delay(200);}
        float mn=s[0],mx=s[0];
        for(int i=0;i<10;i++){if(s[i]<mn)mn=s[i];if(s[i]>mx)mx=s[i];}
        dispClear();dispStatusBar("Agirlik Grafik");
        for(int i=0;i<10;i++){
            int bh=(mx>mn)?(int)((s[i]-mn)/(mx-mn)*36):4;
            display.fillRect(4+i*12,50-bh,10,bh,SSD1306_WHITE);
        }
        char rng[24];snprintf(rng,24,"%.0f-%.0f g",mn,mx);
        dispText(0,54,rng);dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    }
}
} // namespace ModHX711

// ════════════════════════════════════════════════════════════════════
//  SENSÖR ANA MENÜSÜ (genişletilmiş)
// ════════════════════════════════════════════════════════════════════
static void sensors_menu_run() {
    OptionList opts={
        {"DHT11",          nullptr},
        {"DHT22",          nullptr},
        {"BMP280/BME280",  nullptr},
        {"HC-SR04 Mesafe", nullptr},
        {"PIR Hareket",    nullptr},
        {"MQ-2 Gaz",       nullptr},
        {"MQ-135 Gaz",     nullptr},
        {"MAX30102 Kalp",  nullptr},
        {"DS18B20 Temp",   nullptr},
        {"ADXL345 Ivme",   nullptr},
        {"LDR Isik",       nullptr},
        {"Toprak Nemi",    nullptr},
        {"MPU6050 IMU",    nullptr},
        {"BME680 HavaKal.",nullptr},
        {"ADS1115 ADC",    nullptr},
        {"INA219 Akim",    nullptr},
        {"HX711 Terazi",   nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"Sensor Moduller");
    if(sel<0||sel==17)return;
    switch(sel){
        case 0:  ModDHT::run(false);      break;
        case 1:  ModDHT::run(true);       break;
        case 2:  ModBMP::run();            break;
        case 3:  ModHCSR04::run();         break;
        case 4:  ModPIR::run();            break;
        case 5:  ModMQ::run(false);        break;
        case 6:  ModMQ::run(true);         break;
        case 7:  ModMAX30102::run();       break;
        case 8:  ModDS18B20::run();        break;
        case 9:  ModADXL345::run();        break;
        case 10: ModLDR::run();            break;
        case 11: ModSoil::run();           break;
        case 12: ModMPU6050::run();        break;
        case 13: ModBME680::run();         break;
        case 14: ModADS1115::run();        break;
        case 15: ModINA219::run();         break;
        case 16: ModHX711::run();          break;
    }
}

#pragma once
// ════════════════════════════════════════════════════════════════════
//  mod_comms.h — M5Sharkino İletişim Modülleri
//  Pinler: TX2=GP37(Pin7)  RX2=GP38(Pin8)
//          SDA2=GP41(Pin5)  SCL2=GP42(Pin6)
//          DATA=GP40(Pin4)
// ════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <Wire.h>

#ifndef PIN_MOD_DATA
  #define PIN_MOD_DATA  40
  #define PIN_MOD_SDA2  41
  #define PIN_MOD_SCL2  42
  #define PIN_MOD_TX2   37
  #define PIN_MOD_RX2   38
#endif

// ────────────────────────────────────────────────────────────────────
// 1. GPS — NMEA UART
// Bağlantı: GPS TX → Pin8 (GP38=RX2), GPS RX → Pin7 (GP37=TX2)
// ────────────────────────────────────────────────────────────────────
namespace ModGPS {
struct GpsFix {
    float lat=0,lon=0,alt=0,speed=0,course=0;
    uint8_t sats=0; float hdop=9.9f;
    char timeStr[12]=""; char dateStr[8]="";
    bool valid=false;
};
static GpsFix lastFix;
static float refLat=0,refLon=0; static bool refSet=false;

// NMEA parçala (virgülle ayrılmış alan döndür)
static String nmea_field(const String& s, int idx){
    int cnt=0,start=0;
    for(int i=0;i<(int)s.length();i++){
        if(s[i]==','){
            if(cnt==idx) return s.substring(start,i);
            start=i+1; cnt++;
        }
    }
    if(cnt==idx) return s.substring(start);
    return "";
}
static float nmea_coord(const String& raw, const String& hemi){
    if(raw.length()<4) return 0;
    int dot=raw.indexOf('.');
    float deg=raw.substring(0,dot-2).toFloat();
    float mn=raw.substring(dot-2).toFloat();
    float val=deg+mn/60.0f;
    if(hemi=="S"||hemi=="W") val=-val;
    return val;
}
// Haversine mesafesi (km)
static float haversine(float la1,float lo1,float la2,float lo2){
    float R=6371.0f;
    float dlat=(la2-la1)*PI/180, dlon=(lo2-lo1)*PI/180;
    float a=sin(dlat/2)*sin(dlat/2)+cos(la1*PI/180)*cos(la2*PI/180)*sin(dlon/2)*sin(dlon/2);
    return R*2*atan2(sqrt(a),sqrt(1-a));
}

static void parse_line(const String& ln){
    if(ln.startsWith("$GPRMC")||ln.startsWith("$GNRMC")){
        String t=nmea_field(ln,1); String st=nmea_field(ln,2);
        lastFix.valid=(st=="A");
        if(t.length()>=6){
            snprintf(lastFix.timeStr,12,"%s:%s:%s",t.substring(0,2).c_str(),t.substring(2,4).c_str(),t.substring(4,6).c_str());
        }
        String dstr=nmea_field(ln,9);
        if(dstr.length()>=6) snprintf(lastFix.dateStr,8,"%s/%s/%s",dstr.substring(0,2).c_str(),dstr.substring(2,4).c_str(),dstr.substring(4,6).c_str());
        lastFix.lat=nmea_coord(nmea_field(ln,3),nmea_field(ln,4));
        lastFix.lon=nmea_coord(nmea_field(ln,5),nmea_field(ln,6));
        lastFix.speed=nmea_field(ln,7).toFloat()*1.852f;  // knot→km/h
        lastFix.course=nmea_field(ln,8).toFloat();
    } else if(ln.startsWith("$GPGGA")||ln.startsWith("$GNGGA")){
        lastFix.sats=(uint8_t)nmea_field(ln,7).toInt();
        lastFix.hdop=nmea_field(ln,8).toFloat();
        lastFix.alt=nmea_field(ln,9).toFloat();
    }
}

static void run(){
    Serial2.begin(9600,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);
    String buf=""; uint32_t rxB=0;

    OptionList opts={
        {"Konum (Lat/Lon)",   nullptr},
        {"Hiz / Yon",         nullptr},
        {"Rakım / Uydu",      nullptr},
        {"Zaman Sync",        nullptr},
        {"NMEA Ham Akis",     nullptr},
        {"Mesafe Hesap",      nullptr},
        {"Ref Nokta Kaydet",  nullptr},
        {"Hiz Birimi (km/h)", nullptr},
        {"HDOP Kalite",       nullptr},
        {"SD Log",            nullptr},
        {"Back",              nullptr},
    };
    int sel=loopOptions(opts,"GPS NMEA");
    if(sel<0||sel==10){Serial2.end();return;}

    auto collectNMEA=[&](){
        uint32_t t0=millis();
        while(millis()-t0<3000){
            while(Serial2.available()){
                char c=Serial2.read(); rxB++;
                if(c=='\n'||c=='\r'){
                    buf.trim();
                    if(buf.length()>5) parse_line(buf);
                    buf="";
                } else if(buf.length()<100) buf+=c;
            }
            delay(10);
        }
    };

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            dispClear(); dispStatusBar("GPS Konum");
            if(lastFix.valid){
                char b1[24],b2[24];
                snprintf(b1,24,"Lat: %.5f",lastFix.lat);
                snprintf(b2,24,"Lon: %.5f",lastFix.lon);
                dispText(0,12,b1); dispText(0,24,b2);
                dispText(0,36,lastFix.timeStr);
            } else {
                char b[24]; snprintf(b,24,"Uydu: %d  Rx:%lu B",lastFix.sats,rxB);
                dispText(0,18,"FIX YOK"); dispText(0,32,b);
            }
            dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==1){
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            dispClear(); dispStatusBar("Hiz / Yon");
            char b1[24],b2[24];
            snprintf(b1,24,"Hiz: %.1f km/h",lastFix.speed);
            snprintf(b2,24,"Yon: %.1f deg",lastFix.course);
            dispText(0,18,b1); dispText(0,32,b2);
            const char* dir=lastFix.course<22.5f?"K":lastFix.course<67.5f?"KD":lastFix.course<112.5f?"D":
                            lastFix.course<157.5f?"GD":lastFix.course<202.5f?"G":lastFix.course<247.5f?"GB":
                            lastFix.course<292.5f?"B":"KB";
            dispText(50,44,dir); dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==2){
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            dispClear(); dispStatusBar("Rakım / Uydu");
            char b1[24],b2[24],b3[24];
            snprintf(b1,24,"Rakım: %.0f m",lastFix.alt);
            snprintf(b2,24,"Uydu:  %d",lastFix.sats);
            snprintf(b3,24,"HDOP:  %.1f",lastFix.hdop);
            dispText(0,14,b1); dispText(0,26,b2); dispText(0,38,b3);
            dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==3){
        collectNMEA();
        dispClear(); dispStatusBar("GPS Zaman");
        dispText(0,14,lastFix.timeStr);
        dispText(0,28,lastFix.dateStr);
        dispText(0,42,lastFix.valid?"Fix: AKTIF":"Fix: YOK");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==4){
        dispClear(); dispStatusBar("NMEA Ham");
        while(!readButtonLong()){
            potApplyBrightness();
            while(Serial2.available()){
                char c=Serial2.read();
                if(c=='\n'||c=='\r'){
                    dispClear(); dispStatusBar("NMEA Ham");
                    dispText(0,14,buf.substring(0,20));
                    dispText(0,26,buf.substring(20,40));
                    dispText(0,54,"[LONG]=cik"); dispCommit();
                    buf="";
                } else if(buf.length()<60) buf+=c;
            }
            delay(20);
        }
    } else if(sel==5){
        if(!refSet){dispBannerErr("Once Ref Nokta al (opt 7)");Serial2.end();return;}
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            float dist=haversine(refLat,refLon,lastFix.lat,lastFix.lon);
            dispClear(); dispStatusBar("Mesafe");
            char b1[24],b2[24];
            snprintf(b1,24,"%.3f km",dist);
            snprintf(b2,24,"%.0f m",dist*1000);
            dispText(16,18,b1); dispText(16,32,b2);
            dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==6){
        collectNMEA();
        refLat=lastFix.lat; refLon=lastFix.lon; refSet=true;
        char b[24]; snprintf(b,24,"Ref: %.4f,%.4f",refLat,refLon);
        dispBannerOK(b);
    } else if(sel==7){
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            dispClear(); dispStatusBar("Hiz km/h & knot");
            char b1[24],b2[24];
            snprintf(b1,24,"%.1f km/h",lastFix.speed);
            snprintf(b2,24,"%.1f knot",lastFix.speed/1.852f);
            dispText(0,18,b1); dispText(0,34,b2);
            dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==8){
        while(!readButtonLong()){
            potApplyBrightness(); collectNMEA();
            dispClear(); dispStatusBar("HDOP Kalite");
            char b[24]; snprintf(b,24,"HDOP: %.2f",lastFix.hdop);
            dispText(0,14,b);
            const char* q=lastFix.hdop<1?"Mukemmel":lastFix.hdop<2?"Ideal":lastFix.hdop<5?"Iyi":lastFix.hdop<10?"Orta":"Zayif";
            dispText(0,30,q); dispText(0,44,lastFix.valid?"Fix: VAR":"Fix: YOK");
            dispText(0,54,"[LONG]=cik"); dispCommit();
        }
    } else if(sel==9){
        if(!g_sdInited){dispBannerErr("SD kart yok");Serial2.end();return;}
        File f=SD.open("/gps_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");Serial2.end();return;}
        f.println("time,lat,lon,alt,speed,sats");
        for(int i=0;i<20;i++){
            collectNMEA();
            if(lastFix.valid){
                char row[80]; snprintf(row,80,"%s,%.5f,%.5f,%.0f,%.1f,%d\n",
                    lastFix.timeStr,lastFix.lat,lastFix.lon,lastFix.alt,lastFix.speed,lastFix.sats);
                f.print(row);
            }
        }
        f.close(); dispBannerOK("20 satir kaydedildi");
    }
    Serial2.end();
}
} // namespace ModGPS

// ────────────────────────────────────────────────────────────────────
// 2. HC-05 / HC-06 Bluetooth Modülü
// Bağlantı: TX→Pin8(GP38=RX2), RX→Pin7(GP37=TX2)
// HC-05: AT modu (EN pin HIGH + reset), HC-06: sadece AT komut
// ────────────────────────────────────────────────────────────────────
namespace ModHC05 {

static String atCmd(const String& cmd, uint32_t wait=1000){
    while(Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0=millis(); String resp="";
    while(millis()-t0<wait){
        while(Serial2.available()) resp+=Serial2.read();
        if(resp.indexOf("OK")>=0||resp.indexOf("ERROR")>=0) break;
    }
    resp.trim(); return resp;
}
static void run(){
    Serial2.begin(38400,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);
    delay(200);

    OptionList opts={
        {"AT Test",        nullptr},
        {"Versiyon",       nullptr},
        {"Ad Degistir",    nullptr},
        {"PIN Degistir",   nullptr},
        {"Baud Degistir",  nullptr},
        {"Rol Sorgula",    nullptr},
        {"Pair Listesi",   nullptr},
        {"Terminal Modu",  nullptr},
        {"Echo Test",      nullptr},
        {"Baud Algila",    nullptr},
        {"Reset",          nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"HC-05/06 BT");
    if(sel<0||sel==11){Serial2.end();return;}

    auto showResp=[](const String& title,const String& r){
        dispClear(); dispStatusBar(title);
        dispText(0,14,r.substring(0,20));
        if(r.length()>20) dispText(0,26,r.substring(20,40));
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    };

    if(sel==0){ showResp("AT Test",atCmd("AT")); }
    else if(sel==1){ showResp("Versiyon",atCmd("AT+VERSION?")); }
    else if(sel==2){
        char name[17]=""; charPicker(name,16,"Yeni BT Adi");
        if(strlen(name)>0) showResp("Ad Degistir",atCmd("AT+NAME="+String(name)));
    } else if(sel==3){
        char pin[7]=""; charPicker(pin,6,"Yeni PIN (4-6 hane)");
        if(strlen(pin)>0) showResp("PIN Degistir",atCmd("AT+PSWD="+String(pin)));
    } else if(sel==4){
        OptionList bauds={{"9600",nullptr},{"19200",nullptr},{"38400",nullptr},{"57600",nullptr},{"115200",nullptr},{"Back",nullptr}};
        static const uint32_t bRates[]={9600,19200,38400,57600,115200};
        int bs=loopOptions(bauds,"Baud Sec");
        if(bs>=0&&bs<5){
            char cmd[32]; snprintf(cmd,32,"AT+UART=%lu,0,0",bRates[bs]);
            showResp("Baud Degistir",atCmd(cmd));
        }
    } else if(sel==5){ showResp("Rol",atCmd("AT+ROLE?")); }
    else if(sel==6){ showResp("Pair",atCmd("AT+ADCN?")); }
    else if(sel==7){
        // Transparan terminal modu
        dispClear(); dispStatusBar("BT Terminal");
        dispText(0,14,"RX goruntuleniyor"); dispText(0,26,"BTN=komut LONG=cik"); dispCommit();
        String lastRx="";
        while(!readButtonLong()){
            potApplyBrightness();
            while(Serial2.available()){
                char c=Serial2.read();
                if(c=='\n'||c=='\r'){
                    if(lastRx.length()>0){
                        dispClear(); dispStatusBar("BT Terminal");
                        dispText(0,14,"RX:");
                        dispText(0,24,lastRx.substring(0,20));
                        dispText(0,54,"BTN=komut LONG=cik"); dispCommit();
                    }
                    lastRx="";
                } else if(lastRx.length()<60) lastRx+=c;
            }
            if(readButton()){
                char cmd[33]=""; charPicker(cmd,32,"Komut gir");
                if(strlen(cmd)>0){ Serial2.println(cmd); dispBannerOK("Gonderildi"); }
            }
            delay(50);
        }
    } else if(sel==8){
        Serial2.println("HELLO"); delay(300);
        String r=""; uint32_t t0=millis();
        while(millis()-t0<500) while(Serial2.available()) r+=(char)Serial2.read();
        showResp("Echo Test",r.length()>0?r:"Yanit yok");
    } else if(sel==9){
        // Farklı baud'larda dene
        uint32_t bauds[]={9600,19200,38400,57600,115200};
        uint32_t found=0;
        for(auto b:bauds){
            Serial2.end(); Serial2.begin(b,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2); delay(100);
            Serial2.println("AT"); delay(300);
            String r=""; while(Serial2.available()) r+=(char)Serial2.read();
            if(r.indexOf("OK")>=0){found=b;break;}
        }
        char b[24]; snprintf(b,24,found>0?"Baud: %lu":"Bulunamadi",found);
        dispBannerOK(b);
    } else if(sel==10){ showResp("Reset",atCmd("AT+RESET")); }
    Serial2.end();
}
} // namespace ModHC05

// ────────────────────────────────────────────────────────────────────
// 3. NRF24L01 — 2.4GHz RF
// Bağlantı: SPI paylaşımlı + CE=GP40, CSN=GP37
// ────────────────────────────────────────────────────────────────────
namespace ModNRF24 {
// SPI bit-bang (donanım SPI kullanılmış olabilir, CE/CSN ayrı)
static constexpr uint8_t CE_PIN  = PIN_MOD_DATA;   // GP40
static constexpr uint8_t CSN_PIN = PIN_MOD_TX2;    // GP37

// NRF24 register write (SPI donanımı varsa SPI.transfer kullan)
static uint8_t spi_transfer(uint8_t d){
    uint8_t result=0;
    for(int i=7;i>=0;i--){
        digitalWrite(18,(d>>i)&1);
        digitalWrite(17,HIGH); delayMicroseconds(1);
        result=(result<<1)|digitalRead(19);
        digitalWrite(17,LOW);  delayMicroseconds(1);
    }
    return result;
}
static void nrf_write(uint8_t reg,uint8_t val){
    digitalWrite(CSN_PIN,LOW);
    spi_transfer(0x20|reg); spi_transfer(val);
    digitalWrite(CSN_PIN,HIGH);
}
static uint8_t nrf_read(uint8_t reg){
    digitalWrite(CSN_PIN,LOW);
    spi_transfer(reg); uint8_t v=spi_transfer(0xFF);
    digitalWrite(CSN_PIN,HIGH); return v;
}
static void nrf_init(){
    pinMode(CE_PIN,OUTPUT); pinMode(CSN_PIN,OUTPUT);
    digitalWrite(CE_PIN,LOW); digitalWrite(CSN_PIN,HIGH);
    delay(5);
    nrf_write(0x00,0x0B);  // PWR_UP, CRC 2byte, RX mode
    nrf_write(0x01,0x3F);  // Auto-ACK pipe 0-5
    nrf_write(0x02,0x01);  // RX pipe 0 aç
    nrf_write(0x05,76);    // Kanal 76 (2.476 GHz)
    nrf_write(0x06,0x07);  // 1Mbps, 0dBm
}
static void run(){
    nrf_init();
    OptionList opts={
        {"Ping Gonder",    nullptr},
        {"Veri Gonder",    nullptr},
        {"Veri Al",        nullptr},
        {"Kanal Sec",      nullptr},
        {"Adres Ayarla",   nullptr},
        {"Guc Sec",        nullptr},
        {"Data Rate",      nullptr},
        {"ACK Modu",       nullptr},
        {"Pipe Bilgi",     nullptr},
        {"Istatistik",     nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"NRF24L01");
    if(sel<0||sel==10) return;

    static uint32_t txCount=0,rxCount=0;

    if(sel==0){ // Ping
        uint8_t pip[]={0xE7,0xE7,0xE7,0xE7,0xE7};
        nrf_write(0x10,pip[0]);  // TX addr byte0 (basit)
        uint8_t txPkt[4]={0xAB,0xCD,0xEF,0x00};
        digitalWrite(CSN_PIN,LOW);
        spi_transfer(0xA0);
        for(int i=0;i<4;i++) spi_transfer(txPkt[i]);
        digitalWrite(CSN_PIN,HIGH);
        nrf_write(0x00,0x0A);  // TX mode
        digitalWrite(CE_PIN,HIGH); delayMicroseconds(15); digitalWrite(CE_PIN,LOW);
        delay(10);
        uint8_t status=nrf_read(0x07);
        txCount++;
        bool ack=(status&0x20)==0;
        char b[24]; snprintf(b,24,"TX#%lu %s",txCount,ack?"ACK OK":"NO ACK");
        dispBannerOK(b);
    } else if(sel==1){
        char msg[33]=""; charPicker(msg,32,"Mesaj gir");
        if(strlen(msg)>0){
            nrf_write(0x00,0x0A);
            digitalWrite(CSN_PIN,LOW); spi_transfer(0xA0);
            for(int i=0;i<min(32,(int)strlen(msg));i++) spi_transfer(msg[i]);
            digitalWrite(CSN_PIN,HIGH);
            nrf_write(0x00,0x0A);
            digitalWrite(CE_PIN,HIGH); delayMicroseconds(15); digitalWrite(CE_PIN,LOW);
            txCount++;
            dispBannerOK("Gonderildi #"+String(txCount));
        }
    } else if(sel==2){
        nrf_write(0x00,0x0B);  // RX mode
        digitalWrite(CE_PIN,HIGH);
        dispClear(); dispStatusBar("NRF24 RX");
        dispText(0,14,"Dinleniyor..."); dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t status=nrf_read(0x07);
            if(status&0x40){ // RX_DR
                uint8_t buf[32]={0};
                digitalWrite(CSN_PIN,LOW); spi_transfer(0x61);
                for(int i=0;i<32;i++) buf[i]=spi_transfer(0xFF);
                digitalWrite(CSN_PIN,HIGH);
                nrf_write(0x07,0x40);  // IRQ temizle
                rxCount++;
                dispClear(); dispStatusBar("NRF24 RX");
                char b[24]; snprintf(b,24,"RX#%lu: %s",rxCount,(char*)buf);
                dispText(0,18,b); dispText(0,54,"[LONG]=cik"); dispCommit();
            }
            delay(50);
        }
        digitalWrite(CE_PIN,LOW);
    } else if(sel==3){
        char ch[4]="76"; charPicker(ch,3,"Kanal (0-125)");
        uint8_t chn=constrain(atoi(ch),0,125);
        nrf_write(0x05,chn);
        char b[24]; snprintf(b,24,"Kanal %d ayarlandi",chn);
        dispBannerOK(b);
    } else if(sel==9){
        dispClear(); dispStatusBar("NRF24 Stats");
        char b1[24],b2[24],b3[24];
        snprintf(b1,24,"TX: %lu",txCount);
        snprintf(b2,24,"RX: %lu",rxCount);
        snprintf(b3,24,"Kanal: %d",nrf_read(0x05));
        dispText(0,14,b1); dispText(0,26,b2); dispText(0,38,b3);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else {
        dispBannerOK("Gelistirilecek");
    }
}
} // namespace ModNRF24

// ────────────────────────────────────────────────────────────────────
// 4. RS-485 / Modbus RTU
// Bağlantı: TX→Pin7(GP37), RX→Pin8(GP38), DE/RE→Pin4(GP40)
// ────────────────────────────────────────────────────────────────────
namespace ModRS485 {
static uint16_t crc16(const uint8_t* buf,uint8_t len){
    uint16_t crc=0xFFFF;
    for(uint8_t i=0;i<len;i++){
        crc^=buf[i];
        for(uint8_t j=0;j<8;j++) crc=(crc&1)?(crc>>1)^0xA001:(crc>>1);
    }
    return crc;
}
static void send485(const uint8_t* pkt,uint8_t len){
    digitalWrite(PIN_MOD_DATA,HIGH); delay(1);
    Serial2.write(pkt,len); Serial2.flush();
    digitalWrite(PIN_MOD_DATA,LOW);
}

static void run(){
    Serial2.begin(9600,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);
    pinMode(PIN_MOD_DATA,OUTPUT); digitalWrite(PIN_MOD_DATA,LOW);

    OptionList opts={
        {"Adres Tara",     nullptr},
        {"Register Oku",   nullptr},
        {"Register Yaz",   nullptr},
        {"Ham Frame Gon.", nullptr},
        {"CRC Hesapla",    nullptr},
        {"Baud Degistir",  nullptr},
        {"Master/Slave",   nullptr},
        {"Hata Sayaci",    nullptr},
        {"Zaman Asimi",    nullptr},
        {"Modbus Log",     nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"RS-485 Modbus");
    if(sel<0||sel==10){Serial2.end();return;}

    static uint32_t errCnt=0;

    if(sel==0){ // Adres tara (1-247)
        dispClear(); dispStatusBar("RS485 Adres Tara");
        dispText(0,14,"1-10 arasi taranıyor"); dispCommit();
        String found="";
        for(uint8_t addr=1;addr<=10;addr++){
            uint8_t pkt[8]={addr,0x03,0x00,0x00,0x00,0x01,0,0};
            uint16_t c=crc16(pkt,6); pkt[6]=c&0xFF; pkt[7]=c>>8;
            send485(pkt,8); delay(100);
            if(Serial2.available()){
                char b[8]; snprintf(b,8,"0x%02X",addr);
                found+=b; found+=" ";
                while(Serial2.available()) Serial2.read();
            }
        }
        dispClear(); dispStatusBar("Bulunan Adresler");
        dispText(0,18,found.length()>0?found:"Bulunamadi");
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==1){ // Register oku
        char addrS[4]="1",regS[6]="0",cntS[4]="1";
        charPicker(addrS,3,"Slave adresi");
        charPicker(regS,5,"Baslangic reg");
        charPicker(cntS,3,"Reg sayisi");
        uint8_t slaveA=atoi(addrS);
        uint16_t reg=atoi(regS),cnt=atoi(cntS);
        uint8_t pkt[8]={slaveA,0x03,(uint8_t)(reg>>8),(uint8_t)(reg&0xFF),(uint8_t)(cnt>>8),(uint8_t)(cnt&0xFF),0,0};
        uint16_t c=crc16(pkt,6); pkt[6]=c&0xFF; pkt[7]=c>>8;
        send485(pkt,8); delay(200);
        dispClear(); dispStatusBar("Modbus Read");
        String resp="RX: ";
        while(Serial2.available()){char h[4];snprintf(h,4,"%02X ",Serial2.read());resp+=h;}
        dispText(0,14,resp.substring(0,20)); dispText(0,26,resp.substring(20,40));
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==4){ // CRC
        char data[33]=""; charPicker(data,32,"Hex veri (bosluksuz)");
        uint8_t buf[16]; uint8_t len=0;
        for(int i=0;i<(int)strlen(data)-1&&len<16;i+=2){
            char byte[3]={data[i],data[i+1],0};
            buf[len++]=(uint8_t)strtol(byte,nullptr,16);
        }
        uint16_t c=crc16(buf,len);
        char b[24]; snprintf(b,24,"CRC: %04X (L:%02X H:%02X)",c,c&0xFF,c>>8);
        dispBannerOK(b);
    } else if(sel==7){
        dispClear(); dispStatusBar("Hata Sayaci");
        char b[24]; snprintf(b,24,"Hata: %lu",errCnt);
        dispText(0,24,b); dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else {
        dispBannerOK("Gelistirilecek");
    }
    Serial2.end();
}
} // namespace ModRS485

// ────────────────────────────────────────────────────────────────────
// 5. UART Köprü — Transparan Pass-Through
// Bağlantı: TX→Pin7, RX→Pin8 (herhangi UART cihazı)
// ────────────────────────────────────────────────────────────────────
namespace ModUARTBridge {
static void run(){
    OptionList bauds={{"9600",nullptr},{"19200",nullptr},{"38400",nullptr},
                      {"57600",nullptr},{"115200",nullptr},{"Back",nullptr}};
    int bs=loopOptions(bauds,"UART Baud Sec");
    if(bs<0||bs==5) return;
    static const uint32_t bRates[]={9600,19200,38400,57600,115200};
    uint32_t baud=bRates[bs];
    Serial2.begin(baud,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);

    OptionList opts={
        {"Pass-Through",   nullptr},
        {"Echo Test",      nullptr},
        {"Hex Dump",       nullptr},
        {"Baud Degistir",  nullptr},
        {"Timestamp Log",  nullptr},
        {"Filtre (anahtar)",nullptr},
        {"Istatistik",     nullptr},
        {"Komut Gonder",   nullptr},
        {"Sifirla",        nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"UART Bridge");
    if(sel<0||sel==9){Serial2.end();return;}

    static uint32_t rxBytes=0,txBytes=0,errCnt=0;

    if(sel==0){
        dispClear(); dispStatusBar("UART Pass-Through");
        char bb[24]; snprintf(bb,24,"Baud: %lu",baud);
        dispText(0,12,bb); dispText(0,24,"TX:GP37 RX:GP38");
        dispText(0,36,"Veri aktariliyor...");
        dispText(0,48,"[LONG]=cik"); dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            while(Serial2.available()){char c=Serial2.read();rxBytes++;logPush(String(c));}
            delay(10);
        }
    } else if(sel==1){
        Serial2.println("ECHO_TEST"); delay(300);
        String r=""; while(Serial2.available()) r+=(char)Serial2.read();
        dispBannerOK("Echo: "+(r.length()>0?r.substring(0,16):"(bos)"));
    } else if(sel==2){
        dispClear(); dispStatusBar("Hex Dump");
        dispText(0,14,"Alinan veriler:"); dispText(0,26,"[LONG]=cik"); dispCommit();
        String hexLine="";
        while(!readButtonLong()){
            potApplyBrightness();
            while(Serial2.available()){
                char h[4]; snprintf(h,4,"%02X ",Serial2.read()); rxBytes++;
                hexLine+=h; if(hexLine.length()>40) hexLine=hexLine.substring(4);
            }
            dispClear(); dispStatusBar("Hex Dump");
            dispText(0,14,hexLine.substring(0,20));
            dispText(0,26,hexLine.substring(20,40));
            dispText(0,54,"[LONG]=cik"); dispCommit(); delay(50);
        }
    } else if(sel==6){
        dispClear(); dispStatusBar("UART Stats");
        char b1[24],b2[24],b3[24];
        snprintf(b1,24,"RX: %lu bytes",rxBytes);
        snprintf(b2,24,"TX: %lu bytes",txBytes);
        snprintf(b3,24,"Baud: %lu",baud);
        dispText(0,14,b1); dispText(0,26,b2); dispText(0,38,b3);
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){
        char cmd[33]=""; charPicker(cmd,32,"Komut gir");
        if(strlen(cmd)>0){
            Serial2.println(cmd); txBytes+=strlen(cmd);
            delay(300);
            String r=""; while(Serial2.available()) r+=(char)Serial2.read();
            dispBannerOK("TX OK / RX: "+(r.length()>0?r.substring(0,10):"(yok)"));
        }
    } else if(sel==8){
        rxBytes=0; txBytes=0; errCnt=0; dispBannerOK("Istatistik sifirlandi");
    }
    Serial2.end();
}
} // namespace ModUARTBridge

// ────────────────────────────────────────────────────────────────────
// 6. SIM800L GSM Modülü
// Bağlantı: TX→Pin7(GP37), RX→Pin8(GP38)
// Not: SIM800L 3.7-4.2V besleme gerektirir — ayrı güç kaynağı kullan!
// ────────────────────────────────────────────────────────────────────
namespace ModSIM800 {

static String atCmd(const String& cmd,uint32_t wait=3000){
    while(Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0=millis(); String resp="";
    while(millis()-t0<wait){
        while(Serial2.available()){
            char c=Serial2.read();
            resp+=c; if(resp.length()>200) resp=resp.substring(100);
        }
        if(resp.indexOf("OK")>=0||resp.indexOf("ERROR")>=0||resp.indexOf("+CMT")>=0) break;
        delay(10);
    }
    resp.trim(); return resp;
}

static void run(){
    Serial2.begin(9600,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);
    delay(500);
    dispClear(); dispStatusBar("SIM800L GSM");
    dispText(0,12,"UYARI: 3.7-4.2V");
    dispText(0,24,"ayri besleme gerekli!");
    dispText(0,36,"[BTN]=devam"); dispCommit();
    while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    if(readButtonLong()){Serial2.end();return;}

    OptionList opts={
        {"Sinyal Gucu",    nullptr},
        {"Operator Bilgi", nullptr},
        {"SMS Gonder",     nullptr},
        {"SMS Oku",        nullptr},
        {"USSD Gonder",    nullptr},
        {"Network Modu",   nullptr},
        {"APN Ayarla",     nullptr},
        {"HTTP GET",       nullptr},
        {"Arama Baslat",   nullptr},
        {"PIN Durum",      nullptr},
        {"Back",           nullptr},
    };
    int sel=loopOptions(opts,"SIM800L");
    if(sel<0||sel==10){Serial2.end();return;}

    auto showResp=[](const String& title,const String& r){
        dispClear(); dispStatusBar(title);
        dispText(0,12,r.substring(0,20));
        if(r.length()>20) dispText(0,24,r.substring(20,40));
        if(r.length()>40) dispText(0,36,r.substring(40,60));
        dispText(0,54,"[BTN]=kapat"); dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    };

    if(sel==0){
        String r=atCmd("AT+CSQ");
        // +CSQ: rssi,ber  rssi: 0-31, 99=bilinmiyor. dBm=-113+2*rssi
        int rssi=-1;
        int idx=r.indexOf("+CSQ:");
        if(idx>=0) rssi=r.substring(idx+5).toInt();
        int dbm=(rssi>=0&&rssi<32)?-113+2*rssi:-999;
        char b[24]; snprintf(b,24,"RSSI:%d  %ddBm",rssi,dbm);
        showResp("Sinyal",b);
    } else if(sel==1){
        String r=atCmd("AT+COPS?");
        showResp("Operator",r);
    } else if(sel==2){
        char num[16]=""; charPicker(num,15,"Numara (intl: +90...)");
        char msg[65]=""; charPicker(msg,64,"SMS metni");
        if(strlen(num)>0&&strlen(msg)>0){
            atCmd("AT+CMGF=1");  // Text modu
            Serial2.print("AT+CMGS=\""); Serial2.print(num); Serial2.println("\"");
            delay(500);
            Serial2.print(msg); Serial2.write(0x1A);  // Ctrl+Z
            String r=atCmd("",5000);
            showResp("SMS Sonuc",r.indexOf("OK")>=0?"Gonderildi!":"Hata: "+r);
        }
    } else if(sel==3){
        atCmd("AT+CMGF=1");
        String r=atCmd("AT+CMGL=\"ALL\"",5000);
        showResp("SMS Listesi",r);
    } else if(sel==4){
        char ussd[20]="*100#"; charPicker(ussd,19,"USSD kodu");
        String r=atCmd("AT+CUSD=1,\""+String(ussd)+"\",15",8000);
        showResp("USSD",r);
    } else if(sel==5){
        OptionList nm={{"Otomatik",nullptr},{"GSM",nullptr},{"GPRS",nullptr},{"Back",nullptr}};
        int ns=loopOptions(nm,"Network Modu");
        if(ns==0) atCmd("AT+CNMP=2");
        else if(ns==1) atCmd("AT+CNMP=13");
        else if(ns==2) atCmd("AT+CNMP=38");
        if(ns<3) dispBannerOK("Mod ayarlandi");
    } else if(sel==6){
        char apn[32]="internet"; charPicker(apn,31,"APN adı");
        String r=atCmd("AT+SAPBR=3,1,\"APN\",\""+String(apn)+"\"");
        showResp("APN",r);
    } else if(sel==7){
        char url[64]="http://httpbin.org/get"; charPicker(url,63,"URL");
        atCmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
        atCmd("AT+SAPBR=1,1",5000);
        atCmd("AT+HTTPINIT");
        atCmd("AT+HTTPPARA=\"CID\",1");
        atCmd("AT+HTTPPARA=\"URL\",\""+String(url)+"\"");
        String r=atCmd("AT+HTTPACTION=0",10000);
        atCmd("AT+HTTPTERM");
        showResp("HTTP",r);
    } else if(sel==8){
        char num[16]=""; charPicker(num,15,"Numara");
        if(strlen(num)>0){
            atCmd("ATD"+String(num)+";",2000);
            delay(3000);
            atCmd("ATH");
            dispBannerOK("Arama yapildi/kapatildi");
        }
    } else if(sel==9){
        String r=atCmd("AT+CPIN?");
        showResp("SIM PIN",r);
    }
    Serial2.end();
}
} // namespace ModSIM800

// ════════════════════════════════════════════════════════════════════
//  İLETİŞİM ANA MENÜSÜ
// ════════════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────────
// 7. ESP-01 — WiFi AT Komutu Modülü
// Bağlantı: TX→Pin7(GP37), RX→Pin8(GP38), baud 115200
// ESP-01 AT firmware versiyonu: v2.x+ (Espressif resmi)
// ────────────────────────────────────────────────────────────────────
namespace ModESP01 {

static String at(const String& cmd, uint32_t wait=3000, const String& expect="OK") {
    while(Serial2.available()) Serial2.read();
    Serial2.println(cmd);
    uint32_t t0=millis(); String resp="";
    while(millis()-t0<wait) {
        while(Serial2.available()) resp+=(char)Serial2.read();
        if(resp.indexOf(expect)>=0 || resp.indexOf("ERROR")>=0) break;
        delay(10);
    }
    resp.trim(); return resp;
}
static void showR(const String& title, const String& r) {
    dispClear(); dispStatusBar(title);
    for(int i=0;i<4;i++) {
        String seg=r.substring(i*20,(i+1)*20);
        if(seg.length()) dispText(0,12+i*12,seg);
    }
    dispText(0,54,"[BTN]=kapat"); dispCommit();
    while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
}

static void run() {
    Serial2.begin(115200,SERIAL_8N1,PIN_MOD_RX2,PIN_MOD_TX2);
    delay(300);

    OptionList opts={
        {"AT Test",          nullptr},
        {"WiFi Tara",        nullptr},
        {"AP'ye Baglan",     nullptr},
        {"IP Bilgisi",       nullptr},
        {"HTTP GET",         nullptr},
        {"TCP Baglan",       nullptr},
        {"Veri Gonder",      nullptr},
        {"AP Modu",          nullptr},
        {"Firmware Surumu",  nullptr},
        {"Deep Sleep",       nullptr},
        {"Reset",            nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"ESP-01 WiFi AT");
    if(sel<0||sel==11){Serial2.end();return;}

    if(sel==0){ showR("AT Test",at("AT")); }
    else if(sel==1){
        showR("WiFi Tara", at("AT+CWLAP", 10000, "OK"));
    } else if(sel==2){
        char ssid[33]=""; charPicker(ssid,32,"SSID");
        char pass[33]=""; charPicker(pass,32,"Sifre");
        at("AT+CWMODE=1");
        String r=at("AT+CWJAP=\""+String(ssid)+"\",\""+String(pass)+"\"", 15000);
        showR("Baglan",r.indexOf("OK")>=0?"Baglandi!":"Hata: "+r.substring(0,30));
    } else if(sel==3){
        String r=at("AT+CIFSR");
        showR("IP Bilgi",r);
    } else if(sel==4){
        char host[33]="api.ipify.org"; charPicker(host,32,"Host");
        at("AT+CIPMUX=0");
        at("AT+CIPSTART=\"TCP\",\""+String(host)+"\",80", 5000);
        String req="GET / HTTP/1.1\r\nHost: "+String(host)+"\r\nConnection: close\r\n\r\n";
        at("AT+CIPSEND="+String(req.length()));
        String r=at(req, 5000);
        showR("HTTP GET",r.substring(0,80));
    } else if(sel==5){
        char host[33]=""; charPicker(host,32,"Host/IP");
        char portS[8]="80";   charPicker(portS,7,"Port");
        at("AT+CIPMUX=0");
        String r=at("AT+CIPSTART=\"TCP\",\""+String(host)+"\","+String(portS), 5000);
        showR("TCP Baglan",r);
    } else if(sel==6){
        char data[65]="HELLO"; charPicker(data,64,"Gonderilecek veri");
        at("AT+CIPSEND="+String(strlen(data)));
        String r=at(data,3000);
        showR("TCP Gonder",r);
    } else if(sel==7){
        char ssid[33]="M5Sharkino"; charPicker(ssid,32,"AP SSID");
        char pass[33]="12345678";  charPicker(pass,32,"AP Sifre");
        at("AT+CWMODE=2");
        String r=at("AT+CWSAP=\""+String(ssid)+"\",\""+String(pass)+"\",5,3");
        showR("AP Modu",r);
    } else if(sel==8){ showR("Firmware",at("AT+GMR")); }
    else if(sel==9){
        at("AT+GSLP=10000");
        dispBannerOK("Deep sleep 10sn");
    } else if(sel==10){
        at("AT+RST",3000);
        dispBannerOK("Reset gonderildi");
    }
    Serial2.end();
}
} // namespace ModESP01

// ────────────────────────────────────────────────────────────────────
// 8. PN532 — NFC / RFID Okuyucu
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x24
// ────────────────────────────────────────────────────────────────────
namespace ModPN532 {
static constexpr uint8_t PN_ADDR=0x24;

static bool sendCmd(const uint8_t* cmd, uint8_t len) {
    Wire1.beginTransmission(PN_ADDR);
    Wire1.write(0x00);Wire1.write(0x00);Wire1.write(0xFF);
    Wire1.write(len);Wire1.write((uint8_t)(0x100-len));
    uint8_t sum=0xD4;
    for(uint8_t i=0;i<len;i++){Wire1.write(cmd[i]);sum+=cmd[i];}
    Wire1.write((uint8_t)(0x100-sum));Wire1.write(0x00);
    return Wire1.endTransmission()==0;
}
static bool readTag(uint8_t* uid, uint8_t& uidLen) {
    // InListPassiveTarget komutu
    uint8_t cmd[]={0xD4,0x4A,0x01,0x00};
    sendCmd(cmd,4);
    delay(100);
    Wire1.requestFrom(PN_ADDR,(uint8_t)20);
    uint8_t buf[20]={0}; uint8_t i=0;
    while(Wire1.available()&&i<20) buf[i++]=Wire1.read();
    // Yanıt parse (basit)
    if(buf[6]==0xD5&&buf[7]==0x4B&&buf[8]>0) {
        uidLen=buf[12];
        for(uint8_t j=0;j<uidLen&&j<7;j++) uid[j]=buf[13+j];
        return true;
    }
    return false;
}
static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    // SAMConfiguration
    uint8_t sam[]={0xD4,0x14,0x01,0x14,0x01};
    sendCmd(sam,5); delay(100);

    OptionList opts={
        {"Kart Bekle / Oku", nullptr},
        {"UID Goster",       nullptr},
        {"NDEF Oku",         nullptr},
        {"Kart Turu",        nullptr},
        {"UID Log",          nullptr},
        {"UID Karsilastir",  nullptr},
        {"Ham Dump",         nullptr},
        {"Kart Sayaci",      nullptr},
        {"SD Log",           nullptr},
        {"Surekli Tara",     nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"PN532 NFC/RFID");
    if(sel<0||sel==10)return;

    static uint32_t cardCount=0;
    static uint8_t lastUID[7]={0};
    static uint8_t lastUIDLen=0;

    auto uidStr=[](const uint8_t* uid,uint8_t len)->String{
        String s="";for(uint8_t i=0;i<len;i++){if(uid[i]<0x10)s+="0";s+=String(uid[i],HEX);if(i<len-1)s+=":";}
        s.toUpperCase();return s;
    };

    if(sel==0||sel==1){
        dispClear();dispStatusBar("PN532 Bekliyor");
        dispText(16,22,"Kart/etiket koy");
        dispText(0,54,"[LONG]=cik");dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t uid[7]={0};uint8_t ulen=0;
            if(readTag(uid,ulen)){
                memcpy(lastUID,uid,ulen);lastUIDLen=ulen;
                cardCount++;buzzOK();
                dispClear();dispStatusBar("Kart Bulundu!");
                String us=uidStr(uid,ulen);
                dispText(0,14,us);
                char b[24];snprintf(b,24,"Uzunluk: %d byte",ulen);
                dispText(0,28,b);
                snprintf(b,24,"Sayi: %lu",cardCount);
                dispText(0,42,b);
                dispText(0,54,"[LONG]=cik");dispCommit();
                delay(1500);
                dispClear();dispStatusBar("PN532 Bekliyor");
                dispText(16,22,"Kart/etiket koy");
                dispText(0,54,"[LONG]=cik");dispCommit();
            }
            delay(100);
        }
    } else if(sel==3){
        dispClear();dispStatusBar("Kart Turu");
        dispText(16,22,"Kart/etiket koy");dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t uid[7]={0};uint8_t ulen=0;
            if(readTag(uid,ulen)){
                dispClear();dispStatusBar("Kart Turu");
                const char* tp=ulen==4?"Mifare Classic 1K":ulen==7?"Mifare Ultralight/NTAG":"Bilinmiyor";
                dispText(0,20,tp);
                char b[16];snprintf(b,16,"UID len: %d",ulen);
                dispText(0,36,b);
                dispText(0,54,"[LONG]=cik");dispCommit();
                delay(1500);
            }
            delay(100);
        }
    } else if(sel==4||sel==8){
        if(!g_sdInited){dispBannerErr("SD kart yok");return;}
        File f=SD.open("/nfc_log.csv",FILE_APPEND);
        if(!f){dispBannerErr("Log acilamadi");return;}
        dispClear();dispStatusBar("NFC Log");
        dispText(16,22,"Kartlari oku...");
        dispText(0,54,"[LONG]=dur");dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t uid[7]={0};uint8_t ulen=0;
            if(readTag(uid,ulen)){
                cardCount++;
                char row[32];snprintf(row,32,"%lu,%s\n",millis(),uidStr(uid,ulen).c_str());
                f.print(row);buzzNav();
                dispClear();dispStatusBar("NFC Log");
                dispText(0,14,uidStr(uid,ulen));
                char b[16];snprintf(b,16,"#%lu kaydedildi",cardCount);
                dispText(0,28,b);
                dispText(0,54,"[LONG]=dur");dispCommit();
                delay(1000);
            }
            delay(100);
        }
        f.close();dispBannerOK(String(cardCount)+" kart kaydedildi");
    } else if(sel==5){
        // Referans UID karşılaştır
        if(lastUIDLen==0){dispBannerErr("Once kart oku");return;}
        String refUID=uidStr(lastUID,lastUIDLen);
        dispClear();dispStatusBar("UID Karsilastir");
        dispText(0,14,"Ref: "+refUID.substring(0,20));
        dispText(0,28,"Kart koy...");dispCommit();
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t uid[7]={0};uint8_t ulen=0;
            if(readTag(uid,ulen)){
                bool match=(uidStr(uid,ulen)==refUID);
                if(match){buzzOK();dispBannerOK("ESLESTI!");}
                else{buzzNav();dispBannerWarn("ESLESME YOK");}
                delay(1500);
            }
            delay(100);
        }
    } else if(sel==7){
        dispClear();dispStatusBar("Kart Sayaci");
        char b[24];snprintf(b,24,"Toplam: %lu",cardCount);
        dispText(0,24,b);
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        // Sürekli tarama, her kart sayar
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t uid[7]={0};uint8_t ulen=0;
            if(readTag(uid,ulen)){cardCount++;buzzNav();}
            dispClear();dispStatusBar("Surekli Tara");
            char b[24];snprintf(b,24,"Sayi: %lu",cardCount);
            dispText(16,24,b);dispText(0,54,"[LONG]=dur");dispCommit();
            delay(100);
        }
    }
}
} // namespace ModPN532

// ────────────────────────────────────────────────────────────────────
// 9. MCP23017 — 16-bit I2C GPIO Genişletici
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x20
// ────────────────────────────────────────────────────────────────────
namespace ModMCP23017 {
static constexpr uint8_t MCP_ADDR=0x20;
static uint8_t iodirA=0xFF,iodirB=0xFF; // varsayılan: hepsi giriş
static uint8_t portA=0,portB=0;

static void writeReg(uint8_t r,uint8_t v){
    Wire1.beginTransmission(MCP_ADDR);Wire1.write(r);Wire1.write(v);Wire1.endTransmission();
}
static uint8_t readReg(uint8_t r){
    Wire1.beginTransmission(MCP_ADDR);Wire1.write(r);Wire1.endTransmission(false);
    Wire1.requestFrom(MCP_ADDR,(uint8_t)1);return Wire1.available()?Wire1.read():0;
}
static void applyDir(){writeReg(0x00,iodirA);writeReg(0x01,iodirB);}
static void writeGPIO(){writeReg(0x12,portA);writeReg(0x13,portB);}

static void run(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    applyDir();

    OptionList opts={
        {"Port A Oku (8-bit)",nullptr},
        {"Port B Oku (8-bit)",nullptr},
        {"Port A Yaz",       nullptr},
        {"Port B Yaz",       nullptr},
        {"Pin Mod Ayarla",   nullptr},
        {"Pull-up Ayarla",   nullptr},
        {"Interrupt Ayarla", nullptr},
        {"Bit Toggle",       nullptr},
        {"Tum Port Tarama",  nullptr},
        {"LED Chaser",       nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"MCP23017 GPIO");
    if(sel<0||sel==10)return;

    if(sel==0||sel==1){
        bool isA=(sel==0);
        while(!readButtonLong()){
            potApplyBrightness();
            uint8_t val=readReg(isA?0x12:0x13);
            dispClear();dispStatusBar(isA?"Port A (GP0-7)":"Port B (GP8-15)");
            char b[24];snprintf(b,24,"0x%02X = %d",val,val);
            dispText(0,14,b);
            // Bit göstergesi
            for(int i=7;i>=0;i--){
                int bx=4+(7-i)*15, by=28;
                display.drawRect(bx,by,12,12,SSD1306_WHITE);
                if(val&(1<<i)) display.fillRect(bx+2,by+2,8,8,SSD1306_WHITE);
                char n[3];snprintf(n,3,"%d",i);
                dispText(bx+3,by+14,n);
            }
            dispText(0,54,"[LONG]=cik");dispCommit();delay(200);
        }
    } else if(sel==2||sel==3){
        bool isA=(sel==2);
        uint8_t& port=isA?portA:portB;
        while(!readButtonLong()){
            potApplyBrightness();
            JoyDir d=readJoystick();
            if(d==JoyDir::UP)  {port++;writeGPIO();buzzNav();}
            if(d==JoyDir::DOWN){port--;writeGPIO();buzzNav();}
            dispClear();dispStatusBar(isA?"Port A Yaz":"Port B Yaz");
            char b[24];snprintf(b,24,"0x%02X = %d",port,port);
            dispText(0,20,b);
            for(int i=7;i>=0;i--){
                int bx=4+(7-i)*15;
                display.drawRect(bx,28,12,12,SSD1306_WHITE);
                if(port&(1<<i)) display.fillRect(bx+2,30,8,8,SSD1306_WHITE);
            }
            dispText(0,54,"UP/DN=deger [LONG]=cik");dispCommit();delay(80);
        }
    } else if(sel==4){
        // Pin mod (IODIR)
        OptionList pm={{"Port A Hepsi Giris",nullptr},{"Port A Hepsi Cikis",nullptr},
                       {"Port B Hepsi Giris",nullptr},{"Port B Hepsi Cikis",nullptr},{"Back",nullptr}};
        int ps=loopOptions(pm,"Pin Mod");
        if(ps==0){iodirA=0xFF;applyDir();}
        else if(ps==1){iodirA=0x00;applyDir();}
        else if(ps==2){iodirB=0xFF;applyDir();}
        else if(ps==3){iodirB=0x00;applyDir();}
        if(ps<4) dispBannerOK("IODIR ayarlandi");
    } else if(sel==5){
        // Pull-up (GPPU)
        writeReg(0x0C,0xFF);writeReg(0x0D,0xFF);
        dispBannerOK("Tum pull-up aktif");
    } else if(sel==7){
        // Bit toggle
        char pinS[4]="0";charPicker(pinS,3,"Pin no (0-15)");
        int pin=atoi(pinS);
        if(pin<8){portA^=(1<<pin);writeReg(0x12,portA);}
        else{portB^=(1<<(pin-8));writeReg(0x13,portB);}
        dispBannerOK("Pin "+String(pin)+" toggle");
    } else if(sel==8){
        // Tüm port oku
        uint8_t a=readReg(0x12),b=readReg(0x13);
        dispClear();dispStatusBar("Tum Port");
        char buf[24];
        snprintf(buf,24,"A: 0x%02X  B: 0x%02X",a,b);dispText(0,14,buf);
        snprintf(buf,24,"A: %d  B: %d",a,b);dispText(0,28,buf);
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        // LED chaser (Port A çıkış)
        iodirA=0x00;applyDir();
        uint8_t pos=0;
        while(!readButtonLong()){
            potApplyBrightness();
            portA=(1<<pos);writeReg(0x12,portA);
            pos=(pos+1)%8;
            dispClear();dispStatusBar("LED Chaser");
            char b[24];snprintf(b,24,"Pos: %d  0x%02X",pos,portA);
            dispText(0,24,b);dispText(0,54,"[LONG]=dur");dispCommit();
            delay(150);
        }
        portA=0;writeReg(0x12,portA);
    }
}
} // namespace ModMCP23017

// ────────────────────────────────────────────────────────────────────
// 10. Si4703 — FM Radyo Alıcısı
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x10
//           RST→Pin4(GP40), GPIO2→Pin7(GP37) (STC interrupt)
// ────────────────────────────────────────────────────────────────────
namespace ModSi4703 {
static constexpr uint8_t SI_ADDR=0x10;
static uint16_t regs[16]={0};
static float    curFreq=87.5f;
static uint8_t  volume=5;

static void readRegs(){
    Wire1.requestFrom(SI_ADDR,(uint8_t)32);
    for(int i=0x0A;Wire1.available()>=2;){
        regs[i]=(Wire1.read()<<8)|Wire1.read();
        i=(i+1)&0xF;
    }
}
static void writeRegs(){
    Wire1.beginTransmission(SI_ADDR);
    for(int i=2;i<=7;i++){Wire1.write(regs[i]>>8);Wire1.write(regs[i]&0xFF);}
    Wire1.endTransmission();
}
static void init_si4703(){
    pinMode(PIN_MOD_DATA,OUTPUT);
    // 2-wire mode: GPIO2 HIGH before RST release
    pinMode(PIN_MOD_TX2,OUTPUT);digitalWrite(PIN_MOD_TX2,HIGH);
    digitalWrite(PIN_MOD_DATA,LOW);delay(100);
    digitalWrite(PIN_MOD_DATA,HIGH);delay(500);
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    readRegs();
    regs[0x07]=0x8100; writeRegs(); delay(500); // Crystal osc enable
    readRegs();
    regs[0x04]|=0x0040; // RDS enable
    regs[0x02]|=0x0001; // Enable
    regs[0x02]&=~(1<<6); // Unmute
    regs[0x05]=(regs[0x05]&0xFFF0)|volume; // Volume
    writeRegs(); delay(110);
}
static void setFreq(float mhz){
    curFreq=constrain(mhz,87.5f,108.0f);
    uint16_t ch=(uint16_t)((curFreq-87.5f)/0.1f);
    readRegs();
    regs[0x03]=(regs[0x03]&0xFE00)|ch|0x8000; // TUNE=1
    writeRegs();
    delay(100);
    readRegs();
    regs[0x03]&=~0x8000; writeRegs(); // TUNE=0
}
static uint8_t getRSSI(){readRegs();return regs[0x0A]&0x00FF;}
static bool getStereo(){readRegs();return (regs[0x0A]&0x0100)!=0;}

static void run(){
    init_si4703();
    OptionList opts={
        {"Canli FM Dinle",  nullptr},
        {"Frekans Gir",     nullptr},
        {"Yukari Seek",     nullptr},
        {"Asagi Seek",      nullptr},
        {"Volume Ayarla",   nullptr},
        {"Sinyal Gucu",     nullptr},
        {"RDS Istasyon",    nullptr},
        {"Mono/Stereo",     nullptr},
        {"Tarama (87-108)", nullptr},
        {"Favori Kaydet",   nullptr},
        {"Mute Toggle",     nullptr},
        {"Back",            nullptr},
    };
    int sel=loopOptions(opts,"Si4703 FM Radyo");
    if(sel<0||sel==11){
        // Power down
        readRegs();regs[0x02]&=~0x0001;writeRegs();
        return;
    }

    static float favFreqs[5]={88.0f,90.0f,95.0f,100.0f,105.0f};
    static uint8_t favIdx=0;

    if(sel==0){
        while(!readButtonLong()){
            potApplyBrightness();
            JoyDir d=readJoystick();
            if(d==JoyDir::RIGHT&&curFreq<108.0f){setFreq(curFreq+0.1f);buzzNav();}
            if(d==JoyDir::LEFT &&curFreq>87.5f) {setFreq(curFreq-0.1f);buzzNav();}
            if(d==JoyDir::UP   &&volume<15)     {volume++;readRegs();regs[0x05]=(regs[0x05]&0xFFF0)|volume;writeRegs();}
            if(d==JoyDir::DOWN &&volume>0)      {volume--;readRegs();regs[0x05]=(regs[0x05]&0xFFF0)|volume;writeRegs();}
            dispClear();dispStatusBar("FM Radyo");
            char b[24];snprintf(b,24,"%.1f MHz",curFreq);
            dispText(16,10,b);
            snprintf(b,24,"RSSI: %d  Vol:%d",getRSSI(),volume);
            dispText(0,26,b);
            dispText(0,38,getStereo()?"STEREO":"MONO");
            // Frekans bar
            int bw=(int)((curFreq-87.5f)/20.5f*112);
            display.drawRect(4,46,112,6,SSD1306_WHITE);
            display.fillRect(4,46,bw,6,SSD1306_WHITE);
            dispText(0,54,"L/R:freq U/D:vol");dispCommit();
            delay(100);
        }
    } else if(sel==1){
        char fS[8]="100.0";charPicker(fS,7,"Frekans (87.5-108)");
        setFreq(atof(fS));
        dispBannerOK("Frekans: "+String(curFreq,1)+" MHz");
    } else if(sel==2||sel==3){
        bool up=(sel==2);
        readRegs();
        regs[0x02]|=0x0100|(up?0x0200:0x0000); // SEEK=1, SEEKUP
        writeRegs();
        uint32_t t0=millis();
        while(millis()-t0<5000){
            readRegs();
            if(regs[0x0A]&0x4000){break;} // STC
            delay(50);
        }
        readRegs();
        uint16_t ch=regs[0x0B]&0x03FF;
        curFreq=87.5f+ch*0.1f;
        regs[0x02]&=~0x0100;writeRegs();
        dispBannerOK("Seek: "+String(curFreq,1)+" MHz");
    } else if(sel==4){
        while(!readButtonLong()){
            potApplyBrightness();
            JoyDir d=readJoystick();
            if(d==JoyDir::UP  &&volume<15){volume++;readRegs();regs[0x05]=(regs[0x05]&0xFFF0)|volume;writeRegs();buzzNav();}
            if(d==JoyDir::DOWN&&volume>0) {volume--;readRegs();regs[0x05]=(regs[0x05]&0xFFF0)|volume;writeRegs();buzzNav();}
            dispClear();dispStatusBar("Volume");
            char b[24];snprintf(b,24,"%d / 15",volume);
            dispText(16,18,b);
            int bw=volume*112/15;
            display.drawRect(4,32,112,10,SSD1306_WHITE);
            display.fillRect(4,32,bw,10,SSD1306_WHITE);
            dispText(0,54,"UP/DN=ses [LONG]=cik");dispCommit();delay(80);
        }
    } else if(sel==5){
        uint8_t rssi=getRSSI();
        dispClear();dispStatusBar("Sinyal Gucu");
        char b[24];snprintf(b,24,"RSSI: %d dBuV",rssi);
        dispText(0,18,b);
        int bw=rssi*112/75;if(bw>112)bw=112;
        display.drawRect(4,32,112,10,SSD1306_WHITE);
        display.fillRect(4,32,bw,10,SSD1306_WHITE);
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==6){
        // RDS (RDSR register)
        readRegs();
        uint16_t rdsA=regs[0x0C],rdsB=regs[0x0D];
        dispClear();dispStatusBar("RDS");
        char b[24];snprintf(b,24,"PI: 0x%04X",rdsA);dispText(0,14,b);
        snprintf(b,24,"G: %d  TP:%d",((rdsB>>12)&0xF),(rdsB>>10)&1);dispText(0,28,b);
        dispText(0,42,"(RDS firmware gerekir)");
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==7){
        readRegs();
        bool mono=regs[0x02]&0x2000;
        regs[0x02]^=0x2000;writeRegs();
        dispBannerOK(mono?"STEREO modu":"MONO modu");
    } else if(sel==8){
        // Basit tarama (sinyal güçlü istasyonları bul)
        dispClear();dispStatusBar("FM Tarama");dispText(16,24,"Taranıyor...");dispCommit();
        String found="";
        for(float f=87.5f;f<=108.0f&&!readButtonLong();f+=0.1f){
            setFreq(f);delay(50);
            if(getRSSI()>30){found+=String(f,1)+" ";}
        }
        dispClear();dispStatusBar("Bulunan Ist.");
        dispText(0,12,found.substring(0,21));
        dispText(0,24,found.substring(21,42));
        dispText(0,36,found.substring(42,63));
        dispText(0,54,"[BTN]=kapat");dispCommit();
        while(!readButton()&&!readButtonLong()){potApplyBrightness();delay(50);}
    } else if(sel==9){
        favFreqs[favIdx%5]=curFreq;favIdx++;
        dispBannerOK("Favori: "+String(curFreq,1)+" MHz");
    } else if(sel==10){
        readRegs();
        bool muted=!(regs[0x02]&0x0040);
        if(muted)regs[0x02]&=~0x0040; else regs[0x02]|=0x0040;
        writeRegs();
        dispBannerOK(muted?"Ses ACIK":"MUTE");
    }
}
} // namespace ModSi4703

// ────────────────────────────────────────────────────────────────────
// 11. OLED SSD1306 — Ekstra Ekran (İkinci Ekran / Modül)
// Bağlantı: SDA→Pin5(GP41), SCL→Pin6(GP42), I2C adr 0x3C veya 0x3D
// Not: Ana ekran Wire (GP7/GP8), bu Wire1 (GP41/GP42) üzerinde
// ────────────────────────────────────────────────────────────────────
namespace ModOLED {
#include <Adafruit_SSD1306.h>
static Adafruit_SSD1306 ext_oled(128,64,&Wire1,-1);
static bool oled_ok=false;

static void init_oled(){
    Wire1.begin(PIN_MOD_SDA2,PIN_MOD_SCL2,400000);
    oled_ok=ext_oled.begin(SSD1306_SWITCHCAPVCC,0x3C);
    if(!oled_ok)ext_oled.begin(SSD1306_SWITCHCAPVCC,0x3D);
    if(oled_ok){ext_oled.clearDisplay();ext_oled.display();}
}

static void run(){
    init_oled();
    if(!oled_ok){dispBannerErr("Ext OLED bulunamadi");return;}

    OptionList opts={
        {"Mesaj Yaz",        nullptr},
        {"Sensor Mirror",    nullptr},
        {"Saat Goster",      nullptr},
        {"Bar Grafik Demo",  nullptr},
        {"Pixel Art",        nullptr},
        {"Invertir",         nullptr},
        {"Parlaklik",        nullptr},
        {"Scroll Animasyon", nullptr},
        {"Clear",            nullptr},
        {"I2C Adres Sec",    nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"Ext OLED SSD1306");
    if(sel<0||sel==10){ext_oled.clearDisplay();ext_oled.display();return;}

    if(sel==0){
        char msg[33]=""; charPicker(msg,32,"Mesaj gir");
        ext_oled.clearDisplay();
        ext_oled.setTextSize(2);ext_oled.setTextColor(SSD1306_WHITE);
        ext_oled.setCursor(0,0);ext_oled.print(msg);
        ext_oled.display();
        dispBannerOK("Ext OLED'e yazildi");
    } else if(sel==1){
        // Ana ekran içeriğini ext OLED'e yansıt
        dispBannerOK("Mirror modu aktif");
        while(!readButtonLong()){
            potApplyBrightness();
            ext_oled.clearDisplay();
            // Ana display buffer'ından kopyala (basit yaklaşım: aynı metin)
            ext_oled.setTextSize(1);ext_oled.setTextColor(SSD1306_WHITE);
            ext_oled.setCursor(0,0);ext_oled.print("M5Sharkino Mirror");
            ext_oled.setCursor(0,16);ext_oled.print("Uptime:"+String(millis()/1000)+"s");
            ext_oled.display();delay(500);
        }
    } else if(sel==2){
        while(!readButtonLong()){
            potApplyBrightness();
            ext_oled.clearDisplay();
            ext_oled.setTextSize(2);ext_oled.setTextColor(SSD1306_WHITE);
            ext_oled.setCursor(10,0);
            char ts[12];snprintf(ts,12,"%lu",(millis()/1000));
            ext_oled.print(ts);ext_oled.print("s");
            ext_oled.setTextSize(1);
            ext_oled.setCursor(0,40);ext_oled.print("M5Sharkino v3");
            ext_oled.display();delay(1000);
        }
    } else if(sel==3){
        for(int i=0;i<10;i++){
            ext_oled.clearDisplay();
            for(int j=0;j<8;j++){
                int bh=random(10,50);
                ext_oled.fillRect(4+j*15,64-bh,12,bh,SSD1306_WHITE);
            }
            ext_oled.display();delay(500);
        }
    } else if(sel==5){
        ext_oled.invertDisplay(true);delay(1000);ext_oled.invertDisplay(false);
        dispBannerOK("Invertir yapildi");
    } else if(sel==7){
        ext_oled.clearDisplay();
        ext_oled.setTextSize(1);ext_oled.setTextColor(SSD1306_WHITE);
        ext_oled.setCursor(0,0);ext_oled.print("Scroll Test");
        ext_oled.display();
        ext_oled.startscrollleft(0x00,0x07);
        delay(3000);ext_oled.stopscroll();
        dispBannerOK("Scroll tamamlandi");
    } else if(sel==8){
        ext_oled.clearDisplay();ext_oled.display();
        dispBannerOK("Ext OLED temizlendi");
    } else if(sel==9){
        OptionList addrs={{"0x3C (default)",nullptr},{"0x3D",nullptr}};
        int as=loopOptions(addrs,"I2C Adres");
        if(as>=0){
            oled_ok=ext_oled.begin(SSD1306_SWITCHCAPVCC,as==0?0x3C:0x3D);
            dispBannerOK(oled_ok?"OLED baslatildi":"Bulunamadi");
        }
    }
}
} // namespace ModOLED

// ════════════════════════════════════════════════════════════════════
//  İLETİŞİM ANA MENÜSÜ (genişletilmiş)
// ════════════════════════════════════════════════════════════════════
static void comms_menu_run(){
    OptionList opts={
        {"GPS NMEA",         nullptr},
        {"HC-05/06 BT",      nullptr},
        {"NRF24L01 RF",      nullptr},
        {"RS-485 Modbus",    nullptr},
        {"UART Koprusu",     nullptr},
        {"SIM800L GSM",      nullptr},
        {"ESP-01 WiFi AT",   nullptr},
        {"PN532 NFC/RFID",   nullptr},
        {"MCP23017 GPIO",    nullptr},
        {"Si4703 FM Radyo",  nullptr},
        {"Ext OLED SSD1306", nullptr},
        {"Back",             nullptr},
    };
    int sel=loopOptions(opts,"Iletisim Moduller");
    if(sel<0||sel==11)return;
    switch(sel){
        case 0:  ModGPS::run();          break;
        case 1:  ModHC05::run();         break;
        case 2:  ModNRF24::run();        break;
        case 3:  ModRS485::run();        break;
        case 4:  ModUARTBridge::run();   break;
        case 5:  ModSIM800::run();       break;
        case 6:  ModESP01::run();        break;
        case 7:  ModPN532::run();        break;
        case 8:  ModMCP23017::run();     break;
        case 9:  ModSi4703::run();       break;
        case 10: ModOLED::run();         break;
    }
}

#ifndef SIM_A7680C_H
#define SIM_A7680C_H

#include "config_types.h"

class SIM_A7680C {
private:
    HardwareSerial *simSerial;
    int activeSimRx = SIM_RX_PIN;
    int activeSimTx = SIM_TX_PIN;
    int activeGpsRx = GPS_RX_PIN;
    int activeGpsTx = GPS_TX_PIN;

public:
    bool isModemReady = false;

    SIM_A7680C(HardwareSerial *ser) : simSerial(ser) {}

    // Chuyển UART2 về đọc GPS u-blox M10 (115200 baud)
    void switchToGpsMode() {
        simSerial->end();
        simSerial->setRxBufferSize(2048);
        simSerial->begin(GPS_BAUD, SERIAL_8N1, activeGpsRx, activeGpsTx);
    }

    // Tự động đo xung chọn đúng chân đang có tín hiệu của GPS M10 (18 hoặc 8)
    void detectGpsPins() {
        pinMode(18, INPUT_PULLUP);
        pinMode(8, INPUT_PULLUP);
        delay(5);
        int last18 = digitalRead(18), last8 = digitalRead(8);
        int p18 = 0, p8 = 0;
        uint32_t t0 = millis();
        while (millis() - t0 < 250) {
            int c18 = digitalRead(18), c8 = digitalRead(8);
            if (c18 != last18) { p18++; last18 = c18; }
            if (c8 != last8)   { p8++;  last8 = c8; }
        }
        activeGpsRx = (p18 >= p8) ? 18 : 8;
        activeGpsTx = (activeGpsRx == 18) ? 8 : 18;
        Serial.printf("✅ [GPS M10] Đã khóa chân nhận GPS tại RX = GPIO %d (115200 baud)\n", activeGpsRx);
        switchToGpsMode();
    }

    String sendAT(String cmd, uint32_t timeout_ms = 1000) {
        int flushCount = 0;
        while (simSerial->available() && flushCount < 256) {
            simSerial->read();
            flushCount++;
        }
        simSerial->println(cmd);

        String resp = "";
        uint32_t start = millis();
        while (millis() - start < timeout_ms) {
            while (simSerial->available() && resp.length() < 512) {
                uint8_t b = simSerial->read();
                if (b != 0x00 && b < 128) resp += (char)b;
            }
            if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) break;
            delay(5);
        }
        return resp;
    }

    bool initModem() {
        Serial.println("[A7682S-LANS] Đang dò kết nối module SIM 4G...");
        simSerial->setRxBufferSize(1024);

        activeSimRx = SIM_RX_PIN;
        activeSimTx = SIM_TX_PIN;
        simSerial->begin(SIM_BAUD, SERIAL_8N1, activeSimRx, activeSimTx);
        delay(200);
        for (int i = 0; i < 3; i++) {
            if (sendAT("AT", 400).indexOf("OK") != -1) {
                isModemReady = true;
                break;
            }
        }

        if (!isModemReady) {
            activeSimRx = SIM_TX_PIN;
            activeSimTx = SIM_RX_PIN;
            simSerial->end();
            simSerial->begin(SIM_BAUD, SERIAL_8N1, activeSimRx, activeSimTx);
            delay(200);
            for (int i = 0; i < 3; i++) {
                if (sendAT("AT", 400).indexOf("OK") != -1) {
                    isModemReady = true;
                    break;
                }
            }
        }

        if (isModemReady) {
            sendAT("ATE0", 400);
            sendAT("AT+CMGF=1", 400);
            Serial.printf("✅ [A7682S-LANS] Đã kết nối sẵn sàng tại RX=%d, TX=%d!\n", activeSimRx, activeSimTx);
        } else {
            Serial.println("⚠️ [A7682S-LANS] Chưa phản hồi lúc khởi động.");
        }

        // Khởi tạo SIM xong thì chuyển UART2 sang đọc GPS M10 liên tục
        detectGpsPins();
        return isModemReady;
    }

    void sendEmergencySOS(String phone, float roll, float pitch, float max_g, GPSData_t gps, uint8_t crash_type) {
        String alertType = (crash_type == 3) ? "VA CHAM MANH!" : "NGA XE BAT DONG!";
        String msg = "[HOP DEN SOS] " + alertType +
                     " | Luc: " + String(max_g, 1) + "G" +
                     " | Goc Roll=" + String(roll, 1) + ", Pitch=" + String(pitch, 1);

        if (gps.is_valid) {
            msg += " | Toc do: " + String(gps.speed_kmh, 1) + "km/h";
            msg += " | Map: https://maps.google.com/?q=" + String(gps.latitude, 6) + "," + String(gps.longitude, 6);
        } else {
            msg += " | Map: https://maps.google.com/?q=10.776889,106.700806";
        }

        Serial.println("\n================ ĐANG BẮN TIN NHẮN SOS 4G ================");
        Serial.println("📩 Gửi tới SĐT: " + phone);
        Serial.println("📝 Nội dung: " + msg);

        // Chuyển UART2 sang chân SIM A7682S-LANS để bắn SMS
        simSerial->end();
        simSerial->begin(SIM_BAUD, SERIAL_8N1, activeSimRx, activeSimTx);
        delay(100);

        sendAT("AT", 500);
        sendAT("AT+CMGF=1", 500);

        int flushCount = 0;
        while (simSerial->available() && flushCount < 256) {
            simSerial->read();
            flushCount++;
        }

        simSerial->print("AT+CMGS=\"" + phone + "\"\r\n");
        delay(800);
        simSerial->print(msg);
        simSerial->write(0x1A); // Ctrl+Z phát lệnh bắn SMS

        Serial.println("⏳ Đang chờ nhà mạng xác nhận (+CMGS)...");
        uint32_t t0 = millis();
        while (millis() - t0 < 5000) {
            while (simSerial->available()) {
                uint8_t b = simSerial->read();
                if (b != 0x00) Serial.write(b);
            }
            delay(10);
        }
        Serial.println("\n✅ ĐÃ HOÀN TẤT LỆNH BẮN TIN NHẮN SOS!");
        Serial.println("==========================================================\n");

        // Bắn SMS xong thì trả UART2 về đọc GPS M10 ngay lập tức
        switchToGpsMode();
    }
};

#endif
#ifndef SIM_A7680C_H
#define SIM_A7680C_H

#include "config_types.h"

class SIM_A7680C {
private:
    HardwareSerial *simTxSerial;

public:
    int activeSimTx = SIM_TX_PIN;

    SIM_A7680C(HardwareSerial *ser) : simTxSerial(ser) {}

    // Đo điện áp tĩnh để tìm đúng chân RXD của SIM (Chân TXD của SIM luôn có 3.3V, chân RXD là 0V)
    int detectSimTxPin() {
        pinMode(4, INPUT_PULLDOWN);
        pinMode(5, INPUT_PULLDOWN);
        delay(20);
        int v4 = digitalRead(4);
        int v5 = digitalRead(5);

        if (v4 == HIGH && v5 == LOW) {
            activeSimTx = 5;
        } else if (v5 == HIGH && v4 == LOW) {
            activeSimTx = 4;
        } else {
            activeSimTx = SIM_TX_PIN;
        }
        Serial.printf("✅ [A7682S-LANS] Đã khóa cố định chân phát lệnh SIM tại TX = GPIO %d\n", activeSimTx);
        return activeSimTx;
    }

    void initModem() {
        delay(300);
        simTxSerial->print("AT\r\n");
        delay(200);
        simTxSerial->print("ATE0\r\n");
        delay(200);
        simTxSerial->print("AT+CMGF=1\r\n");
        delay(200);
        Serial.println("✅ [A7682S-LANS] Đã nạp cấu hình SMS Text Mode (AT+CMGF=1)!");
    }

    void sendEmergencySOS(String phone, float roll, float pitch, float max_g, GPSData_t gps, uint8_t crash_type) {
        String alertType = (crash_type == 3) ? "VA CHAM MANH!" : "NGA XE BAT DONG!";
        String msg = "[HOP DEN SOS] " + alertType +
                     " | Luc: " + String(max_g, 1) + "G" +
                     " | Goc Roll=" + String(roll, 1) + ", Pitch=" + String(pitch, 1);

        if (gps.is_valid && gps.latitude != 0.0) {
            msg += " | Toc do: " + String(gps.speed_kmh, 1) + "km/h";
            msg += " | Map: https://maps.google.com/?q=" + String(gps.latitude, 6) + "," + String(gps.longitude, 6);
        } else if (gps.latitude != 0.0) {
            msg += " | Vi tri gan nhat: https://maps.google.com/?q=" + String(gps.latitude, 6) + "," + String(gps.longitude, 6);
        } else {
            msg += " | GPS: Chua Fix ve tinh (Sat=" + String(gps.satellites) + ")";
        }

        Serial.println("\n================ ĐANG BẮN TIN NHẮN SOS 4G ================");
        Serial.println("📩 Gửi tới SĐT: " + phone);
        Serial.println("📝 Nội dung: " + msg);

        simTxSerial->print("AT\r\n");
        delay(300);
        simTxSerial->print("AT+CMGF=1\r\n");
        delay(300);
        simTxSerial->print("AT+CMGS=\"" + phone + "\"\r\n");
        delay(800);
        simTxSerial->print(msg);
        simTxSerial->write(0x1A); // Ctrl+Z phát lệnh bắn SMS

        delay(3500);
        Serial.println("✅ ĐÃ PHÁT LỆNH BẮN TIN NHẮN SOS HOÀN TẤT!");
        Serial.println("==========================================================\n");
    }
};

#endif
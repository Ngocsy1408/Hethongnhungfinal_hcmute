#ifndef SIM_A7680C_H
#define SIM_A7680C_H

#include "config_types.h"

class SIM_A7680C {
private:
    HardwareSerial *simSerial;

public:
    bool isModemReady = false;

    SIM_A7680C(HardwareSerial *ser) : simSerial(ser) {}

    String sendAT(String cmd, uint32_t timeout_ms = 800) {
        while (simSerial->available()) simSerial->read();
        simSerial->println(cmd);
        String resp = "";
        uint32_t start = millis();
        while (millis() - start < timeout_ms) {
            while (simSerial->available()) {
                resp += (char)simSerial->read();
            }
            if (resp.indexOf("OK\r\n") != -1 || resp.indexOf("ERROR") != -1) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        return resp;
    }

    bool initModem() {
        Serial.println("[SIM A7680C] Đang kiểm tra kết nối AT...");
        for (int i = 0; i < 3; i++) {
            String r = sendAT("AT", 500);
            if (r.indexOf("OK") != -1) {
                sendAT("ATE0", 500);
                sendAT("AT+CMGF=1", 500);
                isModemReady = true;
                Serial.println("[SIM A7680C] Đã kết nối sẵn sàng!");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        isModemReady = false;
        Serial.println("[SIM A7680C] Chưa cắm module SIM (Hệ thống vẫn chạy bình thường).");
        return false;
    }

    void sendEmergencySOS(String phone, float roll, float pitch, float max_g, GPSData_t gps, uint8_t crash_type) {
        String alertType = (crash_type == 3) ? "VA CHAM MANH!" : "NGA XE BAT DONG!";
        String msg = "[HOP DEN SOS] " + alertType + "\n";
        msg += "SDT Nhan: " + phone + "\n";
        msg += "Luc va cham: " + String(max_g, 1) + "G\n";
        msg += "Goc nghieng: Roll=" + String(roll, 1) + ", Pitch=" + String(pitch, 1) + "\n";

        if (gps.is_valid) {
            msg += "Toc do: " + String(gps.speed_kmh, 1) + " km/h\n";
            msg += "Map: https://maps.google.com/?q=" + String(gps.latitude, 6) + "," + String(gps.longitude, 6);
        } else {
            msg += "GPS: Chua bat duoc ve tinh";
        }

        Serial.println("\n================ PHÁT LỆNH KHẨN CẤP SOS ================");
        Serial.println(msg);
        Serial.println("========================================================\n");

        if (!isModemReady) return; // Nếu đang tháo SIM thì chỉ in ra màn hình

        simSerial->print("AT+CMGS=\"");
        simSerial->print(phone);
        simSerial->println("\"");
        vTaskDelay(pdMS_TO_TICKS(500));
        simSerial->print(msg);
        simSerial->write(0x1A); // Ctrl+Z gửi SMS
        vTaskDelay(pdMS_TO_TICKS(3000));

        // Gọi điện báo động tới số điện thoại
        simSerial->print("ATD");
        simSerial->print(phone);
        simSerial->println(";");
    }
};

#endif
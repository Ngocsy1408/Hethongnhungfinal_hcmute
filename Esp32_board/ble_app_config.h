#ifndef BLE_APP_CONFIG_H
#define BLE_APP_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "w25q128_flash.h"

// Chuẩn UUID Nordic UART Service (Tương thích mọi App Bluetooth Terminal trên Android/iOS)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern String            sosPhoneNumber;
extern Preferences       nvsPrefs;
extern W25Q128_Flash     flashMemory;
extern volatile TelemetryPacket_t latestIMU;
extern volatile GPSData_t         latestGPS;
extern volatile bool              sosTriggered;

BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;

// Gửi phản hồi chữ về màn hình App điện thoại qua Bluetooth
void BLE_SendToPhone(String text) {
    if (deviceConnected && pTxCharacteristic != NULL) {
        pTxCharacteristic->setValue(text.c_str());
        pTxCharacteristic->notify();
    }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLUETOOTH] Điện thoại đã kết nối vào Hộp Đen!");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLUETOOTH] Điện thoại đã ngắt kết nối. Đang phát sóng lại...");
        pServer->startAdvertising(); // Phát sóng lại để điện thoại kết nối lần sau
    }
};

class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String cmd = String(pCharacteristic->getValue().c_str());
        cmd.trim(); // Xóa dấu xuống dòng thừa
        if (cmd.length() == 0) return;

        Serial.println("[BLUETOOTH NHẬN LỆNH]: " + cmd);

        // 1. LỆNH ĐỔI SỐ ĐIỆN THOẠI: Cú pháp "SDT:0912345678" hoặc "SDT:+84912345678"
        if (cmd.startsWith("SDT:") || cmd.startsWith("sdt:")) {
            String newPhone = cmd.substring(4);
            newPhone.trim();
            if (newPhone.length() >= 9 && newPhone.length() <= 15) {
                sosPhoneNumber = newPhone;
                // Lưu vĩnh viễn vào bộ nhớ Flash NVS (Tắt máy rút điện không mất)
                nvsPrefs.begin("blackbox", false);
                nvsPrefs.putString("sos_phone", sosPhoneNumber);
                nvsPrefs.end();

                String reply = "✅ Da luu SDT SOS moi: " + sosPhoneNumber + "\n";
                Serial.print(reply);
                BLE_SendToPhone(reply);
            } else {
                BLE_SendToPhone("❌ Loi: SDT khong hop le! VD: SDT:0912345678\n");
            }
        }
        // 2. LỆNH XEM THÔNG TIN HIỆN TẠI: Gửi chữ "INFO"
        else if (cmd.equalsIgnoreCase("INFO")) {
            String info = "--- TRANG THAI HOP DEN ---\n";
            info += "SDT SOS: " + sosPhoneNumber + "\n";
            info += "Roll: " + String(latestIMU.roll, 1) + " | Pitch: " + String(latestIMU.pitch, 1) + "\n";
            info += "GPS Fix: " + String(latestGPS.is_valid ? "OK" : "Chua Fix") + " (" + String(latestGPS.satellites) + " sat)\n";
            info += "Toc do: " + String(latestGPS.speed_kmh, 1) + " km/h\n";
            BLE_SendToPhone(info);
        }
        // 3. LỆNH ĐỌC LỊCH SỬ TAI NẠN TỪ W25Q128: Gửi chữ "LOG"
        else if (cmd.equalsIgnoreCase("LOG")) {
            String logStr = flashMemory.getCrashHistorySummary();
            Serial.print(logStr);
            BLE_SendToPhone(logStr);
        }
        // 4. LỆNH XÓA TRẮNG HỘP ĐEN W25Q128: Gửi chữ "CLEAR"
        else if (cmd.equalsIgnoreCase("CLEAR")) {
            flashMemory.clearAllLogs();
            BLE_SendToPhone("✅ Da xoa trang lich su hop den W25Q128!\n");
        }
        // 5. LỆNH TEST KHẨN CẤP SOS: Gửi chữ "TEST_SOS"
        else if (cmd.equalsIgnoreCase("TEST_SOS")) {
            sosTriggered = true;
            BLE_SendToPhone("🚨 Dang kich hoat thu nghiem SOS!\n");
        }
        else {
            BLE_SendToPhone("Lenh ho tro:\n- SDT:09xxxxxxxx\n- INFO\n- LOG\n- CLEAR\n- TEST_SOS\n");
        }
    }
};

void BLE_Init_Server() {
    // Đọc số điện thoại đã lưu trong bộ nhớ NVS lúc bật máy
    nvsPrefs.begin("blackbox", true);
    sosPhoneNumber = nvsPrefs.getString("sos_phone", "0912345678");
    nvsPrefs.end();
    Serial.println("[NVS FLASH] Số điện thoại SOS hiện tại: " + sosPhoneNumber);

    BLEDevice::init("HOP_DEN_XE_MAY"); // Tên Bluetooth hiện trên điện thoại
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Kênh TX gửi thông báo từ ESP32 về Điện thoại
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // Kênh RX nhận lệnh từ Điện thoại gửi xuống ESP32
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyRxCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[BLUETOOTH] Đã phát sóng BLE tên: 'HOP_DEN_XE_MAY'");
}

#endif
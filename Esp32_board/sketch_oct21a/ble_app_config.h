#ifndef BLE_APP_CONFIG_H
#define BLE_APP_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "w25q128_flash.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern String                     sosPhoneNumber;
extern Preferences                nvsPrefs;
extern W25Q128_Flash              flashMemory;
extern volatile TelemetryPacket_t latestIMU;
extern volatile GPSData_t         latestGPS;
extern volatile bool              sosTriggered;
extern volatile bool              sosHandled;
extern bool                       isZeroCalibrated;

BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;

void BLE_SendToPhone(String text) {
    if (deviceConnected && pTxCharacteristic != NULL) {
        pTxCharacteristic->setValue(text.c_str());
        pTxCharacteristic->notify();
    }
}

void Process_Command_Input(String rawCmd) {
    rawCmd.trim();
    if (rawCmd.length() == 0) return;
    Serial.println("\n📥 [NHẬN LỆNH ĐIỀU KHIỂN]: " + rawCmd);

    String cmdUpper = rawCmd;
    cmdUpper.toUpperCase();

    // 1. Đổi SĐT: SDT:0889602028
    int sdtPos = cmdUpper.indexOf("SDT:");
    if (sdtPos != -1) {
        String rawPhone = rawCmd.substring(sdtPos + 4);
        String cleanPhone = "";
        for (unsigned int i = 0; i < rawPhone.length(); i++) {
            char c = rawPhone.charAt(i);
            if ((c >= '0' && c <= '9') || c == '+') cleanPhone += c;
        }
        if (cleanPhone.length() >= 9 && cleanPhone.length() <= 15) {
            sosPhoneNumber = cleanPhone;
            nvsPrefs.begin("blackbox", false);
            nvsPrefs.putString("sos_phone", sosPhoneNumber);
            nvsPrefs.end();
            String reply = "✅ ĐÃ CẬP NHẬT SĐT SOS MỚI: " + sosPhoneNumber + "\n";
            Serial.print(reply);
            BLE_SendToPhone(reply);
        } else {
            Serial.println("❌ Lỗi: SĐT không hợp lệ!");
        }
        return;
    }

    // 2. Lấy lại gốc 0.00 độ tại vị trí đặt mạch hiện tại: ZERO
    if (cmdUpper.indexOf("ZERO") != -1) {
        isZeroCalibrated = false;
        Serial.println("✅ Đã đặt lại góc hiện tại làm mốc 0.00°!");
        BLE_SendToPhone("✅ Da dat lai goc 0.00 do!\n");
        return;
    }

    // 3. Xem trạng thái: INFO
    if (cmdUpper.indexOf("INFO") != -1) {
        String info = "\n--- TRANG THAI HOP DEN ---\nSDT SOS: " + sosPhoneNumber +
                      "\nRoll: " + String(latestIMU.roll, 1) + " | Pitch: " + String(latestIMU.pitch, 1) + "\n";
        Serial.print(info);
        BLE_SendToPhone(info);
        return;
    }

    // 4. Đọc lịch sử 15s từ W25Q128: LOG
    if (cmdUpper.indexOf("LOG") != -1) {
        String logStr = flashMemory.getCrashHistorySummary();
        Serial.print(logStr);
        BLE_SendToPhone(logStr);
        return;
    }

    // 5. Xóa lịch sử W25Q128: CLEAR
    if (cmdUpper.indexOf("CLEAR") != -1) {
        flashMemory.clearAllLogs();
        BLE_SendToPhone("✅ Da xoa trang lich su hop den!\n");
        return;
    }

    // 6. Kích hoạt test SOS (Chấp nhận cả "test sos", "TEST_SOS", "sos")
    if (cmdUpper.indexOf("TEST") != -1 || cmdUpper.indexOf("SOS") != -1) {
        sosHandled   = false;
        sosTriggered = true;
        Serial.println("🚨 Đang kích hoạt gửi tin nhắn SOS khẩn cấp tới: " + sosPhoneNumber);
        BLE_SendToPhone("🚨 Dang kich hoat thu nghiem SOS!\n");
        return;
    }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; pServer->startAdvertising(); }
};

class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        Process_Command_Input(String(pCharacteristic->getValue().c_str()));
    }
};

void BLE_Init_Server() {
    nvsPrefs.begin("blackbox", true);
    sosPhoneNumber = nvsPrefs.getString("sos_phone", "0889602028");
    nvsPrefs.end();

    BLEDevice::init("HOP_DEN_XE_MAY");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pTxCharacteristic->addDescriptor(new BLE2902());
    pTxCharacteristic->setCallbacks(new MyRxCallbacks());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyRxCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("✅ [BLUETOOTH] Đã phát sóng BLE: 'HOP_DEN_XE_MAY'");
}

#endif
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
volatile bool      deviceConnected   = false;
SemaphoreHandle_t  bleTxMutex        = NULL;

// Bộ đệm nhận lệnh an toàn (Tránh dùng String trong ngắt BLE gây Reboot)
static char        bleRxCmdBuf[64];
volatile bool      hasPendingBleCmd  = false;

/* ============================================================================
 * HÀM GỬI DỮ LIỆU QUA BLE (CHIA NHỎ GÓI 20-BYTE & CHỐNG XUNG ĐỘT)
 * ============================================================================ */
void BLE_SendToPhone(String text) {
    if (!deviceConnected || pTxCharacteristic == NULL || text.length() == 0) {
        return;
    }

    if (bleTxMutex != NULL) {
        if (xSemaphoreTake(bleTxMutex, pdMS_TO_TICKS(2000)) != pdTRUE) return;
    }

    const char *raw = text.c_str();
    size_t totalLen = text.length();
    const size_t CHUNK_SIZE = 20;

    for (size_t offset = 0; offset < totalLen; offset += CHUNK_SIZE) {
        if (!deviceConnected) break;

        size_t chunkLen = (totalLen - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalLen - offset);
        pTxCharacteristic->setValue((uint8_t*)(raw + offset), chunkLen);
        pTxCharacteristic->notify();

        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (bleTxMutex != NULL) {
        xSemaphoreGive(bleTxMutex);
    }
}

/* ============================================================================
 * XỬ LÝ TẬP LỆNH TỪ WEB APP HOẶC SERIAL MONITOR
 * ============================================================================ */
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
            String err = "❌ Lỗi: SĐT không hợp lệ!\n";
            Serial.print(err);
            BLE_SendToPhone(err);
        }
        return;
    }

    // 2. Lấy lại gốc 0.00 độ: ZERO
    if (cmdUpper.indexOf("ZERO") != -1) {
        isZeroCalibrated = false;
        Serial.println("✅ Đã đặt lại góc hiện tại làm mốc 0.00°!");
        BLE_SendToPhone("✅ Da dat lai goc 0.00 do!\n");
        return;
    }

    // 3. Xem trạng thái: INFO
    if (cmdUpper.indexOf("INFO") != -1) {
        char infoBuf[256];
        snprintf(infoBuf, sizeof(infoBuf),
                 "\n--- TRANG THAI HOP DEN ---\n"
                 "SDT SOS: %s\n"
                 "Roll: %.1f | Pitch: %.1f\n"
                 "GPS: Fix=%d Sat=%d Speed=%.1fkm/h\n"
                 "Vi tri: %.6f,%.6f\n"
                 "Trang thai: %d\n",
                 sosPhoneNumber.c_str(),
                 latestIMU.roll, latestIMU.pitch,
                 latestGPS.is_valid ? 1 : 0, latestGPS.satellites, latestGPS.speed_kmh,
                 latestGPS.latitude, latestGPS.longitude,
                 latestIMU.crash_state);
        Serial.print(infoBuf);
        BLE_SendToPhone(String(infoBuf));
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

    // 6. Kích hoạt test SOS
    if (cmdUpper.indexOf("TEST") != -1 || cmdUpper.indexOf("SOS") != -1) {
        sosHandled   = false;
        sosTriggered = true;
        Serial.println("🚨 Đang kích hoạt gửi tin nhắn SOS khẩn cấp tới: " + sosPhoneNumber);
        BLE_SendToPhone("🚨 Dang kich hoat thu nghiem SOS!\n");
        return;
    }
}

/* ============================================================================
 * TASK RIÊNG XỬ LÝ LỆNH BLE (CHỐNG TRÀN STACK & REBOOT BTC_TASK)
 * ============================================================================ */
void Task_BLE_Command_Worker(void *pvParameters) {
    for (;;) {
        if (hasPendingBleCmd) {
            char localBuf[64];
            strncpy(localBuf, (const char*)bleRxCmdBuf, sizeof(localBuf) - 1);
            localBuf[sizeof(localBuf) - 1] = '\0';
            hasPendingBleCmd = false;

            Process_Command_Input(String(localBuf));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ============================================================================
 * CALLBACKS SỰ KIỆN KẾT NỐI VÀ NHẬN DỮ LIỆU BLE
 * ============================================================================ */
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("📲 [BLUETOOTH] Đã kết nối với Web App!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("🔌 [BLUETOOTH] Đã ngắt kết nối, phát sóng lại...");
        pServer->startAdvertising();
    }
};

class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // CHỈ COPY MẢNG CHAR NHẸ NHÀNG, KHÔNG XỬ LÝ NẶNG TRONG CALLBACK NÀY!
        std::string val = pCharacteristic->getValue();
        if (val.length() > 0) {
            size_t copyLen = (val.length() < sizeof(bleRxCmdBuf) - 1) ? val.length() : (sizeof(bleRxCmdBuf) - 1);
            memcpy(bleRxCmdBuf, val.data(), copyLen);
            bleRxCmdBuf[copyLen] = '\0';
            hasPendingBleCmd = true;
        }
    }
};

/* ============================================================================
 * KHỞI TẠO SERVER BLE NORDIC UART (NUS)
 * ============================================================================ */
void BLE_Init_Server() {
    if (bleTxMutex == NULL) {
        bleTxMutex = xSemaphoreCreateMutex();
    }

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

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    // Khởi tạo Task riêng với Stack 8192 bytes chuyên xử lý lệnh BLE
    xTaskCreatePinnedToCore(Task_BLE_Command_Worker, "BleCmdWorker", 8192, NULL, 1, NULL, 1);

    Serial.println("✅ [BLUETOOTH] Đã phát sóng BLE: 'HOP_DEN_XE_MAY'");
}

#endif
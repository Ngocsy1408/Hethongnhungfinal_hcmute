#include <Arduino.h>
#include <Preferences.h>
#include "config_types.h"
#include "w25q128_flash.h"
#include "gps_m10_parser.h"
#include "sim_a7680c.h"
#include "ble_app_config.h"

/* 3 Cổng Hardware UART độc lập trên ESP32-S3 */
HardwareSerial STM32_UART(1); // GPIO 17 (RX), GPIO 16 (TX)
HardwareSerial GPS_UART(2);   // GPIO 18 (RX), GPIO 8 (TX)
HardwareSerial SIM_UART(0);   // GPIO 4 (RX),  GPIO 5 (TX)

W25Q128_Flash flashMemory;
GPS_M10       gpsParser;
SIM_A7680C    simModem(&SIM_UART);
Preferences   nvsPrefs;

/* Biến toàn cục hệ thống */
String                     sosPhoneNumber = "0912345678";
volatile TelemetryPacket_t latestIMU;
volatile GPSData_t         latestGPS      = {false, 0.0, 0.0, 0.0f, 0};
volatile bool              sosTriggered   = false;
volatile float             peakImpactG    = 1.0f;
volatile float             secMaxG        = 1.0f; // Lực G lớn nhất trong mỗi giây

/* BỘ ĐỆM HỘP ĐEN MÁY BAY (10 giây trước + 1 giây lúc đâm + 5 giây sau = 16 mốc) */
BlackboxLogRecord_t preCrashRing[10];  // Lưu xoay vòng 10 giây gần nhất
uint8_t             preRingIdx = 0;    // Vị trí ghi hiện tại trong vòng đệm 10s
uint8_t             preRingCount = 0;  // Số giây đã tích lũy được (tối đa 10)
BlackboxLogRecord_t fullCrashWindow[BLACKBOX_WINDOW_LEN]; // Băng 16 mốc hoàn chỉnh để ghi vào W25Q128

uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

const char* getCrashStateText(uint8_t state) {
    switch (state) {
        case 0:  return "BINH THUONG";
        case 1:  return "PHANH GAP!";
        case 2:  return "⚠️ NGA XE!";
        case 3:  return "💥 VA CHAM!";
        case 99: return "LOI CAM BIEN IMU!";
        default: return "UNKNOWN";
    }
}

// Hàm tạo 1 mẫu Snapshot tại thời điểm hiện tại
BlackboxLogRecord_t takeCurrentSnapshot(int8_t relSec, float g_val) {
    BlackboxLogRecord_t rec;
    rec.magic       = LOG_RECORD_MAGIC;
    rec.rel_sec     = relSec;
    rec.crash_state = latestIMU.crash_state;
    rec.roll        = latestIMU.roll;
    rec.pitch       = latestIMU.pitch;
    rec.max_g       = g_val;
    rec.latitude    = (float)latestGPS.latitude;
    rec.longitude   = (float)latestGPS.longitude;
    rec.speed_kmh   = latestGPS.speed_kmh;
    return rec;
}

/* ============================================================================
 * TASK 1 (CORE 1): Đọc gói tin 100Hz từ STM32F401 & Giải mã GPS u-blox M10
 * ============================================================================ */
void Task_Sensor_And_GPS(void *pvParameters) {
    uint8_t rxBuf[sizeof(TelemetryPacket_t)];
    uint16_t rxIdx = 0;
    TelemetryPacket_t tempPkt;

    for (;;) {
        while (STM32_UART.available()) {
            uint8_t b = STM32_UART.read();
            if (rxIdx == 0 && b != SYNC_BYTE_1) continue;
            if (rxIdx == 1 && b != SYNC_BYTE_2) { rxIdx = 0; continue; }

            rxBuf[rxIdx++] = b;

            if (rxIdx == sizeof(TelemetryPacket_t)) {
                rxIdx = 0;
                uint16_t calc_crc = Calculate_CRC16(rxBuf, sizeof(TelemetryPacket_t) - 2);
                memcpy(&tempPkt, rxBuf, sizeof(TelemetryPacket_t));

                if (calc_crc == tempPkt.crc16) {
                    memcpy((void*)&latestIMU, &tempPkt, sizeof(TelemetryPacket_t));

                    float g_total = sqrtf(tempPkt.accel[0]*tempPkt.accel[0] +
                                          tempPkt.accel[1]*tempPkt.accel[1] +
                                          tempPkt.accel[2]*tempPkt.accel[2]);
                    if (g_total > peakImpactG) peakImpactG = g_total;
                    if (g_total > secMaxG)     secMaxG     = g_total;

                    if ((tempPkt.crash_state == 2 || tempPkt.crash_state == 3) && !sosTriggered) {
                        sosTriggered = true;
                    }
                }
            }
        }

        while (GPS_UART.available()) {
            gpsParser.feed((char)GPS_UART.read(), &latestGPS);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* ============================================================================
 * TASK 2 (CORE 0): Quản lý Bộ đệm 10s Trước + 5s Sau Tai nạn, W25Q128 & SIM 4G
 * ============================================================================ */
void Task_Blackbox_Manager(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    simModem.initModem();

    uint32_t lastPrintMs   = 0;
    uint32_t lastOneSecMs  = millis();
    bool     sosHandled    = false;
    bool     recordingPost = false;
    uint8_t  postSecCount  = 0;

    for (;;) {
        uint32_t now = millis();

        // 1. NHỊP ĐỒNG HỒ MỖI 1 GIÂY (Cập nhật Bộ đệm 10s trước hoặc Thu 5s sau tai nạn)
        if (now - lastOneSecMs >= 1000) {
            lastOneSecMs = now;
            float g_this_sec = secMaxG;
            secMaxG = 1.0f; // Reset đỉnh G cho giây tiếp theo

            if (!recordingPost) {
                // Đang chạy bình thường: Đẩy vào Bộ đệm vòng 10 giây quá khứ
                preCrashRing[preRingIdx] = takeCurrentSnapshot(0, g_this_sec);
                preRingIdx = (preRingIdx + 1) % 10;
                if (preRingCount < 10) preRingCount++;
            } else {
                // Đang trong 5 giây SAU tai nạn (T+1s đến T+5s)
                postSecCount++;
                fullCrashWindow[10 + postSecCount] = takeCurrentSnapshot((int8_t)postSecCount, g_this_sec);
                Serial.printf("⏳ [HỘP ĐEN] Đang thu diễn biến SAU tai nạn: Giây T+%ds / +5s...\n", postSecCount);

                if (postSecCount >= 5) {
                    // Đã thu đủ 10s trước + 1s lúc đâm + 5s sau = 16 mốc! Ghi xuống W25Q128!
                    recordingPost = false;
                    flashMemory.save15SecCrashWindow(fullCrashWindow);
                    Serial.print(flashMemory.getCrashHistorySummary());
                    BLE_SendToPhone("💾 Da luu xong 15s Hop Den vao W25Q128! Go 'LOG' de xem.\n");
                }
            }
        }

        // 2. KHOẢNH KHẮC PHÁT HIỆN TAI NẠN (T = 0s)
        if (sosTriggered && !sosHandled) {
            sosHandled    = true;
            recordingPost = true;
            postSecCount  = 0;

            Serial.println("\n🚨 [PHÁT HIỆN TAI NẠN @ T=0s] Đóng băng 10s quá khứ & bắt đầu thu tiếp 5s sau va chạm!");

            // A. Sao chép 10 giây quá khứ từ Bộ đệm vòng ra mảng fullCrashWindow[0..9] (T-10s đến T-1s)
            for (int i = 0; i < 10; i++) {
                int ringPos = (preRingIdx + i) % 10;
                fullCrashWindow[i] = preCrashRing[ringPos];
                fullCrashWindow[i].rel_sec = (int8_t)(i - 10); // Gán nhãn -10s, -9s, ..., -1s
            }

            // B. Ghi nhận mốc T = 0s (Đúng khoảnh khắc xảy ra tai nạn) vào vị trí số 10
            fullCrashWindow[10] = takeCurrentSnapshot(0, peakImpactG);
            if (fullCrashWindow[10].crash_state < 2) fullCrashWindow[10].crash_state = 2;

            // C. Bắn tin nhắn SMS SOS & Gọi điện ngay lập tức
            GPSData_t gpsSnap;
            memcpy(&gpsSnap, (const void*)&latestGPS, sizeof(GPSData_t));
            BLE_SendToPhone("🚨 TAI NAN! Dang gui SMS SOS & thu 5s sau va cham...\n");

            simModem.sendEmergencySOS(
                sosPhoneNumber,
                latestIMU.roll,
                latestIMU.pitch,
                peakImpactG,
                gpsSnap,
                fullCrashWindow[10].crash_state
            );
        }

        // Reset cờ khi xe đã được dựng thẳng lại VÀ đã ghi xong 5 giây sau tai nạn
        if (latestIMU.crash_state == 0 && sosHandled && !recordingPost) {
            sosTriggered = false;
            sosHandled   = false;
            peakImpactG  = 1.0f;
        }

        // 3. In thông số trực tiếp mỗi 200ms lên Serial Monitor
        if (now - lastPrintMs >= 200) {
            lastPrintMs = now;
            Serial.printf("[#%6lu] Roll:%6.2f° | Pitch:%6.2f° | %s || [GPS] Fix:%d Sat:%2d (%4.1fkm/h) || SDT:%s\n",
                          latestIMU.seq_num,
                          latestIMU.roll,
                          latestIMU.pitch,
                          getCrashStateText(latestIMU.crash_state),
                          latestGPS.is_valid,
                          latestGPS.satellites,
                          latestGPS.speed_kmh,
                          sosPhoneNumber.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("   HỘP ĐEN XE MÁY (CHẾ ĐỘ 10s TRƯỚC + 5s SAU TAI NẠN)  ");
    Serial.println("=======================================================");

    STM32_UART.setRxBufferSize(2048);
    STM32_UART.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

    GPS_UART.setRxBufferSize(2048);
    GPS_UART.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    SIM_UART.setRxBufferSize(2048);
    SIM_UART.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

    flashMemory.begin(W25Q_CS_PIN, W25Q_SCK_PIN, W25Q_MISO_PIN, W25Q_MOSI_PIN);
    BLE_Init_Server();

    xTaskCreatePinnedToCore(Task_Sensor_And_GPS, "SensorGPS", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(Task_Blackbox_Manager, "BlackboxMgr", 8192, NULL, 2, NULL, 0);
}

void loop() {
    vTaskDelete(NULL);
}
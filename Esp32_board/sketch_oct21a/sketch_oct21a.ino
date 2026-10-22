#include <Arduino.h>
#include <Preferences.h>
#include "config_types.h"
#include "w25q128_flash.h"
#include "gps_m10_parser.h"
#include "sim_a7680c.h"
#include "ble_app_config.h"

/* Khởi tạo phần cứng */
HardwareSerial STM32_UART(1);
HardwareSerial SIM_UART(2);

W25Q128_Flash flashMemory;
GPS_M10       gpsParser;
SIM_A7680C    simModem(&SIM_UART);
Preferences   nvsPrefs;

/* Biến toàn cục hệ thống */
String                     sosPhoneNumber   = "0889602028";
volatile TelemetryPacket_t latestIMU;
volatile GPSData_t         latestGPS        = {false, 0.0, 0.0, 0.0f, 0};
volatile bool              sosTriggered     = false;
volatile bool              sosHandled       = false;
volatile float             peakImpactG      = 1.0f;
volatile float             secMaxG          = 1.0f;

bool     isZeroCalibrated = false;
float    offsetRoll       = 0.0f;
float    offsetPitch      = 0.0f;

uint8_t  rxBuffer[sizeof(TelemetryPacket_t)];
uint16_t rxIndex     = 0;
uint32_t rxByteCount = 0;

BlackboxLogRecord_t preCrashRing[10];
uint8_t             preRingIdx = 0;
BlackboxLogRecord_t fullCrashWindow[BLACKBOX_WINDOW_LEN];

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
 * TASK QUẢN LÝ HỘP ĐEN 15 GIÂY, W25Q128 & SIM 4G (Chạy trên CORE 0)
 * ============================================================================ */
void Task_Blackbox_Manager(void *pvParameters) {
    uint32_t lastPrintMs   = 0;
    uint32_t lastOneSecMs  = millis();
    bool     recordingPost = false;
    uint8_t  postSecCount  = 0;

    for (;;) {
        if (Serial.available()) {
            Process_Command_Input(Serial.readStringUntil('\n'));
        }

        uint32_t now = millis();

        // 1. Nhịp đồng hồ mỗi 1 giây: Lưu bộ đệm vòng 10s Trước & Thu 5s Sau tai nạn
        if (now - lastOneSecMs >= 1000) {
            lastOneSecMs = now;
            float g_this_sec = secMaxG;
            secMaxG = 1.0f;

            if (!recordingPost) {
                preCrashRing[preRingIdx] = takeCurrentSnapshot(0, g_this_sec);
                preRingIdx = (preRingIdx + 1) % 10;
            } else {
                postSecCount++;
                fullCrashWindow[10 + postSecCount] = takeCurrentSnapshot((int8_t)postSecCount, g_this_sec);
                Serial.printf("⏳ [HỘP ĐEN] Đang thu diễn biến SAU tai nạn: Giây T+%ds / +5s...\n", postSecCount);

                if (postSecCount >= 5) {
                    recordingPost = false;
                    flashMemory.save15SecCrashWindow(fullCrashWindow);
                    Serial.print(flashMemory.getCrashHistorySummary());
                    BLE_SendToPhone("💾 Da luu xong 15s Hop Den vao W25Q128! Go 'LOG' de xem.\n");
                }
            }
        }

        // 2. Khoảnh khắc phát hiện tai nạn (T = 0s)
        if (sosTriggered && !sosHandled) {
            sosHandled    = true;
            recordingPost = true;
            postSecCount  = 0;

            Serial.println("\n🚨 [PHÁT HIỆN TAI NẠN @ T=0s] Đóng băng 10s quá khứ & bắt đầu thu 5s sau va chạm!");
            for (int i = 0; i < 10; i++) {
                int ringPos = (preRingIdx + i) % 10;
                fullCrashWindow[i] = preCrashRing[ringPos];
                fullCrashWindow[i].rel_sec = (int8_t)(i - 10);
            }
            fullCrashWindow[10] = takeCurrentSnapshot(0, peakImpactG);
            if (fullCrashWindow[10].crash_state < 2) fullCrashWindow[10].crash_state = 2;

            GPSData_t gpsSnap;
            memcpy(&gpsSnap, (const void*)&latestGPS, sizeof(GPSData_t));
            BLE_SendToPhone("🚨 TAI NAN! Dang gui SMS SOS toi: " + sosPhoneNumber + "\n");

            simModem.sendEmergencySOS(
                sosPhoneNumber,
                latestIMU.roll,
                latestIMU.pitch,
                peakImpactG,
                gpsSnap,
                fullCrashWindow[10].crash_state
            );
            lastOneSecMs = millis();
        }

        if (latestIMU.crash_state == 0 && sosHandled && !recordingPost) {
            sosTriggered = false;
            sosHandled   = false;
            peakImpactG  = 1.0f;
        }

        if (now - lastPrintMs >= 200) {
            lastPrintMs = now;
            Serial.printf("[#%6lu | Byte:%6lu] Roll:%6.2f° | Pitch:%6.2f° | %s || SDT:%s\n",
                          latestIMU.seq_num,
                          rxByteCount,
                          latestIMU.roll,
                          latestIMU.pitch,
                          getCrashStateText(latestIMU.crash_state),
                          sosPhoneNumber.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("   HỘP ĐEN XE MÁY: BẢN PHÂN CHIA MODULE HOÀN CHỈNH     ");
    Serial.println("=======================================================");

    // 1. Khởi tạo W25Q128, Bluetooth BLE và SIM A7682S-LANS trước
    flashMemory.begin(W25Q_CS_PIN, W25Q_SCK_PIN, W25Q_MISO_PIN, W25Q_MOSI_PIN);
    BLE_Init_Server();
    simModem.initModem();

    // 2. Mở cổng STM32_UART (RX=17, TX=16) sau cùng để đảm bảo không bị ngắt
    STM32_UART.setRxBufferSize(2048);
    STM32_UART.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
    while (STM32_UART.available()) STM32_UART.read();

    xTaskCreatePinnedToCore(Task_Blackbox_Manager, "BlackboxMgr", 8192, NULL, 2, NULL, 0);
}

/* ============================================================================
 * VÒNG LẶP CHÍNH loop() (CORE 1): Đọc gói tin 100Hz từ STM32 & Auto-Zero góc
 * ============================================================================ */
void loop() {
    static uint32_t tiltStartMs = 0;

    while (STM32_UART.available()) {
        uint8_t b = STM32_UART.read();
        rxByteCount++;

        if (rxIndex == 0 && b != SYNC_BYTE_1) continue;
        if (rxIndex == 1 && b != SYNC_BYTE_2) {
            rxIndex = (b == SYNC_BYTE_1) ? 1 : 0;
            continue;
        }

        rxBuffer[rxIndex++] = b;

        if (rxIndex == sizeof(TelemetryPacket_t)) {
            rxIndex = 0;
            uint16_t calc_crc = Calculate_CRC16(rxBuffer, sizeof(TelemetryPacket_t) - 2);
            TelemetryPacket_t tempPkt;
            memcpy(&tempPkt, rxBuffer, sizeof(TelemetryPacket_t));

            if (calc_crc == tempPkt.crc16) {
                if (!isZeroCalibrated) {
                    offsetRoll  = tempPkt.roll;
                    offsetPitch = tempPkt.pitch;
                    isZeroCalibrated = true;
                }

                float relRoll = tempPkt.roll - offsetRoll;
                while (relRoll > 180.0f)  relRoll -= 360.0f;
                while (relRoll < -180.0f) relRoll += 360.0f;

                float relPitch = tempPkt.pitch - offsetPitch;
                while (relPitch > 180.0f)  relPitch -= 360.0f;
                while (relPitch < -180.0f) relPitch += 360.0f;

                tempPkt.roll  = relRoll;
                tempPkt.pitch = relPitch;

                float g_total = sqrtf(tempPkt.accel[0]*tempPkt.accel[0] +
                                      tempPkt.accel[1]*tempPkt.accel[1] +
                                      tempPkt.accel[2]*tempPkt.accel[2]);
                if (g_total > peakImpactG) peakImpactG = g_total;
                if (g_total > secMaxG)     secMaxG     = g_total;

                if (g_total > 3.5f) {
                    tempPkt.crash_state = 3; // Va chạm mạnh
                } else if (fabsf(relRoll) > 55.0f || fabsf(relPitch) > 55.0f) {
                    if (tiltStartMs == 0) tiltStartMs = millis();
                    else if (millis() - tiltStartMs >= 1500) tempPkt.crash_state = 2; // Ngã xe!
                    else tempPkt.crash_state = 0;
                } else {
                    tiltStartMs = 0;
                    tempPkt.crash_state = 0;
                }

                memcpy((void*)&latestIMU, &tempPkt, sizeof(TelemetryPacket_t));

                if ((tempPkt.crash_state == 2 || tempPkt.crash_state == 3) && !sosTriggered) {
                    sosTriggered = true;
                }
            }
        }
    }
    delay(1);
}
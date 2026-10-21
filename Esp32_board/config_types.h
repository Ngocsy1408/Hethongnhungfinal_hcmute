#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <Arduino.h>

/* 1. ĐỊNH NGHĨA CHÂN KẾT NỐI TRÊN ESP32-S3 N16R8 */
#define STM32_RX_PIN    17   // Nối PA9 của STM32
#define STM32_TX_PIN    16   // Nối PA10 của STM32
#define STM32_BAUD      115200

#define GPS_RX_PIN      18   // Nối TX của GPS M10
#define GPS_TX_PIN      8    // Nối RX của GPS M10
#define GPS_BAUD        38400

#define SIM_RX_PIN      4    // Nối TXD của A7680C
#define SIM_TX_PIN      5    // Nối RXD của A7680C
#define SIM_BAUD        115200

#define W25Q_CS_PIN     10
#define W25Q_MOSI_PIN   11
#define W25Q_SCK_PIN    12
#define W25Q_MISO_PIN   13

/* 2. CẤU TRÚC GÓI TIN NHẬN TỪ STM32 (45 BYTES) */
#define SYNC_BYTE_1     0xAA
#define SYNC_BYTE_2     0x55

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic_1;      // 0xAA
    uint8_t  magic_2;      // 0x55
    uint32_t seq_num;      // Số thứ tự gói
    uint32_t timestamp_us; // Thời gian (us)
    float    accel[3];     // Ax, Ay, Az (g)
    float    gyro[3];      // Gx, Gy, Gz (deg/s)
    float    roll;         // Góc Roll Kalman (Độ)
    float    pitch;        // Góc Pitch Kalman (Độ)
    uint8_t  crash_state;  // 0: Bình thường, 1: Phanh gấp, 2: Ngã xe, 3: Va chạm
    uint16_t crc16;        // Checksum CRC16
} TelemetryPacket_t;

/* 3. CẤU TRÚC MỖI MỐC THỜI GIAN TRONG BĂNG GHI HỘP ĐEN 15 GIÂY (LƯU VÀO W25Q128) */
#define BLACKBOX_WINDOW_LEN  16  // 10 giây trước (-10..-1) + Lúc đâm (0) + 5 giây sau (+1..+5)

typedef struct {
    uint32_t magic;        // 0xCAFEBABE
    int8_t   rel_sec;      // Mốc thời gian tương đối: từ -10s đến +5s (0s là lúc va chạm)
    uint8_t  crash_state;  // Trạng thái tại giây đó (0: Bình thường, 2: Ngã, 3: Va chạm)
    float    roll;         // Góc Roll tại giây đó
    float    pitch;        // Góc Pitch tại giây đó
    float    max_g;        // Lực G lớn nhất trong giây đó
    float    latitude;     // Vĩ độ GPS
    float    longitude;    // Kinh độ GPS
    float    speed_kmh;    // Vận tốc (km/h) tại giây đó
} BlackboxLogRecord_t;
#pragma pack(pop)

/* 4. CẤU TRÚC DỮ LIỆU GPS U-BLOX M10 */
typedef struct {
    bool     is_valid;
    double   latitude;
    double   longitude;
    float    speed_kmh;
    uint8_t  satellites;
} GPSData_t;

#endif
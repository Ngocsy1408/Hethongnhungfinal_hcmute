#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <Arduino.h>

/* 1. ĐỊNH NGHĨA CHÂN KẾT NỐI TRÊN ESP32-S3 N16R8 */
#define STM32_RX_PIN        17   // Nối với PA9 (TX) của STM32F401
#define STM32_TX_PIN        16   // Nối với PA10 (RX) của STM32F401
#define STM32_BAUD          115200

#define GPS_RX_PIN          18   // Nối với TX của GPS u-blox M10
#define GPS_TX_PIN          8    // Nối với RX của GPS u-blox M10
#define GPS_BAUD            115200 // Đã cập nhật đúng chuẩn 115200 của M10

#define SIM_RX_PIN          4    // Nối với TXD của SIM A7682S-LANS
#define SIM_TX_PIN          5    // Nối với RXD của SIM A7682S-LANS
#define SIM_BAUD            115200

#define W25Q_CS_PIN         10   // SPI2 CS cho W25Q128
#define W25Q_MOSI_PIN       11   // SPI2 MOSI (DI)
#define W25Q_SCK_PIN        12   // SPI2 SCK (CLK)
#define W25Q_MISO_PIN       13   // SPI2 MISO (DO)

/* 2. HẰNG SỐ ĐỒNG BỘ & ĐỊA CHỈ FLASH */
#define SYNC_BYTE_1         0xAA
#define SYNC_BYTE_2         0x55
#define LOG_RECORD_MAGIC    0xCAFEBABE
#define LOG_SECTOR_ADDR     0x001000UL
#define BLACKBOX_WINDOW_LEN 16   // 10 giây trước (-10..-1) + Lúc đâm (0) + 5 giây sau (+1..+5)

/* 3. GÓI TIN NHỊ PHÂN 45 BYTES TỪ STM32F401 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic_1;      // 0xAA
    uint8_t  magic_2;      // 0x55
    uint32_t seq_num;      // Số thứ tự gói tin
    uint32_t timestamp_us; // Thời gian (us)
    float    accel[3];     // Ax, Ay, Az (g)
    float    gyro[3];      // Gx, Gy, Gz (deg/s)
    float    roll;         // Góc Roll Kalman (Độ)
    float    pitch;        // Góc Pitch Kalman (Độ)
    uint8_t  crash_state;  // 0: Bình thường, 1: Phanh gấp, 2: Ngã xe, 3: Va chạm
    uint16_t crc16;        // Checksum CRC16-CCITT
} TelemetryPacket_t;

/* 4. BẢN GHI HỘP ĐEN LƯU TRONG FLASH W25Q128 */
typedef struct {
    uint32_t magic;        // 0xCAFEBABE
    int8_t   rel_sec;      // Mốc thời gian: từ -10s đến +5s (0s là lúc đâm)
    uint8_t  crash_state;  // Trạng thái tại giây đó
    float    roll;         // Góc Roll
    float    pitch;        // Góc Pitch
    float    max_g;        // Gia tốc đỉnh G trong giây đó
    float    latitude;     // Vĩ độ GPS
    float    longitude;    // Kinh độ GPS
    float    speed_kmh;    // Vận tốc (km/h)
} BlackboxLogRecord_t;
#pragma pack(pop)

/* 5. DỮ LIỆU ĐỊNH VỊ GPS U-BLOX M10 */
typedef struct {
    bool     is_valid;
    double   latitude;
    double   longitude;
    float    speed_kmh;
    uint8_t  satellites;
} GPSData_t;

#endif
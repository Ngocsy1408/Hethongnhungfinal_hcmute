#ifndef TELEMETRY_PROTOCOL_H
#define TELEMETRY_PROTOCOL_H

#include <stdint.h>

#define SYNC_BYTE_1 0xAA
#define SYNC_BYTE_2 0x55

typedef enum {
    CRASH_STATE_NORMAL     = 0x00,
    CRASH_STATE_HARD_BRAKE = 0x01,
    CRASH_STATE_FALLEN     = 0x02,
    CRASH_STATE_COLLISION  = 0x03
} CrashStatus_e;

#pragma pack(push, 1)
typedef struct {
    uint8_t       magic_1;     // 0xAA
    uint8_t       magic_2;     // 0x55
    uint32_t      seq_num;     // Số thứ tự tăng dần của frame
    uint32_t      timestamp_us;// Microsecond timestamp
    float         accel[3];    // Ax, Ay, Az (g)
    float         gyro[3];     // Gx, Gy, Gz (deg/s)
    float         roll;        // Kalman filtered roll (deg)
    float         pitch;       // Kalman filtered pitch (deg)
    uint8_t       crash_state; // Máy trạng thái sự cố
    uint16_t      crc16;       // Checksum toàn bộ frame
} TelemetryPacket_t;
#pragma pack(pop)

// Bảng tra hoặc thuật toán tính CRC-16-CCITT (Poly: 0x1021, Init: 0xFFFF)
static inline uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

#endif
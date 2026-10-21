#ifndef ICM42688_DEFS_H
#define ICM42688_DEFS_H

// Địa chỉ định danh thiết bị
#define ICM42688_WHO_AM_I_VAL         0x47
#define ICM42688_REG_BANK_SEL         0x76

// BANK 0 REGISTERS
#define REG_DEVICE_CONFIG             0x11
#define REG_DRIVE_CONFIG              0x13
#define REG_INT_CONFIG                0x14
#define REG_FIFO_CONFIG               0x16
#define REG_TEMP_DATA1                0x1D
#define REG_ACCEL_DATA_X1             0x1F
#define REG_GYRO_DATA_X1              0x25
#define REG_PWR_MGMT0                 0x4E
#define REG_GYRO_CONFIG0              0x4F
#define REG_ACCEL_CONFIG0             0x50
#define REG_GYRO_CONFIG1              0x51
#define REG_GYRO_ACCEL_CONFIG0        0x52
#define REG_ACCEL_CONFIG1             0x53
#define REG_WHO_AM_I                  0x75

// BANK 1 REGISTERS (Anti-Aliasing Filter configs)
#define REG_GYRO_CONFIG_STATIC2       0x0B
#define REG_GYRO_CONFIG_STATIC3       0x0C
#define REG_GYRO_CONFIG_STATIC4       0x0D

// BANK 2 REGISTERS (Anti-Aliasing Filter configs)
#define REG_ACCEL_CONFIG_STATIC2      0x03
#define REG_ACCEL_CONFIG_STATIC3      0x04
#define REG_ACCEL_CONFIG_STATIC4      0x05

/* THÔNG SỐ CẤU HÌNH DÀNH RIÊNG CHO XE MÁY */

// Dải đo: Accel +/- 16G (Chống kịch trần khi vấp ổ gà/va chạm)
// 16G = 2048 LSB/g
#define ACCEL_FS_16G                  (0x00 << 5)
#define ACCEL_SCALE_16G               (1.0f / 2048.0f)

// Dải đo: Gyro +/- 2000 dps (Chống kịch trần khi xe trượt ngã/lộn vòng)
// 2000dps = 16.4 LSB/(deg/s)
#define GYRO_FS_2000DPS               (0x00 << 5)
#define GYRO_SCALE_2000DPS            (1.0f / 16.4f)

// Tần số lấy mẫu cảm biến ODR: 100Hz (Cân bằng tải tính toán và xử lý động học)
#define ODR_100HZ                     0x08

// Chế độ nguồn: Cả Gyro và Accel đều ở chế độ Low Noise (LN) Mode
#define PWR_MODE_LN                   0x0F

#endif
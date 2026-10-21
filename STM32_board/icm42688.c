#include "icm42688.h"
#include "icm42688_defs.h"

// Macro chọn/bỏ chọn chân CS của SPI
#define CS_LOW(dev)   HAL_GPIO_WritePin(dev->csPinBank, dev->csPin, GPIO_PIN_RESET)
#define CS_HIGH(dev)  HAL_GPIO_WritePin(dev->csPinBank, dev->csPin, GPIO_PIN_SET)

// Ghi 1 thanh ghi SPI (Thêm bit 0x00 vào MSB để báo lệnh Write)
static void WriteRegister(ICM42688_t *dev, uint8_t reg, uint8_t data) {
    uint8_t tx[2] = { reg & 0x7F, data };
    CS_LOW(dev);
    HAL_SPI_Transmit(dev->spiHandle, tx, 2, 10);
    CS_HIGH(dev);
}

// Đọc 1 hoặc nhiều byte từ thanh ghi SPI (Bit 0x80 ở MSB báo lệnh Read)
static void ReadRegisters(ICM42688_t *dev, uint8_t reg, uint8_t *buffer, uint16_t length) {
    uint8_t addr = reg | 0x80;
    CS_LOW(dev);
    HAL_SPI_Transmit(dev->spiHandle, &addr, 1, 10);
    HAL_SPI_Receive(dev->spiHandle, buffer, length, 10);
    CS_HIGH(dev);
}

static void SelectBank(ICM42688_t *dev, uint8_t bank) {
    WriteRegister(dev, ICM42688_REG_BANK_SEL, bank);
}

bool ICM42688_Init(ICM42688_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin) {
    dev->spiHandle = hspi;
    dev->csPinBank = cs_port;
    dev->csPin     = cs_pin;

    CS_HIGH(dev);
    HAL_Delay(10);

    // 1. Kiểm tra WHO_AM_I
    uint8_t who_am_i = 0;
    SelectBank(dev, 0);
    ReadRegisters(dev, REG_WHO_AM_I, &who_am_i, 1);
    if (who_am_i != ICM42688_WHO_AM_I_VAL) {
        return false; // Sai chip hoặc lỗi dây SPI
    }

    // 2. Soft Reset cảm biến về trạng thái sạch sẽ
    WriteRegister(dev, REG_DEVICE_CONFIG, 0x01);
    HAL_Delay(5); // Chờ khởi động lại sau reset

    // 3. Đưa Accel & Gyro vào chế độ Low Noise (LN Mode)
    WriteRegister(dev, REG_PWR_MGMT0, PWR_MODE_LN);
    HAL_Delay(2); // Chờ mạch PLL bên trong ổn định

    // 4. Cấu hình Dải đo (FSR) và Tốc độ lấy mẫu (ODR)
    // Accel: +/- 16G, ODR: 100Hz
    WriteRegister(dev, REG_ACCEL_CONFIG0, ACCEL_FS_16G | ODR_100HZ);
    // Gyro: +/- 2000dps, ODR: 100Hz
    WriteRegister(dev, REG_GYRO_CONFIG0, GYRO_FS_2000DPS | ODR_100HZ);

    // 5. CẤU HÌNH LỌC SỐ CHỐNG RUNG ĐỘNG CƠ (UI DLPF Filter Order 2)
    // Đặt tần số cắt BW = ODR / 4 (~25Hz đối với ODR 100Hz)
    // Giúp triệt hoàn toàn tiếng ré của động cơ, xe đi êm hay xóc đều không giật đồ thị
    WriteRegister(dev, REG_GYRO_ACCEL_CONFIG0, 0x22); 

    // 6. CẤU HÌNH BỘ LỌC CỨNG ANTI-ALIASING (AAF) CHO ACCEL
    SelectBank(dev, 2);
    WriteRegister(dev, REG_ACCEL_CONFIG_STATIC2, 0x30); // Bật AAF
    WriteRegister(dev, REG_ACCEL_CONFIG_STATIC3, 0x40); // Tần số cắt AAF ~42Hz
    WriteRegister(dev, REG_ACCEL_CONFIG_STATIC4, 0x00);

    // 7. CẤU HÌNH BỘ LỌC CỨNG ANTI-ALIASING (AAF) CHO GYRO
    SelectBank(dev, 1);
    WriteRegister(dev, REG_GYRO_CONFIG_STATIC2, 0x30); // Bật AAF
    WriteRegister(dev, REG_GYRO_CONFIG_STATIC3, 0x40); // Tần số cắt AAF ~42Hz
    WriteRegister(dev, REG_GYRO_CONFIG_STATIC4, 0x00);

    // Quay lại Bank 0 cho các chu kỳ đọc dữ liệu tiếp theo
    SelectBank(dev, 0);

    return true; // Khởi tạo hoàn tất
}

bool ICM42688_ReadData(ICM42688_t *dev, IMUData_t *data) {
    uint8_t raw_buf[14]; // 2 Temp + 6 Accel + 6 Gyro = 14 bytes

    // Burst Read 14 thanh ghi cùng lúc để bảo đảm tính đồng bộ dữ liệu (Time Coherency)
    ReadRegisters(dev, REG_TEMP_DATA1, raw_buf, 14);

    int16_t raw_temp = (int16_t)((raw_buf[0]  << 8) | raw_buf[1]);
    int16_t raw_ax   = (int16_t)((raw_buf[2]  << 8) | raw_buf[3]);
    int16_t raw_ay   = (int16_t)((raw_buf[4]  << 8) | raw_buf[5]);
    int16_t raw_az   = (int16_t)((raw_buf[6]  << 8) | raw_buf[7]);
    int16_t raw_gx   = (int16_t)((raw_buf[8]  << 8) | raw_buf[9]);
    int16_t raw_gy   = (int16_t)((raw_buf[10] << 8) | raw_buf[11]);
    int16_t raw_gz   = (int16_t)((raw_buf[12] << 8) | raw_buf[13]);

    // Ép tỉ lệ chuẩn (Scaling Factors)
    data->ax = (float)raw_ax * ACCEL_SCALE_16G;
    data->ay = (float)raw_ay * ACCEL_SCALE_16G;
    data->az = (float)raw_az * ACCEL_SCALE_16G;

    data->gx = (float)raw_gx * GYRO_SCALE_2000DPS;
    data->gy = (float)raw_gy * GYRO_SCALE_2000DPS;
    data->gz = (float)raw_gz * GYRO_SCALE_2000DPS;

    // Nhiệt độ nội (Dùng để bù trừ nhiệt độ trôi bias nếu cần)
    data->temp = ((float)raw_temp / 132.48f) + 25.0f;

    return true;
}
#include "stm32f4xx_hal.h"
#include "dwt_stm32.h"
#include "icm42688.h"
#include "adaptive_kalman.h"
#include "telemetry_protocol.h"
#include <math.h>

#define RAD_TO_DEG 57.29577951308232f

volatile uint32_t uwTick = 0;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

ICM42688_t        imu;
IMUData_t         imu_raw;
AdaptiveKalman_t  kfRoll, kfPitch;
TelemetryPacket_t tx_packet;
TelemetryPacket_t dma_tx_buffer;

uint32_t packet_seq = 0;
uint32_t last_us = 0;

/* QUAN TRỌNG: Bật FPU tự động TRƯỚC KHI vào hàm main() để chống treo chip */
__attribute__((constructor)) void Enable_FPU_Before_Main(void) {
    SCB_CPACR |= (0xFUL << 20);
}

/* Ngắt đồng hồ hệ thống 1ms */
void SysTick_Handler(void) {
    uwTick++;
}

/* Khởi tạo toàn bộ xung nhịp 84MHz, GPIO, SPI1, UART1, DMA2 */
void Hardware_Init_All(void) {
    SCB_CPACR |= (0xFUL << 20);

    // 1. Đưa Clock về HSI mặc định trước khi cài PLL (An toàn khi nạp lại nhiều lần)
    RCC_CR |= (1UL << 0);
    RCC_CFGR &= ~3UL;
    while ((RCC_CFGR & (3UL << 2)) != 0UL) {}
    RCC_CR &= ~(1UL << 24);
    while (RCC_CR & (1UL << 25)) {}

    // 2. Cấu hình PLL lên 84MHz từ HSI 16MHz
    FLASH_ACR = (1UL << 8) | (1UL << 9) | (1UL << 10) | 2UL;
    RCC_PLLCFGR = (16UL << 0) | (336UL << 6) | (1UL << 16) | (7UL << 24);
    RCC_CR |= (1UL << 24);
    while (!(RCC_CR & (1UL << 25))) {}
    RCC_CFGR = (4UL << 10) | 2UL;
    while ((RCC_CFGR & (3UL << 2)) != (2UL << 2)) {}

    // 3. Bật SysTick 1ms (84MHz / 1000)
    SYST_RVR = 84000UL - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = 7UL;

    // 4. Bật Clock cho GPIOA, GPIOC, DMA2, SPI1, USART1
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 2) | (1UL << 22);
    RCC_APB2ENR |= (1UL << 4) | (1UL << 12);

    // 5. Cấu hình GPIO: PA4 (CS), PA5/6/7 (SPI1 AF5), PA9/10 (UART1 AF7), PC13 (LED)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    GPIOA->MODER   &= ~((3UL << 8) | (3UL << 10) | (3UL << 12) | (3UL << 14) | (3UL << 18) | (3UL << 20));
    GPIOA->MODER   |=  ((1UL << 8) | (2UL << 10) | (2UL << 12) | (2UL << 14) | (2UL << 18) | (2UL << 20));
    GPIOA->OSPEEDR |=  ((3UL << 8) | (3UL << 10) | (3UL << 12) | (3UL << 14) | (3UL << 18) | (3UL << 20));
    GPIOA->AFR[0]  &= ~((0xFUL << 20) | (0xFUL << 24) | (0xFUL << 28));
    GPIOA->AFR[0]  |=  ((5UL << 20)   | (5UL << 24)   | (5UL << 28));
    GPIOA->AFR[1]  &= ~((0xFUL << 4)  | (0xFUL << 8));
    GPIOA->AFR[1]  |=  ((7UL << 4)    | (7UL << 8));

    GPIOC->MODER   &= ~(3UL << 26);
    GPIOC->MODER   |=  (1UL << 26); // PC13 Output

    // 6. Cấu hình SPI1 Mode 3 (CPOL=1, CPHA=1, Master, Baudrate = 84MHz/8 = 10.5MHz)
    SPI1_CR1 = (1UL << 0) | (1UL << 1) | (1UL << 2) | (2UL << 3) | (1UL << 8) | (1UL << 9) | (1UL << 6);

    // 7. Cấu hình USART1 (115200 bps @ 84MHz) + Bật truyền TX/RX
    USART1_BRR = 729UL;
    USART1_CR3 = (1UL << 7);
    USART1_CR1 = (1UL << 2) | (1UL << 3) | (1UL << 13);

    // 8. Cấu hình DMA2_Stream7 (Channel 4, Memory-to-Peripheral, High Priority)
    DMA2_S7CR = (4UL << 25) | (2UL << 16) | (1UL << 10) | (1UL << 6);
}

CrashStatus_e Detect_Accident(float roll, float pitch, float acc_mag, float gyro_mag) {
    static uint32_t fall_timer = 0;
    if (acc_mag > 4.5f) return CRASH_STATE_COLLISION;

    if ((fabsf(roll) > 55.0f || fabsf(pitch) > 60.0f) && (gyro_mag < 15.0f)) {
        if (fall_timer == 0) fall_timer = HAL_GetTick();
        else if (HAL_GetTick() - fall_timer > 1500) return CRASH_STATE_FALLEN;
    } else {
        fall_timer = 0;
    }
    return CRASH_STATE_NORMAL;
}

/* Tách riêng hàm tính toán số thực và bắn thẳng qua chân PA9 (USART1) */
__attribute__((noinline)) void Process_IMU_Step(uint32_t current_us, uint32_t delta_us, bool sensor_ok) {
    float dt = (float)delta_us / 1000000.0f;

    if (sensor_ok) {
        ICM42688_ReadData(&imu, &imu_raw);
    } else {
        imu_raw.ax = 0.0f; imu_raw.ay = 0.0f; imu_raw.az = 1.0f;
        imu_raw.gx = 0.0f; imu_raw.gy = 0.0f; imu_raw.gz = 0.0f;
    }

    float acc_mag  = sqrtf(imu_raw.ax * imu_raw.ax + imu_raw.ay * imu_raw.ay + imu_raw.az * imu_raw.az);
    float gyro_mag = sqrtf(imu_raw.gx * imu_raw.gx + imu_raw.gy * imu_raw.gy + imu_raw.gz * imu_raw.gz);

    float roll_acc  = atan2f(imu_raw.ay, imu_raw.az + 1e-6f) * RAD_TO_DEG;
    float pitch_acc = atan2f(-imu_raw.ax, sqrtf(imu_raw.ay * imu_raw.ay + imu_raw.az * imu_raw.az) + 1e-6f) * RAD_TO_DEG;

    float roll_clean  = AKF_Update(&kfRoll, roll_acc, imu_raw.gx, dt, acc_mag);
    float pitch_clean = AKF_Update(&kfPitch, pitch_acc, imu_raw.gy, dt, acc_mag);

    CrashStatus_e status = Detect_Accident(roll_clean, pitch_clean, acc_mag, gyro_mag);

    tx_packet.seq_num      = ++packet_seq;
    tx_packet.timestamp_us = current_us;
    tx_packet.accel[0]     = imu_raw.ax;
    tx_packet.accel[1]     = imu_raw.ay;
    tx_packet.accel[2]     = imu_raw.az;
    tx_packet.gyro[0]      = imu_raw.gx;
    tx_packet.gyro[1]      = imu_raw.gy;
    tx_packet.gyro[2]      = imu_raw.gz;
    tx_packet.roll         = roll_clean;
    tx_packet.pitch        = pitch_clean;
    // Nếu chưa nhận cảm biến ICM42688 thì gửi mã 99 sang ESP32 để báo trên màn hình
    tx_packet.crash_state  = sensor_ok ? (uint8_t)status : 99;
    tx_packet.crc16        = Calculate_CRC16((uint8_t*)&tx_packet, sizeof(TelemetryPacket_t) - 2);

    // Bắn trực tiếp 45 bytes ra chân PA9 (Đảm bảo 100% có xung UART phát ra)
    uint8_t *raw_bytes = (uint8_t*)&tx_packet;
    for (uint16_t i = 0; i < sizeof(TelemetryPacket_t); i++) {
        while (!(USART1_SR & (1U << 7))) {} // Chờ thanh ghi phát trống (TXE)
        USART1_DR = raw_bytes[i];
    }

    // Nhịp tim đèn LED PC13
    if ((packet_seq % 50) == 0) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

int main(void) {
    Hardware_Init_All();
    DWT_Init();

    // Kiểm tra ICM-42688 nhưng KHÔNG chặn while(1) nữa để UART luôn bắn sang ESP32!
    bool sensor_ok = ICM42688_Init(&imu, &hspi1, GPIOA, GPIO_PIN_4);

    AKF_Init(&kfRoll, 0.001f, 0.003f, 0.05f);
    AKF_Init(&kfPitch, 0.001f, 0.003f, 0.05f);

    tx_packet.magic_1 = SYNC_BYTE_1;
    tx_packet.magic_2 = SYNC_BYTE_2;
    last_us = DWT_GetMicroseconds();

    while (1) {
        uint32_t current_us = DWT_GetMicroseconds();
        uint32_t delta_us = current_us - last_us;
        if (delta_us >= 10000U) { // Chu kỳ 100Hz (10ms)
            last_us = current_us;
            // Nếu lúc đầu lỏng dây cảm biến, tự động thử kết nối lại mỗi 1 giây
            if (!sensor_ok && (packet_seq % 100 == 0)) {
                sensor_ok = ICM42688_Init(&imu, &hspi1, GPIOA, GPIO_PIN_4);
            }
            Process_IMU_Step(current_us, delta_us, sensor_ok);
        }
    }
}

/* 4 hàm hệ thống rỗng cho trình biên dịch GCC 14.3 */
int _close(int file) { (void)file; return -1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }

#ifndef ICM42688_H
#define ICM42688_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

typedef struct {
    SPI_HandleTypeDef *spiHandle;
    GPIO_TypeDef      *csPinBank;
    uint16_t          csPin;
} ICM42688_t;

typedef struct {
    float ax, ay, az; // Đơn vị: g
    float gx, gy, gz; // Đơn vị: deg/s
    float temp;       // Đơn vị: độ C
} IMUData_t;

// API functions
bool ICM42688_Init(ICM42688_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
bool ICM42688_ReadData(ICM42688_t *dev, IMUData_t *data);

#endif

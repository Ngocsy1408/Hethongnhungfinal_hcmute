#ifndef DWT_STM32_H
#define DWT_STM32_H

#include "stm32f4xx_hal.h"

static inline void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t DWT_GetMicroseconds(void) {
    // 84MHz clock: 84 cycles = 1 microsecond
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

#endif
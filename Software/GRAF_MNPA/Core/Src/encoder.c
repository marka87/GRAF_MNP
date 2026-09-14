/*
 * encoder.c
 *
 * Direct, atomic 32-bit hardware timer encoder interface for TIM2 and TIM5.
 * Hardware handles quadrature decoding (TI12) and two's complement direction.
 */

#include "encoder.h"
#include <stm32f7xx_hal_tim.h>

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim5;

void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    TIM2->CNT = 0;

    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    TIM5->CNT = 0;
}

int32_t Encoder_GetPosition_A_AXIS(void) {
    return (int32_t)TIM2->CNT;
}

int32_t Encoder_GetPosition_Z_AXIS(void) {
    return (int32_t)TIM5->CNT;
}

void Encoder_Reset_A_AXIS(void) {
    TIM2->CNT = 0;
}

void Encoder_Reset_Z_AXIS(void) {
    TIM5->CNT = 0;
}

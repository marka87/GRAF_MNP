/*
 * encoder.c
 *
 *  Created on: Dec 4, 2024
 *      Author: Mark
 */

// encoder.c
#include "encoder.h"
#include <stm32f7xx_hal_tim.h>
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim5;

// Initialize the Encoders
void Encoder_Init(void) {
    // Start TIM2 for A_AXIS
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim2, 0); // Reset position to zero
//    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
//    __HAL_TIM_SET_COUNTER(&htim3, 0); // Reset position to zero

    // Start TIM5 for Z_AXIS
    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim5, 0); // Reset position to zero
}

// Get the position for A_AXIS
//uint32_t Encoder_GetPosition_A_AXIS(void) {
//    return __HAL_TIM_GET_COUNTER(&htim2);
//}

// Get the position for Z_AXIS
uint32_t Encoder_GetPosition_Z_AXIS(void) {
    return __HAL_TIM_GET_COUNTER(&htim5);
}

uint32_t Encoder_GetPosition_A_AXIS(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

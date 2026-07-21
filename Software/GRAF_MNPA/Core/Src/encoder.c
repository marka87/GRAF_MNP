/*
 * encoder.c
 *
 *  Created on: Dec 4, 2024
 *      Author: Mark
 */

// encoder.c
#include "encoder.h"
#include <stm32f7xx_hal_tim.h>
#include <stdlib.h>
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim5;
volatile int32_t encoder_position_A_AXIS = 0;
volatile int32_t encoder_position_Z_AXIS = 0;

// Initialize the Encoders
void Encoder_Init(void) {
    // Start TIM2 for A_AXIS
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim2, 0); // Reset position to zero

    // Start TIM5 for Z_AXIS
    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim5, 0); // Reset position to zero
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim2) { // A_AXIS Encoder
        if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim)) {
            encoder_position_A_AXIS -= UINT32_MAX; // Unterlauf
        } else {
            encoder_position_A_AXIS += UINT32_MAX; // Überlauf
        }
    } else if (htim == &htim5) { // Z_AXIS Encoder
        if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim)) {
            encoder_position_Z_AXIS -= UINT32_MAX; // Unterlauf
        } else {
            encoder_position_Z_AXIS += UINT32_MAX; // Überlauf
        }
    }
}
int32_t Encoder_GetPosition_A_AXIS(void) {
    // Double-Check-Locking ohne IRQ-Blockierung
    // Erste Leseoperation ohne Schutz (Optimierung für Common Case)
    int32_t counter = __HAL_TIM_GET_COUNTER(&htim2);
    int32_t position = encoder_position_A_AXIS;

    // Zweite Leseoperation: Falls Interrupt dazwischen war, erneut lesen
    int32_t counter2 = __HAL_TIM_GET_COUNTER(&htim2);

    // Wenn Counter sich verändert hat (Interrupt trat auf), IRQ blockieren und neu lesen
    if (counter2 < counter) {
        __disable_irq();
        position = encoder_position_A_AXIS + __HAL_TIM_GET_COUNTER(&htim2);
        __enable_irq();
    } else {
        position = position + counter;
    }

    return position;
}

int32_t Encoder_GetPosition_Z_AXIS(void) {
    // Double-Check-Locking ohne IRQ-Blockierung
    // Erste Leseoperation ohne Schutz (Optimierung für Common Case)
    int32_t counter = __HAL_TIM_GET_COUNTER(&htim5);
    int32_t position = encoder_position_Z_AXIS;

    // Zweite Leseoperation: Falls Interrupt dazwischen war, erneut lesen
    int32_t counter2 = __HAL_TIM_GET_COUNTER(&htim5);

    // Wenn Counter sich verändert hat (Interrupt trat auf), IRQ blockieren und neu lesen
    if (counter2 < counter) {
        __disable_irq();
        position = encoder_position_Z_AXIS + __HAL_TIM_GET_COUNTER(&htim5);
        if (labs(position) < 1L) {  // Toleranzbereich
            encoder_position_Z_AXIS = 0;
            __HAL_TIM_SET_COUNTER(&htim5, 0);
            position = 0;
        }
        __enable_irq();
    } else {
        position = position + counter;
        if (labs(position) < 1L) {  // Toleranzbereich
            position = 0;
        }
    }

    return position;
}


//int32_t Encoder_GetPosition_Z_AXIS(void) {
//    return __HAL_TIM_GET_COUNTER(&htim5);
//}
//
//int32_t Encoder_GetPosition_A_AXIS(void) {
//    return __HAL_TIM_GET_COUNTER(&htim2);
//}

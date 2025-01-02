/*
 * AAXIS_TASK.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#include "main.h"
#include "AD5684RARUZ.h"
#include "encoder.h" // Include your encoder header file
#include "DAC_task.h"
#include <stm32f7xx_hal_tim.h>
#include "Reference_Run.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim5;

#define a_mot 0x01		//Address for DAC-A...
#define TARGET_POSITION 0 // Middle position for the encoder
#define KP 0.05f           // Proportional gain
#define KI 0.001f          // Integral gain
#define KD 0.01f          // Derivative gain
#define VOLTAGE_MIN 0.0f  // Minimum voltage output
#define VOLTAGE_MAX 5.0f  // Maximum voltage output
#define INTEGRAL_LIMIT 1000.0f


volatile float integral = 0.0f;
volatile float previous_error = 0.0f;
ad5684_dac_t dac;

void Start_Task() {
    Motor_ReferenceRun(&dac);
}


void Motor_Regulator_Init(ad5684_dac_t* dac_ptr) {
    dac = *dac_ptr;
    // Configure and start the timer interrupt
    HAL_TIM_Base_Start_IT(&htim1);
}

void Motor_Regulator_Update() {
	if (integral > INTEGRAL_LIMIT) integral = INTEGRAL_LIMIT;
	if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    int encoder_value = Encoder_GetPosition_A_AXIS();
    int error = TARGET_POSITION - encoder_value;
    integral += error;
    float derivative = error - previous_error;
    float output = KP * error + KI * integral + KD * derivative;
    float voltage = 2.5f + output;

    if (voltage < VOLTAGE_MIN) voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX) voltage = VOLTAGE_MAX;

    ad5684_set_voltage(&dac, voltage, a_mot);
    previous_error = error;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
    	HAL_GPIO_TogglePin(GPIOJ, LED_GREEN_Pin);
        Motor_Regulator_Update();
    }
}

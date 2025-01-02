/*
 * AAXIS_TASK.h
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#ifndef SRC_AAXIS_TASK_H_
#define SRC_AAXIS_TASK_H_
#include "AD5684RARUZ.h"



#endif /* SRC_AAXIS_TASK_H_ */

void Start_Task();
void Motor_Regulator_Init(ad5684_dac_t* dac_ptr);
void Motor_Regulator_Update();
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

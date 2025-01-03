/*
 * PID_Control.h
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#ifndef SRC_PID_CONTROL_H_
#define SRC_PID_CONTROL_H_

#include "AD5684RARUZ.h"
extern uint32_t A_Axis_TargetPosition;
extern uint32_t allowedMin;
extern uint32_t allowedMax;
// Funktionsprototyp
void A_Axis_PIDControl(ad5684_dac_t* dac, uint32_t A_Axis_TargetPosition);

#endif /* SRC_PID_CONTROL_H_ */

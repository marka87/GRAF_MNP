/*
 * Z_PID_Control.h
 *
 *  Created on: Jan 3, 2025
 *      Author: Mark
 */

#ifndef SRC_Z_PID_CONTROL_H_
#define SRC_Z_PID_CONTROL_H_
#include "AD5684RARUZ.h"
#include <stdbool.h>

extern float voltage;
void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition);
void Z_Axis_Control(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition, bool holding);
void Z_PID_SetParameters(float kp, float ki, float kd);
void Z_PID_GetParameters(float *kp, float *ki, float *kd);
void Z_PID_EmergencyNeutral(ad5684_dac_t *dac);
#endif /* SRC_Z_PID_CONTROL_H_ */

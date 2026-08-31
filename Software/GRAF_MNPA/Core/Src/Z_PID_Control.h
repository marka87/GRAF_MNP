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
#include <stdint.h>

extern float voltage;
void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition);
void Z_Axis_Control(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition, bool holding);
void Z_PID_SetMode(bool fast_mode);
void Z_PID_SetSchedulerEnabled(bool enabled);
void Z_PID_SetSpeedLevel(uint8_t level);
uint8_t Z_PID_GetSpeedLevel(void);
bool Z_PID_IsSchedulerEnabled(void);
void Z_PID_SetFastParameters(float kp, float ki, float kd);
void Z_PID_SetMediumParameters(float kp, float ki, float kd);
void Z_PID_SetSlowParameters(float kp, float ki, float kd);
void Z_PID_GetFastParameters(float *kp, float *ki, float *kd);
void Z_PID_GetMediumParameters(float *kp, float *ki, float *kd);
void Z_PID_GetSlowParameters(float *kp, float *ki, float *kd);
void Z_PID_SetSchedulerParameters(uint32_t slow_enter, uint32_t fast_exit, uint32_t hold_target_delta, uint32_t hold_cycles);
void Z_PID_GetSchedulerParameters(uint32_t *slow_enter, uint32_t *fast_exit, uint32_t *hold_target_delta, uint32_t *hold_cycles);
void Z_PID_SetMediumSchedulerParameters(uint32_t medium_enter, uint32_t medium_exit);
void Z_PID_GetMediumSchedulerParameters(uint32_t *medium_enter, uint32_t *medium_exit);
void Z_PID_SetParameters(float kp, float ki, float kd);
void Z_PID_GetParameters(float *kp, float *ki, float *kd);
void Z_PID_EmergencyNeutral(ad5684_dac_t *dac);
#endif /* SRC_Z_PID_CONTROL_H_ */

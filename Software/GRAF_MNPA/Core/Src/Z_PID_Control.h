/*
 * Z_PID_Control.h
 *
 *  Created on: Jan 3, 2025
 *      Author: Mark
 */

#ifndef SRC_Z_PID_CONTROL_H_
#define SRC_Z_PID_CONTROL_H_
#include "AD5684RARUZ.h"

extern uint32_t Z_Axis_TargetPosition;
void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition);


#endif /* SRC_Z_PID_CONTROL_H_ */

/*
 * Reference_Run.h
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#ifndef SRC_REFERENCE_RUN_H_
#define SRC_REFERENCE_RUN_H_

#include "AD5684RARUZ.h"
#include "display.h"
extern uint32_t A_Axis_TargetPosition;
extern uint32_t Z_Axis_TargetPosition;
extern uint32_t encoder_start;
extern uint32_t encoder_end;
extern uint32_t z_encoder_start;
extern uint32_t z_encoder_end;
void A_Axis_ReferenceRun(ad5684_dac_t* dac);
void Z_Axis_ReferenceRun(ad5684_dac_t* dac);

#endif /* SRC_REFERENCE_RUN_H_ */

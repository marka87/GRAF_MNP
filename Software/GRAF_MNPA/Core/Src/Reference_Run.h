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
#include "ADC_read.h"

extern uint32_t A_Axis_TargetPosition;
extern uint32_t Z_Axis_TargetPosition;
extern int32_t a_encoder_start;
extern int32_t a_encoder_end;
extern int32_t z_encoder_start;
extern int32_t z_encoder_end;
extern int32_t z_ax_no_pos;

//void A_Axis_ReferenceRun(ad5684_dac_t* dac);
void A_Axis_ReferenceRun(ad5684_dac_t *dac, bool* success);
void Z_Axis_ReferenceRun(ad5684_dac_t *dac, bool* success);

#endif /* SRC_REFERENCE_RUN_H_ */

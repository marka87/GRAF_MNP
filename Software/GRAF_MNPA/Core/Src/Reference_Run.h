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

// Funktionsprototyp für Referenzlauf
extern uint32_t A_Axis_TargetPosition;
extern uint32_t Z_Axis_TargetPosition;

extern uint32_t allowedMin;
extern uint32_t allowedMax;

void A_Axis_ReferenceRun(ad5684_dac_t* dac);
void Z_Axis_ReferenceRun(ad5684_dac_t* dac);

#endif /* SRC_REFERENCE_RUN_H_ */

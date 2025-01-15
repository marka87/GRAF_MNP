/*
 * encoder.h
 *
 *  Created on: Dec 4, 2024
 *      Author: Mark Angyal
 *      Header File for encoder
 */

#ifndef SRC_ENCODER_H_
#define SRC_ENCODER_H_
#include <main.h>
#include <stm32f7xx_hal_tim.h>


void Encoder_Init(void);
int32_t Encoder_GetPosition_A_AXIS(void);
int32_t Encoder_GetPosition_Z_AXIS(void);

#endif /* SRC_ENCODER_H_ */

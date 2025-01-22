/*
 * d_mot_control.c
 *
 *  Created on: Jan 22, 2025
 *      Author: Mark
 */

#define d_mot 0x04		//DAC-C...
#include "AD5684RARUZ.h"

void d_mot_control(ad5684_dac_t* dac, float d_mot_voltage){
    ad5684_set_voltage(dac, d_mot_voltage, d_mot);
}

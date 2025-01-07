/*
 * Reference_Run.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#include "display.h"
#include "main.h"
#include "AD5684RARUZ.h"
#include "encoder.h"
#include "Reference_Run.h"
#include <stm32f7xx_hal_tim.h>
#include "PID_Control.h"
#include <stdlib.h>
#include <stdio.h>
#include "ADC_read.h"
#define a_mot 0x01 // Address for DAC-A...
#define z_mot 0x02 // Address for DAC-B...

#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define ENCODER_TOLERANCE 100

uint32_t A_Axis_TargetPosition = 0;
uint32_t Z_Axis_TargetPosition = 0;
uint32_t encoder_start = 0;
uint32_t encoder_end = 0;
uint32_t z_encoder_start = 0;
uint32_t z_encoder_end = 0;

extern ADC_HandleTypeDef hadc1; // Declare hadc1 as an external variable

void A_Axis_ReferenceRun(ad5684_dac_t *dac) {


	// Relais aktivieren
	HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);

	// Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
	ad5684_set_voltage(dac, 3.0f, a_mot);// Richtung: Uhrzeigersinn auf position 0
	HAL_Delay(2500);
	encoder_start = Encoder_GetPosition_A_AXIS(); // Anfangsposition speichern

	// Schritt 2: Motor in entgegengesetzter Richtung (2V) für 2,5 Sekunden
	ad5684_set_voltage(dac, 2.0f, a_mot); // Richtung: Gegen Uhrzeigersinn auf position 24000
	HAL_Delay(2500);
	encoder_end = Encoder_GetPosition_A_AXIS(); // Endposition speichern
	HAL_Delay(100);
	// Schritt 3: Mitte berechnen
	A_Axis_TargetPosition = (encoder_start + encoder_end) / 2;
	// Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
    HAL_Delay(100);
}

void Z_Axis_ReferenceRun(ad5684_dac_t *dac) {


	// Relais aktivieren
	HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);

	// Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
	ad5684_set_voltage(dac, 2.8f, z_mot); // Richtung: runter auf position 0
	HAL_Delay(500);
	z_encoder_start = Encoder_GetPosition_Z_AXIS(); // Anfangsposition speichern

	// Schritt 2: Motor in entgegengesetzter Richtung (2V) bis zum maximalen Wert
	ad5684_set_voltage(dac, 2.0f, z_mot); // Richtung: oben auf position ~3500
	HAL_Delay(2500);
	z_encoder_end = Encoder_GetPosition_Z_AXIS();
	HAL_Delay(50);
	// Schritt 3: Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);
	HAL_Delay(100);
	// Schritt 4: Mitte berechnen
//	Z_Axis_TargetPosition = (z_encoder_start + z_encoder_end) / 2;
}

/*
 * Reference_Run.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */
#include "Reference_Run.h"
#include "main.h"
#include "display.h"

#include "AD5684RARUZ.h"
#include "encoder.h"
#include <stm32f7xx_hal_tim.h>
#include "PID_Control.h"
#include <stdlib.h>
#include <stdio.h>
#define a_mot 0x01 // Address for DAC-A...
#define z_mot 0x02 // Address for DAC-B...
#define d_mot 0x04 // Address for DAC-C...

#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define ENCODER_TOLERANCE 100
extern ADC_HandleTypeDef hadc1;
extern display_info_t display1;
extern char display_buffer[DISPLAY_MAX_LINES][30];

uint32_t A_Axis_TargetPosition = 0;
uint32_t Z_Axis_TargetPosition = 0;
uint32_t a_encoder_start = 0;
uint32_t a_encoder_end = 0;
uint32_t z_encoder_start = 0;
uint32_t z_encoder_end = 0;

void A_Axis_ReferenceRun(ad5684_dac_t *dac) {
    display_jazz_clear(&display1);
    display_jazz_write_string_5x7(&display1, 0, "A-Achse Referenz");
    HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren

    // Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 3.0f, a_mot); // Richtung: Uhrzeigersinn auf Position 0
    HAL_Delay(2500);

    // Überwache den Drucksensor
    uint16_t druck_sen_value = ADC_Drucksensor(&hadc1);
    if (druck_sen_value > 20) {
        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
        HAL_Delay(100);
        return; // Funktion abbrechen
    }

    a_encoder_start = Encoder_GetPosition_A_AXIS(); // Anfangsposition speichern

    // Schritt 2: Motor in entgegengesetzter Richtung (2V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 2.0f, a_mot); // Richtung: Gegen Uhrzeigersinn auf Position 24000
    HAL_Delay(2500);

    // Überwache den Drucksensor erneut
    druck_sen_value = ADC_Drucksensor(&hadc1);
    if (druck_sen_value > 20) {
        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
        HAL_Delay(100);
        return; // Funktion abbrechen
    }

    a_encoder_end = Encoder_GetPosition_A_AXIS(); // Endposition speichern
    HAL_Delay(100);

    // Schritt 3: Mitte berechnen
    A_Axis_TargetPosition = (a_encoder_start + a_encoder_end) / 2;

    // Motor stoppen
    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
    HAL_Delay(100);
}


void Z_Axis_ReferenceRun(ad5684_dac_t *dac, bool* success) {


	uint16_t nadel_oben;
	uint16_t z_ax_no_pos = 0;
//	uint16_t no_sen_status = 0;
	uint32_t start_tick = 0;

	*success = true;
	display_jazz_clear(&display1);
	display_jazz_write_string_5x7(&display1, 0, "Z-Achse Referenz");

	HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);// Relais aktivieren

	ad5684_set_voltage(dac, 2.8f, z_mot); // Schritt 1: Motor nach unten (2,8V)
	ad5684_set_voltage(dac, 2.8f, d_mot); // Druck gegen Drucksensor (2,8V)
	HAL_Delay(500);
	z_encoder_start = Encoder_GetPosition_Z_AXIS(); // Anfangsposition speichern

	start_tick = HAL_GetTick();
	ad5684_set_voltage(dac, 2.1f, z_mot); // Schritt 2: Motor nach oben (2,0V)
	HAL_Delay(100);

	bool nadel_oben_reached = false;
	while(HAL_GetTick() < start_tick + 2000) {
		nadel_oben = ADC_Nadel_Oben(&hadc1);
		if(nadel_oben < 3845){
			z_ax_no_pos = Encoder_GetPosition_Z_AXIS(); // Position des Z-Achse, beim erreichen der Nadel Oben Position
			display_jazz_write_string_5x7(&display1, 0, "NO-Sen.: OK");
			nadel_oben_reached = true;
			break;
		}

	}

	if (!nadel_oben_reached) {
		display_jazz_write_string_5x7(&display1, 0, "NO-Sen.: ERR");
		*success = false;
		return;
	}

	HAL_Delay(1000);

	z_encoder_end = Encoder_GetPosition_Z_AXIS(); // Endposition speichern

	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);	// Schritt 3: Motor stoppen
    if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 500)) { // Innerhalb von 500 Schritten zum Startwert
        ad5684_set_voltage(dac, 2.3f, z_mot); // Langsamer fahren
    } else if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 200)) { // Sehr nahe am Startwert
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot); // Minimale Spannung für sanfte Annäherung
    }

	HAL_Delay(200);
	// Schritt 4: Mitte berechnen
	Z_Axis_TargetPosition = z_ax_no_pos	;	//(z_encoder_start + z_encoder_end) / 2;

	*success = true;
}

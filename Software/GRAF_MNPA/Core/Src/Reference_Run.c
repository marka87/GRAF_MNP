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
uint32_t z_ax_no_pos = 0; // Encoder-Position beim Erreichen des Nadel-oben-Pins

void A_Axis_ReferenceRun(ad5684_dac_t *dac, bool *success) {
	uint16_t druck_sen_value = ADC_Drucksensor(&hadc1);
	uint32_t start_tick = 0;
	*success = true;

	display_jazz_clear(&display1);
	display_jazz_write_string_5x7(&display1, 0, "A-Achse Referenz");
	HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);	// Relais aktivieren

	ad5684_set_voltage(dac, 3.0f, a_mot); 	// Schritt 1: Motor im Uhrzeigersinn (3V) drehen
	start_tick = HAL_GetTick();
	bool drucksensor_error = false;

	// Überprüfe Drucksensor während der Bewegung
	while (HAL_GetTick() < (start_tick + 2000)) {
		if (druck_sen_value > 50) {
			// Fehler: Drucksensor ausgelöst
			display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
			ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
			*success = false;
			return;
		}
	}
	// Startposition speichern
	a_encoder_start = Encoder_GetPosition_A_AXIS();
	display_jazz_write_string_5x7(&display1, 1, "Start.Pos: OK");
	// Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
	HAL_Delay(100);
	// Schritt 2: Motor gegen den Uhrzeigersinn (2V) drehen
	ad5684_set_voltage(dac, 2.0f, a_mot);
	start_tick = HAL_GetTick();
	// Überprüfe Drucksensor während der Bewegung
	while (HAL_GetTick() < (start_tick + 2000)) {
		if (druck_sen_value > 50) {
			// Fehler: Drucksensor ausgelöst
			display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
			ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
			*success = false;
			return;
		}
	}
	// Endposition speichern
	a_encoder_end = Encoder_GetPosition_A_AXIS();
	display_jazz_write_string_5x7(&display1, 2, "End.Pos: OK");
	// Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
	HAL_Delay(100);
	// Schritt 3: Mitte berechnen
	A_Axis_TargetPosition = (a_encoder_start + a_encoder_end) / 2;
	display_jazz_write_string_5x7(&display1, 3, "Referenzlauf OK");
	*success = true;
}

void Z_Axis_ReferenceRun(ad5684_dac_t *dac, bool *success) {
	uint16_t druck_sen_value = ADC_Drucksensor(&hadc1);

	uint32_t start_tick = 0;

	*success = true;
	display_jazz_clear(&display1);
	display_jazz_write_string_5x7(&display1, 0, "Z-Achse Referenz");

	HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren

	// Schritt 1: Motor nach unten (2,8V)
	ad5684_set_voltage(dac, 2.6f, z_mot);
	// Druck gegen Drucksensor (2,8V)
//	ad5684_set_voltage(dac, 2.8f, d_mot);
	HAL_Delay(500);

	// Anfangsposition speichern
	z_encoder_start = Encoder_GetPosition_Z_AXIS();

	// Schritt 2: Motor nach oben (2,1V)
	start_tick = HAL_GetTick();
	ad5684_set_voltage(dac, 2.0f, z_mot);
	HAL_Delay(100);

	bool nadel_oben_reached = false;
	// Wir warten max. 2 Sekunden darauf, dass "Nadel oben" LOW wird
	while (HAL_GetTick() < (start_tick + 2000)) {
		// Prüfe, ob Sensor-Pin LOW ist (oder HIGH, je nach Verschaltung)
		if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) {
			// Nadel oben erreicht
			z_ax_no_pos = Encoder_GetPosition_Z_AXIS(); // Encoder-Position speichern
			display_jazz_write_string_5x7(&display1, 0, "NO-Sen.: OK");
			nadel_oben_reached = true;
			break;
		}
	}

	if (!nadel_oben_reached) {
		display_jazz_write_string_5x7(&display1, 0, "NO-Sen.: ERR");
		ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);
		*success = false;
		return;
	}

	HAL_Delay(1000);

	// Endposition speichern
	z_encoder_end = Encoder_GetPosition_Z_AXIS();

	// Schritt 3: Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);

	// ggf. langsamer fahren oder Feinposition
	if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 500)) {
		ad5684_set_voltage(dac, 2.4f, z_mot); // Langsam
	} else if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 200)) {
		ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot); // Minimal
	}

	HAL_Delay(200);

	// Schritt 4: Mitte / oder Position wählen
	Z_Axis_TargetPosition = (z_encoder_start + z_encoder_end) / 2; // // (z_encoder_start + z_encoder_end) / 2 ;z_ax_no_pos + 50;

	*success = true;
}

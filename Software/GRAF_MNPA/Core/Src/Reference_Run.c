/*
 * Reference_Run.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#include "Reference_Run.h"

#include <main.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stm32f746xx.h>
#include <stm32f7xx_hal.h>
#include <stm32f7xx_hal_adc.h>
#include <stm32f7xx_hal_gpio.h>
#include <sys/_stdint.h>

#include "encoder.h"

#define a_mot 0x01 // Address for DAC-A...
#define z_mot 0x02 // Address for DAC-B...
#define d_mot 0x04 // Address for DAC-C...

#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define ENCODER_TOLERANCE 100
extern ADC_HandleTypeDef hadc1;
extern display_info_t display1;
//extern char display_buffer[DISPLAY_MAX_LINES][30];

uint32_t A_Axis_TargetPosition = 0;
uint32_t Z_Axis_TargetPosition = 0;
int32_t a_encoder_start = 0;
int32_t a_encoder_end = 0;
int32_t z_encoder_start = 0;
int32_t z_encoder_end = 0;
int32_t z_ax_no_pos = 0; // Encoder-Position beim Erreichen des Nadel-oben-Pins

a_axis_ref_result_t a_axis_last_ref_result = A_REF_OK;
uint16_t a_axis_trip_adc_val = 0;

void A_Axis_ReferenceRun(ad5684_dac_t *dac, bool *success) {
	uint16_t druck_sen_value = ADC_Drucksensor(&hadc1);
	uint32_t start_tick = 0;
	*success = true;
	a_axis_last_ref_result = A_REF_OK;
	a_axis_trip_adc_val = 0;

	display_jazz_clear(&display1);
	display_jazz_write_string_5x7(&display1, 0, "A-Achse Referenz");
	HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);// Relais aktivieren

	ad5684_set_voltage(dac, 3.0f, a_mot); // Schritt 1: Motor im Uhrzeigersinn (3V) drehen
	start_tick = HAL_GetTick();

	// Überprüfe Drucksensor während der Bewegung
	while (HAL_GetTick() < (start_tick + 2000)) {
		druck_sen_value = ADC_Drucksensor(&hadc1);
		if (druck_sen_value > 100) {
			// Fehler: Drucksensor ausgelöst -> Not-Stopp und sofortiger Abbruch
			display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
			display_jazz_write_string_5x7(&display1, 2, "Hebel einstellen");
			ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
			*success = false;
			a_axis_last_ref_result = A_REF_ERR_DRUCKSENSOR;
			a_axis_trip_adc_val = druck_sen_value;
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
		druck_sen_value = ADC_Drucksensor(&hadc1);
		if (druck_sen_value > 100) {
			// Fehler: Drucksensor ausgelöst -> Not-Stopp und sofortiger Abbruch
			display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
			display_jazz_write_string_5x7(&display1, 2, "Hebel einstellen");
			ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
			*success = false;
			a_axis_last_ref_result = A_REF_ERR_DRUCKSENSOR;
			a_axis_trip_adc_val = druck_sen_value;
			return;
		}
	}
	// Endposition speichern
	a_encoder_end = Encoder_GetPosition_A_AXIS();
	display_jazz_write_string_5x7(&display1, 2, "End.Pos: OK");
	// Motor stoppen
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
	HAL_Delay(100);

	// Schritt 3: Plausibilitaetspruefung - Hat sich der Motor ueberhaupt bewegt?
	int32_t a_stroke = labs(a_encoder_end - a_encoder_start);
	if (a_stroke < 30) {
		display_jazz_write_string_5x7(&display1, 1, "ERR. A-MOTOR");
		display_jazz_write_string_5x7(&display1, 2, "Kabel/Hub pruefen");
		ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
		*success = false;
		a_axis_last_ref_result = A_REF_ERR_NO_MOVEMENT;
		return;
	}

	// Schritt 4: Mitte berechnen
	A_Axis_TargetPosition = (a_encoder_start + a_encoder_end) / 2;
	display_jazz_write_string_5x7(&display1, 3, "Referenzlauf OK");
	a_axis_last_ref_result = A_REF_OK;
}

void Z_Axis_ReferenceRun(ad5684_dac_t *dac, bool *success) {
	uint32_t start_tick = 0;
	*success = true;
	display_jazz_clear(&display1);
	display_jazz_write_string_5x7(&display1, 0, "Z-Achse Referenz");
	HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren

	// Schritt 1: Motor nach unten (2,8V) zum mechanischen Endanschlag fahren
	ad5684_set_voltage(dac, 2.8f, z_mot);
	HAL_Delay(1200);

	// Unteren mechanischen Anschlag als absoluten Nullpunkt (0) kalibrieren
	Encoder_Reset_Z_AXIS();
	z_encoder_start = 0;

	// Schritt 2: Motor nach oben (2,0V)
	start_tick = HAL_GetTick();
	ad5684_set_voltage(dac, 2.0f, z_mot);

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

	HAL_Delay(500);

	// Endposition (oberer harter Anschlag) speichern
	z_encoder_end = Encoder_GetPosition_Z_AXIS();

	// Hub-Plausibilitaetspruefung: Mindestens 2000 Inc Hub und NO-Sensor plausibel
	if (z_encoder_end < 2000 || z_ax_no_pos < 1000 || z_ax_no_pos > z_encoder_end) {
		display_jazz_write_string_5x7(&display1, 0, "Z-Ref: HUB-ERR");
		ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);
		*success = false;
		return;
	}

	// Schritt 3: Motor stoppen bzw. sanfte Haltespannung nach oben
	ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);
	if (Encoder_GetPosition_Z_AXIS() > 500) {
		ad5684_set_voltage(dac, 2.4f, z_mot); // Sanft halten
	}

	HAL_Delay(100);

	// Schritt 4: Standby-Zielposition setzen
	Z_Axis_TargetPosition = (uint32_t)(z_ax_no_pos + 50);

}

/*
 * PID_Control.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */

#include "PID_Control.h"
#include "encoder.h"
#include "Reference_Run.h"
#include "AD5684RARUZ.h"
#include <stdlib.h>
#include <stdio.h>

/* PID-Parameter */
#define KP 0.001f
#define KI 0.002f
#define KD 0.00001f

/* Limitierung für den Integral-Anteil */
#define INTEGRAL_LIMIT  50.0f

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     2.0f
#define VOLTAGE_MAX     3.0f
#define NEUTRAL_VOLTAGE 2.5f

/* Toleranzbereich (derzeit auskommentiert) */
#define POSITION_TOLERANCE 100

/* DAC-Adresse */
#define A_MOT 0x01

/* Abtastzeit: 10 ms (Timer-Intervall) */
#define DT 0.01f

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;

/* Skalierungsfaktor für den Output (optional) */
static float scale_factor = 1.0f;  // Beispiel: 1.0f = keine Skalierung

void A_Axis_PIDControl(ad5684_dac_t *dac, uint32_t A_Axis_TargetPosition) {
	/* Istwert aus Encoder */
	int encoder_value = Encoder_GetPosition_A_AXIS();

	/* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
	int error = encoder_value - A_Axis_TargetPosition;
	/* Integral-Anteil mit Zeitbezug */
	integral += error * DT;

	/* Integralbegrenzung */
	if (integral > INTEGRAL_LIMIT)
		integral = INTEGRAL_LIMIT;
	if (integral < -INTEGRAL_LIMIT)
		integral = -INTEGRAL_LIMIT;

	/* Differential-Anteil mit Zeitbezug */
	float derivative = (error - previous_error) / DT;

	/* PID-Berechnung */
	float output = (KP * error) + (KI * integral) + (KD * derivative);

	/* Skalierung anwenden (optional) */
	output *= scale_factor;

	/* Spannung berechnen */
	float voltage = NEUTRAL_VOLTAGE + output;

	/* Begrenzen der Spannung */
	if (voltage < VOLTAGE_MIN)
		voltage = VOLTAGE_MIN;
	if (voltage > VOLTAGE_MAX)
		voltage = VOLTAGE_MAX;

	/* Spannung an den DAC senden */
	ad5684_set_voltage(dac, voltage, A_MOT);

	/* Debug-Ausgabe */
	printf(
			"Encoder: %d, Error: %d, Integral: %.2f, Deriv: %.2f, Out: %.6f, Voltage: %.3f\n",
			encoder_value, error, integral, derivative, output, voltage);

	/* Fehler für den nächsten Zyklus speichern */
	previous_error = (float) error;
}

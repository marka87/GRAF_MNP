/* Z_PID_Control.c
 *
 *  	Created on: Jan 3, 2025
 *      Author: Mark Angyal
 *      Z-Achse PID-Regelung
 */

#include "Z_PID_Control.h"

#include <sys/_stdint.h>

#include "encoder.h"

#define KP_DEFAULT 0.0030f
#define KI_DEFAULT 0.000001f
#define KD_DEFAULT 0.018000f

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;
static float z_kp = KP_DEFAULT;
static float z_ki = KI_DEFAULT;
static float z_kd = KD_DEFAULT;
static uint32_t last_target = 0u;
float voltage;

void Z_Axis_PIDControl(ad5684_dac_t *dac, uint32_t Z_Axis_TargetPosition) {
	/* Istwert aus Encoder */
	int encoder_value = Encoder_GetPosition_Z_AXIS();
	/* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
	int error = encoder_value - (int)Z_Axis_TargetPosition;
	if (Z_Axis_TargetPosition != last_target) {
		previous_error = 0.0f;
		last_target = Z_Axis_TargetPosition;
	}
	// Integralanteil
	integral += (float)error; // * DT
	float integral_limit = (z_ki > 0.0f) ? (2.5f / z_ki) : 0.0f;
    if (integral > integral_limit) integral = integral_limit;
    if (integral < -integral_limit) integral = -integral_limit;
	/* Differentialanteil */
	float derivative = (float)(error - previous_error); // / DT;

	/* PID-Berechnung */
	float output = (z_kp * (float)error) + (z_ki * integral) + (z_kd * derivative);

	/* Spannung berechnen */
	voltage = NEUTRAL_VOLTAGE + output;

	/* Begrenzen der Spannung */
	if (voltage < VOLTAGE_MIN)
		voltage = VOLTAGE_MIN;
	if (voltage > VOLTAGE_MAX)
		voltage = VOLTAGE_MAX;

	/* Spannung an den DAC senden */
	ad5684_set_voltage(dac, voltage, z_mot);

	/* Fehler für den nächsten Zyklus speichern */
	previous_error = (float) error;
}

void Z_PID_SetParameters(float kp, float ki, float kd) {
	if (kp > 0.0f && ki > 0.0f && kd >= 0.0f) {
		z_kp = kp;
		z_ki = ki;
		z_kd = kd;
		integral = 0.0f;
		previous_error = 0.0f;
	}
}

void Z_PID_GetParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = z_kp;
	if (ki != NULL) *ki = z_ki;
	if (kd != NULL) *kd = z_kd;
}

void Z_PID_EmergencyNeutral(ad5684_dac_t *dac) {
	integral = 0.0f;
	previous_error = 0.0f;
	voltage = NEUTRAL_VOLTAGE;
	ad5684_set_voltage(dac, voltage, z_mot);
}

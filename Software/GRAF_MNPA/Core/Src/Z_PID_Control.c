/* Z_PID_Control.c
 *
 *  	Created on: Jan 3, 2025
 *      Author: Mark Angyal
 *      Z-Achse PID-Regelung
 */

#include "Z_PID_Control.h"

#include <sys/_stdint.h>

#include "encoder.h"

//#define KP 0.007f			//Wow
//#define KI 0.00007f
//#define KD 0.075f

//#define KP 0.0005f			//Sanft für ablauf
//#define KI 0.00000016f
//#define KD 0.000012f

//#define KP 0.001f			//Sanft für ablauf
//#define KI 0.000029f
//#define KD 0.000031f

//#define KP 0.00047f			//Testlauf
//#define KI 0.00000151f
//#define KD 0.00012f
#define KP 0.0030f			//Testlauf
#define KI 0.000001f//KI 0.000001f
#define KD 0.018000f //KD 0.012f

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f
//#define INTEGRAL_LIMIT  10000.0f

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;
float voltage;

void Z_Axis_PIDControl(ad5684_dac_t *dac, uint32_t Z_Axis_TargetPosition) {
	/* Istwert aus Encoder */
	int encoder_value = Encoder_GetPosition_Z_AXIS();
	/* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
	int error = encoder_value - Z_Axis_TargetPosition;
	// Integralanteil
	integral += error; // * DT
//    if (integral > INTEGRAL_LIMIT) integral = INTEGRAL_LIMIT;
//    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;
	/* Differentialanteil */
	float derivative = (error - previous_error); // / DT;

	/* PID-Berechnung */
	float output = (KP * error) + (KI * integral) + (KD * derivative);

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


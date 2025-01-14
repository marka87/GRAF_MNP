
/* Z_PID_Control.c
 *
 *  Created on: Jan 3, 2025
 *      Author: Mark
 */

#include "Z_PID_Control.h"
#include "encoder.h"
#include "Reference_Run.h"
#include "AD5684RARUZ.h"
#include <stdlib.h>
#include <stdio.h>
//#define KP 0.0006f	//0.0006f Aggressive
//#define KI 0.0005f	//0.000067f
//#define KD 0.00005f 	//0.0000066f
/* PID-Parameter */
#define KP 0.0006f		//Mitte
#define KI 0.001f
#define KD 0.00001f

//#define KP 0.00065f	//Sanft
//#define KI 0.000065f
//#define KD 0.0000065f

/* Limitierung für den Integral-Anteil */
#define INTEGRAL_LIMIT  10000.0f //10000 aggressive

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.25f

/* Toleranzbereich */
#define POSITION_TOLERANCE 1

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...


/* Abtastzeit: n ms (Timer-Intervall) */
#define DT 0.001f

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;
float voltage;

/* Skalierungsfaktor für den Output (optional) */
static float scale_factor = 1.0f;  // Beispiel: 1.0f = keine Skalierung

void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition)
{
    /* Istwert aus Encoder */
    int encoder_value = Encoder_GetPosition_Z_AXIS();

    /* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
    int error = encoder_value - Z_Axis_TargetPosition;

    /* -- Optional: Toleranz-Prüfung --*/
    if (abs(error) <= POSITION_TOLERANCE) {
        integral = 0.02f;
    }
    // Integralanteil
    integral += error * DT;

    /* Integralbegrenzung */
    if (integral > INTEGRAL_LIMIT)  integral = INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    /* Differentialanteil */
    float derivative = (error - previous_error) / DT;

    /* PID-Berechnung */
    float output = (KP * error) + (KI * integral) + (KD * derivative);

    /* Skalierung anwenden (optional) */
    output *= scale_factor;

    /* Spannung berechnen */
    voltage = NEUTRAL_VOLTAGE + output;

    /* Begrenzen der Spannung */
    if (voltage < VOLTAGE_MIN)  voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX)  voltage = VOLTAGE_MAX;

    /* Spannung an den DAC senden */
    ad5684_set_voltage(dac, voltage, z_mot);

    /* Fehler für den nächsten Zyklus speichern */
    previous_error = (float)error;
}


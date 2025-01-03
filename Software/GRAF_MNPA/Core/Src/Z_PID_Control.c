/*
 * Z_PID_Control.c
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

/* PID-Parameter */
#define KP 0.0008f
#define KI 0.0007f
#define KD 0.000004f

/* Limitierung für den Integral-Anteil */
#define INTEGRAL_LIMIT  100.0f

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f

/* Toleranzbereich (derzeit auskommentiert) */
#define POSITION_TOLERANCE 1

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...


/* Abtastzeit: 10 ms (Timer-Intervall) */
#define DT 0.01f

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;

/* Skalierungsfaktor für den Output (optional) */
static float scale_factor = 1.0f;  // Beispiel: 1.0f = keine Skalierung

void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition)
{
    /* Istwert aus Encoder */
    int encoder_value = Encoder_GetPosition_Z_AXIS();

    /* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
    int error = encoder_value - Z_Axis_TargetPosition;

    /* -- Optional: Toleranz-Prüfung auskommentiert --
    if (abs(error) <= POSITION_TOLERANCE) {
        // Motor stoppen
        ad5684_set_voltage(dac, NEUTRAL_VOLTAGE, A_MOT);
        integral = 0.0f;
        previous_error = 0.0f;
        return;
    }
    */

    /* Integral-Anteil mit Zeitbezug */
    integral += error * DT;

    /* Integralbegrenzung */
    if (integral > INTEGRAL_LIMIT)  integral = INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    /* Differential-Anteil mit Zeitbezug */
    float derivative = (error - previous_error) / DT;

    /* PID-Berechnung */
    float output = (KP * error) + (KI * integral) + (KD * derivative);

    /* Skalierung anwenden (optional) */
    output *= scale_factor;

    /* Spannung berechnen */
    float voltage = NEUTRAL_VOLTAGE + output;

    /* Begrenzen der Spannung */
    if (voltage < VOLTAGE_MIN)  voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX)  voltage = VOLTAGE_MAX;

    /* Spannung an den DAC senden */
    ad5684_set_voltage(dac, voltage, z_mot);

    /* Debug-Ausgabe */
    printf("Encoder: %d, Error: %d, Integral: %.2f, Deriv: %.2f, Out: %.6f, Voltage: %.3f\n",
           encoder_value, error, integral, derivative, output, voltage);

    /* Fehler für den nächsten Zyklus speichern */
    previous_error = (float)error;
}



/* Z_PID_Control.c
 *
 *  	Created on: Jan 3, 2025
 *      Author: Mark Angyal
 *      Z-Achse PID-Regelung
 */

#include "Z_PID_Control.h"
#include "encoder.h"
#include "Reference_Run.h"
#include "AD5684RARUZ.h"
#include <stdlib.h>
#include <stdio.h>


#define KP 0.00045f		//Mitte
#define KI 0.00000151f
#define KD 0.00000012f

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;
float voltage;

void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition)
{
    /* Istwert aus Encoder */
    int encoder_value = Encoder_GetPosition_Z_AXIS();

    /* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
    int error = encoder_value - Z_Axis_TargetPosition;

    // Integralanteil
    integral += error; // * DT

    /* Differentialanteil */
    float derivative = (error - previous_error); // / DT;

    /* PID-Berechnung */
    float output = (KP * error) + (KI * integral) + (KD * derivative);

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


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

//#define KP 0.007f			//Wow
//#define KI 0.00006f
//#define KD 0.05f

#define KP 0.0007f			//Wow light
#define KI 0.000006f
#define KD 0.005f

//#define KP 0.00045f		//Sanft
//#define KI 0.00000151f
//#define KD 0.00000012f

//#define KP 0.00045f			//Thomas
//#define KI 0.00000151f
//#define KD 0.00012f

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

///* Z_PID_Control.c
// *
// *   	Created on: Jan 3, 2025
// *      Author: Mark Angyal
// *      Z-Achse PID-Regelung
// */
//
//#include "Z_PID_Control.h"
//#include "encoder.h"
//#include "Reference_Run.h"
//#include "AD5684RARUZ.h"
//#include <stdlib.h>
//#include <stdio.h>
//#define KP_HOLD 0.007f      // Wow
//#define KI_HOLD 0.00007f
//#define KD_HOLD 0.075f
//
//#define KP_MOVE 0.00045f    // Sanft
//#define KI_MOVE 0.00000151f
//#define KD_MOVE 0.00000012f
//
///* Spannungsgrenzen und Neutralspannung */
//#define VOLTAGE_MIN     0.0f
//#define VOLTAGE_MAX     5.0f
//#define NEUTRAL_VOLTAGE 2.5f
//
///* DAC-Adresse */
//#define z_mot 0x02      // DAC-B...
//
///* Statische Variablen für PID-Zustände */
//static float integral = 0.0f;
//static float previous_error = 0.0f;
//float voltage;
//
///* Toleranzbereich für den Haltemodus */
//#define POSITION_TOLERANCE 10
//
//void Z_Axis_Control(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition, bool holding)
//{
//    /* Wähle die PID-Parameter basierend auf dem Modus */
//    float kp = holding ? KP_HOLD : KP_MOVE;
//    float ki = holding ? KI_HOLD : KI_MOVE;
//    float kd = holding ? KD_HOLD : KD_MOVE;
//
//    /* Istwert aus Encoder */
//    int encoder_value = Encoder_GetPosition_Z_AXIS();
//
//    /* Fehlerberechnung */
//    int error = encoder_value - Z_Axis_TargetPosition;
//
//    /* Integralanteil */
//    integral += error;
//
////    /* Integralbegrenzung */
////    if (integral > 1000.0f) integral = 1000.0f;
////    if (integral < -1000.0f) integral = -1000.0f;
//
//    /* Differentialanteil */
//    float derivative = (error - previous_error);
//
//    /* PID-Berechnung */
//    float output = (kp * error) + (ki * integral) + (kd * derivative);
//
//    /* Spannung berechnen */
//    voltage = NEUTRAL_VOLTAGE + output;
//
//    /* Begrenzen der Spannung */
//    if (voltage < VOLTAGE_MIN)  voltage = VOLTAGE_MIN;
//    if (voltage > VOLTAGE_MAX)  voltage = VOLTAGE_MAX;
//
//    /* Spannung an den DAC senden */
//    ad5684_set_voltage(dac, voltage, z_mot);
//
//    /* Fehler für den nächsten Zyklus speichern */
//    previous_error = (float)error;
//}
//
//void Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition)
//{
//    /* Bestimme, ob wir im Haltemodus sind */
//    bool holding = abs(Z_Axis_TargetPosition - Encoder_GetPosition_Z_AXIS()) < 350;
//
//    /* Rufe die Steuerungsfunktion auf */
//    Z_Axis_Control(dac, Z_Axis_TargetPosition, holding);
//}
//

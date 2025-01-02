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

#define KP 0.00005f
#define KI 0.00001f
#define KD 0.00001f
#define INTEGRAL_LIMIT 1000.0f
#define VOLTAGE_MIN 0.0f
#define VOLTAGE_MAX 5.0f
#define POSITION_TOLERANCE 10 // Toleranzbereich um die Zielposition

static float integral = 0.0f;
static float previous_error = 0.0f;

void A_Axis_PIDControl(ad5684_dac_t* dac, uint32_t target_position) {
    int encoder_value = Encoder_GetPosition_A_AXIS();
    int error = encoder_value - target_position;

    if (abs(error) <= POSITION_TOLERANCE) {
        error = 0; // Fehler auf 0 setzen, wenn innerhalb der Toleranz
        integral = 0; // Integralanteil zurücksetzen
    }
    // Integralanteil berechnen und begrenzen
    integral += error;
    if (integral > INTEGRAL_LIMIT) integral = INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    // Differenzialanteil berechnen
    float derivative = error - previous_error;

    // PID-Ausgabe berechnen
    float output = KP * error + KI * integral + KD * derivative;
    float voltage = 2.5f + output;

    // Begrenzen der Spannung
    if (voltage < VOLTAGE_MIN) voltage = VOLTAGE_MIN;
    if (voltage > VOLTAGE_MAX) voltage = VOLTAGE_MAX;

    // Spannung an den DAC senden
    ad5684_set_voltage(dac, voltage, 0x01);

    // Fehler für nächsten Schritt speichern
    previous_error = error;
}

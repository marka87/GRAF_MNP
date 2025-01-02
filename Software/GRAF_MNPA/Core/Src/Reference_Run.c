/*
 * Reference_Run.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */


#include "main.h"
#include "AD5684RARUZ.h"
#include "encoder.h"
#include "Reference_Run.h"
#include <stm32f7xx_hal_tim.h>
#include "PID_Control.h"

#define a_mot 0x01 // Address for DAC-A...
#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define ENCODER_TOLERANCE 10
uint32_t timeout = 0;

void A_Axis_ReferenceRun(ad5684_dac_t* dac) {
    uint32_t encoder_start = 0;
    uint32_t encoder_end = 0;
    uint32_t encoder_mid = 0;

    // Relais aktivieren
    HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);

    // Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 3.0f, a_mot);	    // Richtung: Uhrzeigersinn auf position 0
    HAL_Delay(3000);
    encoder_start = Encoder_GetPosition_A_AXIS(); // Anfangsposition speichern

    // Schritt 2: Motor in entgegengesetzter Richtung (2V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 2.0f, a_mot); 	    // Richtung: Gegen Uhrzeigersinn auf position 24000
    HAL_Delay(3000);
    encoder_end = Encoder_GetPosition_A_AXIS(); // Endposition speichern
    HAL_Delay(50);
    // Motor stoppen
    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
    HAL_Delay(500);

    // Schritt 3: Mitte berechnen
    encoder_mid = (encoder_start + encoder_end) / 2;

    // Schritt 4: Zur Mitte fahren
    while (abs((int)(Encoder_GetPosition_A_AXIS() - encoder_mid)) > ENCODER_TOLERANCE) {
            if (Encoder_GetPosition_A_AXIS() > encoder_mid) {
                ad5684_set_voltage(dac, 3.0f, a_mot); // Richtung: Uhrzeigersinn
            } else {
                ad5684_set_voltage(dac, 2.0f, a_mot); // Richtung: Gegen Uhrzeigersinn
            }
            HAL_Delay(10);
            timeout += 10;

            if (timeout > 10000) { // Timeout nach 10 Sekunden
                printf("Timeout: Motor erreicht die Mitte nicht\n");
                break;
            }
    // Motor stoppen
    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);

    // Schritt 5: PID-Regelung aktivieren
    A_Axis_PIDControl(dac, encoder_mid);

	}
}

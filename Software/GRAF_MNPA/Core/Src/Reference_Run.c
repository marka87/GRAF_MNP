/*
 * Reference_Run.c
 *
 *  Created on: Jan 2, 2025
 *      Author: Mark
 */
#include "Reference_Run.h"
#include "main.h"
#include "display.h"

#include "AD5684RARUZ.h"
#include "encoder.h"
#include <stm32f7xx_hal_tim.h>
#include "PID_Control.h"
#include <stdlib.h>
#include <stdio.h>
#define a_mot 0x01 // Address for DAC-A...
#define z_mot 0x02 // Address for DAC-B...
#define d_mot 0x04 // Address for DAC-C...

#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define ENCODER_TOLERANCE 100
extern ADC_HandleTypeDef hadc1;
extern display_info_t display1;
extern char display_buffer[DISPLAY_MAX_LINES][30];

uint32_t A_Axis_TargetPosition = 0;
uint32_t Z_Axis_TargetPosition = 0;
uint32_t a_encoder_start = 0;
uint32_t a_encoder_end = 0;
uint32_t z_encoder_start = 0;
uint32_t z_encoder_end = 0;

//void A_Axis_ReferenceRun(ad5684_dac_t *dac) {
//    display_jazz_clear(&display1);
//    display_jazz_write_string_5x7(&display1, 0, "A-Achse Referenz");
//    HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren
//
//    // Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
//    ad5684_set_voltage(dac, 3.0f, a_mot); // Richtung: Uhrzeigersinn auf Position 0
//    HAL_Delay(2500);
//
//    // Überwache den Drucksensor
//    uint16_t druck_sen_value = ADC_Drucksensor(&hadc1);
//    if (druck_sen_value > 20) {
//        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
//        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
//        HAL_Delay(100);
//        return; // Funktion abbrechen
//    }
//
//    a_encoder_start = Encoder_GetPosition_A_AXIS(); // Anfangsposition speichern
//
//    // Schritt 2: Motor in entgegengesetzter Richtung (2V) für 2,5 Sekunden
//    ad5684_set_voltage(dac, 2.0f, a_mot); // Richtung: Gegen Uhrzeigersinn auf Position 24000
//    HAL_Delay(2500);
//
//    // Überwache den Drucksensor erneut
//    druck_sen_value = ADC_Drucksensor(&hadc1);
//    if (druck_sen_value > 20) {
//        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
//        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
//        HAL_Delay(100);
//        return; // Funktion abbrechen
//    }
//
//    a_encoder_end = Encoder_GetPosition_A_AXIS(); // Endposition speichern
//    HAL_Delay(100);
//
//    // Schritt 3: Mitte berechnen
//    A_Axis_TargetPosition = (a_encoder_start + a_encoder_end) / 2;
//
//    // Motor stoppen
//    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
//    HAL_Delay(100);
//}
void A_Axis_ReferenceRun(ad5684_dac_t *dac)
{
    display_jazz_clear(&display1);
    display_jazz_write_string_5x7(&display1, 0, "A-Achse Referenz");
    HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren

    // Schritt 1: Motor im Uhrzeigersinn (3V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 3.0f, a_mot); // Richtung: Uhrzeigersinn auf Position 0
    HAL_Delay(2500);

    // Drucksensor per ADC abfragen
    uint16_t druck_value = ADC_Drucksensor(&hadc1);
    // Beispiel: Schwellwert 3000 (nahe 5V, je nach Auflösung / Spannungsteiler)
    if (druck_value > 3000) {
        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
        HAL_Delay(100);
        return; // Funktion abbrechen
    }

    // Anfangsposition speichern
    a_encoder_start = Encoder_GetPosition_A_AXIS();

    // Schritt 2: Motor in entgegengesetzter Richtung (2V) für 2,5 Sekunden
    ad5684_set_voltage(dac, 2.0f, a_mot); // Richtung: Gegen Uhrzeigersinn
    HAL_Delay(2500);

    // Drucksensor erneut abfragen
    druck_value = ADC_Drucksensor(&hadc1);
    if (druck_value > 3000) {
        display_jazz_write_string_5x7(&display1, 1, "ERR. DRUCK-Sen");
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot); // Motor stoppen
        HAL_Delay(100);
        return; // Funktion abbrechen
    }

    // Endposition speichern
    a_encoder_end = Encoder_GetPosition_A_AXIS();
    HAL_Delay(100);

    // Schritt 3: Mitte berechnen
    A_Axis_TargetPosition = (a_encoder_start + a_encoder_end) / 2;

    // Motor stoppen
    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, a_mot);
    HAL_Delay(100);
}


void Z_Axis_ReferenceRun(ad5684_dac_t *dac, bool* success)
{
    uint32_t z_ax_no_pos = 0; // Encoder-Position beim Erreichen des Nadel-oben-Pins
    uint32_t start_tick = 0;

    *success = true;
    display_jazz_clear(&display1);
    display_jazz_write_string_5x7(&display1, 0, "Z-Achse Referenz");

    HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET); // Relais aktivieren

    // Schritt 1: Motor nach unten (2,8V)
    ad5684_set_voltage(dac, 2.6f, z_mot);
    // Druck gegen Drucksensor (2,8V)
    ad5684_set_voltage(dac, 2.6f, d_mot);
    HAL_Delay(500);

    // Anfangsposition speichern
    z_encoder_start = Encoder_GetPosition_Z_AXIS();

    // Schritt 2: Motor nach oben (2,1V)
    start_tick = HAL_GetTick();
    ad5684_set_voltage(dac, 2.05f, z_mot);
    HAL_Delay(100);

    bool nadel_oben_reached = false;
    // Wir warten max. 2 Sekunden darauf, dass "Nadel oben" LOW wird
    while (HAL_GetTick() < (start_tick + 2000))
    {
        // Prüfe, ob Sensor-Pin LOW ist (oder HIGH, je nach Verschaltung)
        if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET)
        {
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

    HAL_Delay(1000);

    // Endposition speichern
    z_encoder_end = Encoder_GetPosition_Z_AXIS();

    // Schritt 3: Motor stoppen
    ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot);

    // ggf. langsamer fahren oder Feinposition
    if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 500)) {
        ad5684_set_voltage(dac, 2.35f, z_mot); // Langsam
    }
    else if (Encoder_GetPosition_Z_AXIS() > (z_encoder_start + 200)) {
        ad5684_set_voltage(dac, TARGET_VOLTAGE_NEUTRAL, z_mot); // Minimal
    }

    HAL_Delay(200);

    // Schritt 4: Mitte / oder Position wählen
    // Hier nimmst du die Position beim Erreichen der Nadel-oben-Sensierung
    // oder eine andere Position:
    Z_Axis_TargetPosition = z_ax_no_pos + 50;  // // (z_encoder_start + z_encoder_end) / 2 ;

    *success = true;
}

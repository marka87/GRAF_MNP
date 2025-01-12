/*
 * ADC_read.c
 *
 *  Created on: Nov 29, 2024
 *      Author: Mark
 */
//#include <string.h>
//#include <stdio.h>
#include "ADC_read.h"

void ADC_Init(ADC_HandleTypeDef *adc_handle) {
    // ADC-Kalibrierung und Start
    if (HAL_ADCEx_Calibration_Start(adc_handle) != HAL_OK) {
        // Fehlerbehandlung: Kalibrierung fehlgeschlagen
    }
    if (HAL_ADC_Start(adc_handle) != HAL_OK) {
        // Fehlerbehandlung: ADC-Start fehlgeschlagen
    }
}

// Einlesen der Lichtschranke (ADC_NO_SEN_5V)
uint16_t ADC_Nadel_Oben(ADC_HandleTypeDef *adc_handle) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;                 // Kanal 2 (ADC1_IN2) für "Nadel oben"
    sConfig.Rank = ADC_REGULAR_RANK_1;               // Standard-Rank
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;   // Abtastzeit
    sConfig.Offset = 0;                              // Kein Offset

    if (HAL_ADC_ConfigChannel(adc_handle, &sConfig) != HAL_OK) {
        // Fehlerbehandlung: Kanalkonfiguration fehlgeschlagen
        return 0xFFFF; // Beispielwert für Fehler
    }

    if (HAL_ADC_Start(adc_handle) != HAL_OK) {
        // Fehlerbehandlung: ADC-Start fehlgeschlagen
        return 0xFFFF;
    }

    if (HAL_ADC_PollForConversion(adc_handle, 2000) != HAL_OK) {
        // Fehlerbehandlung: ADC-Konvertierung fehlgeschlagen
        HAL_ADC_Stop(adc_handle);
        return 0xFFFF;
    }

    uint16_t adc_value = HAL_ADC_GetValue(adc_handle);

    HAL_ADC_Stop(adc_handle); // Stoppen nach dem Lesen

    return adc_value;
}

// Einlesen des Drucksensors (ADC_DRUCK_SEN_5V)
uint16_t ADC_Drucksensor(ADC_HandleTypeDef *adc_handle) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_3;                 // Kanal 3 (ADC1_IN3) für Drucksensor
    sConfig.Rank = ADC_REGULAR_RANK_1;               // Standard-Rank
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;   // Abtastzeit
    sConfig.Offset = 0;                              // Kein Offset

    if (HAL_ADC_ConfigChannel(adc_handle, &sConfig) != HAL_OK) {
        // Fehlerbehandlung: Kanalkonfiguration fehlgeschlagen
        return 0xFFFF; // Beispielwert für Fehler
    }

    if (HAL_ADC_Start(adc_handle) != HAL_OK) {
        // Fehlerbehandlung: ADC-Start fehlgeschlagen
        return 0xFFFF;
    }

    if (HAL_ADC_PollForConversion(adc_handle, 2000) != HAL_OK) {
        // Fehlerbehandlung: ADC-Konvertierung fehlgeschlagen
        HAL_ADC_Stop(adc_handle);
        return 0xFFFF;
    }

    uint16_t adc_value = HAL_ADC_GetValue(adc_handle);

    HAL_ADC_Stop(adc_handle); // Stoppen nach dem Lesen

    return adc_value;
}


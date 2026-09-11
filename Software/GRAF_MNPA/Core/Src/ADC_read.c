/*
 * ADC_read.c
 *
 *  Created on: Nov 29, 2024
 *      Author: Mark
 */
#include "ADC_read.h"

void ADC_Init(ADC_HandleTypeDef* adc_handle) {
    // ADC-Start
    HAL_ADC_Start(adc_handle);
}

// Einlesen des Drucksensors (ADC_DRUCK_SEN_5V)
uint16_t ADC_Drucksensor(ADC_HandleTypeDef* adc_handle) {

    HAL_ADC_Start(adc_handle);                     // ADC starten
    HAL_ADC_PollForConversion(adc_handle, 5);      // Auf Abschluss warten (5 ms Timeout)
    uint16_t adc_value = HAL_ADC_GetValue(adc_handle); // Wert lesen
    return (uint16_t)adc_value;
}

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
	// ADC-Start
	HAL_ADCEx_Calibration_Start(adc_handle);
	HAL_ADC_Start(adc_handle);
}

// Einlesen der Lichtschranke (ADC_NO_SEN_5V)
float ADC_Nadel_Oben(ADC_HandleTypeDef *adc_handle) {
	ADC_ChannelConfTypeDef sConfig = { 0 };
	sConfig.Channel = ADC_CHANNEL_2;      // Kanal 2 (ADC1_IN2) für "Nadel oben"
	sConfig.Rank = ADC_REGULAR_RANK_1;             // Standard-Rank
	sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; // Abtastzeit
	HAL_ADC_ConfigChannel(adc_handle, &sConfig);   // Kanal konfigurieren

	HAL_ADC_Start(adc_handle);                     // ADC starten
	HAL_ADC_PollForConversion(adc_handle, 2000); // Auf Abschluss warten
	uint16_t value1 = HAL_ADC_GetValue(adc_handle); // Wert lesen
	return value1;             // Umrechnen in Spannung  * (5.0f / 4095.0f);

}

// Einlesen des Drucksensors (ADC_DRUCK_SEN_5V)
float ADC_Drucksensor(ADC_HandleTypeDef *adc_handle) {
	ADC_ChannelConfTypeDef sConfig = { 0 };
	sConfig.Channel = ADC_CHANNEL_3;       // Kanal 3 (ADC1_IN3) für Drucksensor
	sConfig.Rank = ADC_REGULAR_RANK_1;             // Standard-Rank
	sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; // Abtastzeit
	HAL_ADC_ConfigChannel(adc_handle, &sConfig);   // Kanal konfigurieren

	HAL_ADC_Start(adc_handle);                     // ADC starten
	HAL_ADC_PollForConversion(adc_handle, 2000); 	// Auf Abschluss warten
	uint16_t value = HAL_ADC_GetValue(adc_handle); // Wert lesen
	return value;             // Umrechnen in Spannung  * (5.0f / 4095.0f);

}


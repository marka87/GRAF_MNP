/*
 * ADC_read.c
 *
 *  Created on: Nov 29, 2024
 *      Author: Mark
 */
#include <string.h>
#include <stdio.h>
#include "ADC_read.h"

//uint32_t adc_dma_buffer[ADC_BUFFER_SIZE]; // Buffer for DMA values
//
//void ADC_Init(ADC_HandleTypeDef *hadc) {
//    // Initialize ADC
//    HAL_ADC_Init(hadc);
//
//    // Start ADC with DMA
//    HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_dma_buffer, ADC_BUFFER_SIZE);
//}
//
//float ADC_Nadel_Oben(void) {
//    uint32_t value = adc_dma_buffer[0]; // Value from DMA buffer
//    return value * (2.0f * 2.5f / 4095.0f); // Convert to voltage
//}
//
//float ADC_Drucksensor(void) {
//    uint32_t value = adc_dma_buffer[1]; // Value from DMA buffer
//    return value * (2.0f * 2.5f / 4095.0f); // Convert to voltage
//}
////DMA

//uint32_t adc_dma_buffer[2]; // Buffer für DMA-Werte
//
//void ADC_Init(ADC_HandleTypeDef *hadc) {
//
//    HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_dma_buffer, ADC_BUFFER_SIZE); // ADC mit DMA starten
//}
//
//// Einlesen der Lichtschranke (ADC_NO_SEN_5V)
//float ADC_Nadel_Oben(void) {
//    uint32_t value = adc_dma_buffer[0]; // Wert aus DMA-Puffer
//    return value * (2.0f * 2.5f / 4095.0f); // Umrechnung in Spannung
//}
//
//// Einlesen des Drucksensors (ADC_DRUCK_SEN_5V)
//float ADC_Drucksensor(void) {
//    uint32_t value = adc_dma_buffer[1]; // Wert aus DMA-Puffer
//    return value * (2.0f * 2.5f / 4095.0f); // Umrechnung in Spannung
//}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// Normal

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
	uint32_t value1 = HAL_ADC_GetValue(adc_handle); // Wert lesen
	return value1 * (2.0f * 2.5f / 4095.0f);

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
	uint32_t value = HAL_ADC_GetValue(adc_handle); // Wert lesen
	return value * (2.0f * 2.5f / 4095.0f);             // Umrechnen in Spannung

}

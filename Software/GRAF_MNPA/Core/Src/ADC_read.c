/*
 * ADC_read.c
 *
 *  Created on: Nov 29, 2024
 *      Author: Mark
 */
//#include <string.h>
//#include <stdio.h>
#include "ADC_read.h"

void ADC_Init(ADC_HandleTypeDef* adc_handle) {
    // ADC-Start
	HAL_ADCEx_Calibration_Start(adc_handle);
    HAL_ADC_Start(adc_handle);
}

// Einlesen der Lichtschranke (ADC_NO_SEN_5V)
uint16_t ADC_Drucksensor(ADC_HandleTypeDef* adc_handle) {

    HAL_ADC_Start(adc_handle);                     // ADC starten
    HAL_ADC_PollForConversion(adc_handle, 2000); // Auf Abschluss warten
    uint16_t adc_value = HAL_ADC_GetValue(adc_handle); // Wert lesen
    return (uint16_t)adc_value;
}
//// Initialisierung des ADC
//void ADC_Init(ADC_HandleTypeDef* hadc)
//{
//    // Kalibrierung
//    if (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK) {
//        Error_Handler();
//    }
//    // ADC-Start
//    if (HAL_ADC_Start(hadc) != HAL_OK) {
//        Error_Handler();
//    }
//}
//
//// Drucksensor einfach auslesen, ohne Kanal-Konfiguration
//uint16_t ADC_Drucksensor(ADC_HandleTypeDef *adc_handle)
//{
//    // ADC läuft schon (continuous oder single conversion)
//    // Hier starten wir einmal für Single-Conversion:
//    if (HAL_ADC_Start(adc_handle) != HAL_OK) {
//        return 0xFFFF; // Fehler
//    }
//
//    // Auf Ende der Konvertierung warten
//    if (HAL_ADC_PollForConversion(adc_handle, 10) != HAL_OK) {
//        HAL_ADC_Stop(adc_handle);
//        return 0xFFFF; // Fehler
//    }
//
//    // Wert auslesen
//    uint32_t adc_value = HAL_ADC_GetValue(adc_handle);
//
//    // ADC stoppen (wenn du nicht continuous arbeitest)
//    HAL_ADC_Stop(adc_handle);
//
//    return (uint16_t)adc_value;
//}


//// Einlesen des Drucksensors (ADC_DRUCK_SEN_5V)
//uint16_t ADC_Drucksensor(ADC_HandleTypeDef* hadc) {
//    // Warten auf Ende der Konvertierung
//    if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
//        // Get ADC value
//        uint32_t adc_value = HAL_ADC_GetValue(hadc);
//        return (uint16_t)adc_value;
//    }
//    // Fehlerwert
//    return 0xFFFF;
//}

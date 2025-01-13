/*
// * ADC_read.h
// *
// *  Created on: Nov 29, 2024
// *      Author: Mark
// */

#ifndef SRC_ADC_READ_H_
#define SRC_ADC_READ_H_

#include <main.h>
#include <stm32f7xx_hal_conf.h>
#include <stm32f7xx_it.h>
#include <stm32f7xx_hal_adc.h>
#define ADC_BUFFER_SIZE 2 // Anzahl der ADC-Kanäle

//void ADC_Init(ADC_HandleTypeDef *adc_handle);
//uint16_t  ADC_Nadel_Oben(ADC_HandleTypeDef *adc_handle);
//uint16_t  ADC_Drucksensor(ADC_HandleTypeDef *adc_handle);

void ADC_Init(ADC_HandleTypeDef* hadc);
uint16_t ADC_Drucksensor(ADC_HandleTypeDef* hadc);
#endif /* SRC_ADC_READ_H_ */

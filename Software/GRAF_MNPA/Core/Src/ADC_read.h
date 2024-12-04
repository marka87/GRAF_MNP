/*
 * ADC_read.h
 *
 *  Created on: Nov 29, 2024
 *      Author: Mark
 */

#ifndef SRC_ADC_READ_H_
#define SRC_ADC_READ_H_


#include <main.h>
#include <stm32f7xx_hal_conf.h>
#include <stm32f7xx_it.h>
#include <stm32f7xx_hal_adc.h>
#define ADC_BUFFER_SIZE 2 // Anzahl der ADC-Kanäle

extern uint32_t adc_dma_buffer[ADC_BUFFER_SIZE];

void ADC_Init(ADC_HandleTypeDef *hadc);
float ADC_Nadel_Oben(void);
float ADC_Drucksensor(void);
//extern uint32_t adc_dma_buffer[2];
//ADC_HandleTypeDef hadc1;
//DMA_HandleTypeDef hdma_adc1;
//void ADC_Init(ADC_HandleTypeDef *hadc);
////void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
////void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc);
//float ADC_Nadel_Oben(void);     // Funktion ohne Parameter
//float ADC_Drucksensor(void);   // Funktion ohne Parameter


//void ADC_Init(ADC_HandleTypeDef *adc_handle);
//float ADC_Nadel_Oben(ADC_HandleTypeDef* adc_handle);
//float ADC_Drucksensor(ADC_HandleTypeDef* adc_handle);


#endif /* SRC_ADC_READ_H_ */

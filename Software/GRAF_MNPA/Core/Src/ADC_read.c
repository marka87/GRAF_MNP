/*
 * ADC_read.c
 *
 * Polled single conversion of ADC1 (Pressure sensor on PA3 / ADC1_IN3).
 * Conversion takes 1.0 us (27 cycles at 27 MHz ADCCLK).
 */
#include "ADC_read.h"

uint16_t ADC_Drucksensor(ADC_HandleTypeDef* adc_handle) {
    HAL_ADC_Start(adc_handle);
    HAL_ADC_PollForConversion(adc_handle, 2);
    return (uint16_t)HAL_ADC_GetValue(adc_handle);
}


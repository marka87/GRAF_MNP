/*
 * ADC_read.c
 *
 * Direct atomic register read of ADC1 (Continuous Conversion Mode).
 * Sampling runs continuously in hardware at 1.0 us without CPU blocking.
 */
#include "ADC_read.h"

uint16_t ADC_Drucksensor(ADC_HandleTypeDef* adc_handle) {
    return (uint16_t)adc_handle->Instance->DR;
}


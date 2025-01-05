/*
 * AD5684RARUZ.h
 *
 *  Created on: Nov 4, 2024
 *      Author: anma
 */

#ifndef SRC_AD5684RARUZ_H_
#define SRC_AD5684RARUZ_H_

#include <main.h>
#include <stm32f7xx_hal_conf.h>
#include <stm32f7xx_it.h>
#include <stm32f7xx_hal_spi.h>


typedef struct {
	SPI_HandleTypeDef* spi_handle;
	GPIO_TypeDef* spi_cs_port;
	uint16_t spi_cs_pin;

} ad5684_dac_t;

void ad5684_init(ad5684_dac_t* dac);
void ad5684_set_voltage(ad5684_dac_t* dac, float voltage, uint8_t dac_channel);
#endif /* SRC_AD5684RARUZ_H_ */

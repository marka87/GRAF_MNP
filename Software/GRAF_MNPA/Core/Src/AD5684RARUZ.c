/*
 * AD5684RARUZ.c
 *
 *  Created on: Nov 4, 2024
 *      Author: anma
 */

#include <main.h>
#include "AD5684RARUZ.h"


void ad5684_init(ad5684_dac_t* dac) {
	uint8_t tx_buffer[3];
	tx_buffer[0]=0x37;	//0011 Write to and update DAC Channel n + 0111 EN_DAC A, B, C
	tx_buffer[1]=0x00;
	tx_buffer[2]=0x00;

	HAL_SPI_Transmit(dac->spi_handle, tx_buffer, 3, 2000);

	tx_buffer[0]=0x00;
	tx_buffer[1]=0x00;
	tx_buffer[2]=0x00;

	uint8_t rx_buffer[3];
	rx_buffer[0]=0x00;
	rx_buffer[1]=0x00;
	rx_buffer[2]=0x00;

	HAL_SPI_TransmitReceive(dac->spi_handle, tx_buffer, rx_buffer, 3, 2000); //Auslesen, ob richtige werte drin sind.
}

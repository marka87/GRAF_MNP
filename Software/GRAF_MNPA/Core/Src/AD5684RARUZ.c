/*
 * AD5684RARUZ.c
 *
 * x.x = Spg wert zB 2.5
 * ad5684_set_voltage(&dac, x.x, 0x01); // DAC A auf 2.5V setzen
 * ad5684_set_voltage(&dac, x.x, 0x02); // DAC B auf 3.3V setzen
 * ad5684_set_voltage(&dac, x.x, 0x04); // DAC C auf 1.2V setzen
 * Vout A	0 0 0 1 DAC A	A_AXIS_DAC - A8 pin
 * Vout B	0 0 1 0 DAC B	Z_AXIS_DAC - A18 pin
 * Vout C	0 1 0 0 DAC C	DRUCKMOTOR_DAC - A24
 *
 * 0 0 1 1 Write to and update DAC Channel
 *
 * dac_a = 0x31;
 * dac_b = 0x32;
 * dac_c = 0x34;
 *  Created on: Nov 4, 2024
 *      Author: anma
 */

#include <main.h>
#include "AD5684RARUZ.h"

void ad5684_init(ad5684_dac_t *dac) {
	uint8_t tx_buffer[3];
	tx_buffer[0] = 0x37; //0011 Write to and update DAC Channel n + 0111 EN_DAC A, B, C
	tx_buffer[1] = 0x00;
	tx_buffer[2] = 0x00;

	HAL_SPI_Transmit(dac->spi_handle, tx_buffer, 3, 2000);
}


void ad5684_set_voltage(ad5684_dac_t *dac, float voltage, uint8_t dac_channel) {
	uint16_t ad5684_data;
	uint8_t tx_buffer[3];

	ad5684_data = ((voltage / 5.0) * 65535); // Skaliere auf 16-Bit-Wert

	//Write to and Update DAC
	tx_buffer[0] = 0x30 | dac_channel; // 0011 (Write + Update) 0x30 | (oder) dac_channel wert

	tx_buffer[1] = (ad5684_data >> 8) & 0xFF; // Obere 8 Bit, nach rechts schieben
	tx_buffer[2] = ad5684_data & 0xFF;        // Untere 8 Bit

// Daten an DAC senden
	HAL_SPI_Transmit(dac->spi_handle, tx_buffer,sizeof(tx_buffer) / sizeof(uint8_t), 2000);
}



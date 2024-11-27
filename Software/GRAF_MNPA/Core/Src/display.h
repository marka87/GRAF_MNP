/*
 * display.h
 *	the header file for the display: JAZZ-AC-Y - ST7567
 *
 *  Created on: Nov 27, 2024
 *      Author: Mark Angyal
 */

#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

#include <main.h>
#include <stm32f7xx_hal_conf.h>
#include <stm32f7xx_it.h>
#include <stm32f7xx_hal_spi.h>
#include <stdbool.h>



typedef struct {
	SPI_HandleTypeDef* spi_handle;
	uint32_t lcd_width;
	uint32_t lcd_height;
	uint32_t lcd_ram_pages;
} display_info_t;



void display_jazz_init(display_info_t* disp);
void display_jazz_fillpattern(display_info_t* disp, uint8_t pattern);
void display_jazz_position(display_info_t* disp, uint8_t page, uint8_t column);
void display_jazz_write_command(display_info_t* disp, uint8_t cmd); //command senden
void display_jazz_write_data(display_info_t* disp, uint8_t data);	//data senden
void display_jazz_write_string(display_info_t* disp, uint8_t row, const char * fmt, ...);
void display_jazz_write_string_5x7(display_info_t* disp, uint32_t row, char *str);
void display_jazz_write_letter_5x7(display_info_t* disp, uint8_t row, uint8_t column, char c);
void display_jazz_clear(display_info_t* disp);
void print_graph(display_info_t* disp, uint8_t data[], uint32_t size);

//void display_jazz_draw_logo(display_info_t *disp, const uint8_t data[], uint32_t size);
//void display_jazz_draw_byte(display_info_t *disp, uint32_t x, uint32_t y);
//void display_jazz_position(display_info_t *disp, uint8_t page, uint8_t column);
//void display_jazz_clear(display_info_t *disp);
//void display_jazz_fill(display_info_t *disp);
////void refresh_display_values(disp_jazz_t *options,bool required, char display_values[][22], int8_t inverse_row);
////void refresh_display_submenu_values(disp_jazz_t *options, bool required, char display_values[][22], int8_t inverse_row);
//void refresh_display_time(display_info_t *disp, char display_values[][22]);
//
//void print_graph(display_info_t *disp, uint8_t data[], uint32_t size);






#endif /* SRC_DISPLAY_H_ */

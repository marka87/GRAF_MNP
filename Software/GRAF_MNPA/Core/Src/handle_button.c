/*
 * handle_button.c
 *
 *  Created on: Jan 17, 2025
 *      Author: Mark
 *
 *      Button handling
 */
#include "main.h"
#include "handle_button.h"
/* Button States */

uint8_t btn_up_state = 0, last_btn_up_state = 1;
uint8_t btn_down_state = 0, last_btn_down_state = 1;
uint8_t btn_ok_state = 0, last_btn_ok_state = 1;

uint8_t btn_ok_pressed = 0;
uint8_t btn_up_pressed = 0;
uint8_t btn_down_pressed = 0;

/* Handle BTN_OK */
void handle_button_ok() {
	btn_ok_state = HAL_GPIO_ReadPin(GPIOE, BTN_OK_Pin);
	if (btn_ok_state == GPIO_PIN_RESET && last_btn_ok_state == GPIO_PIN_SET) {
		btn_ok_pressed = 1;
	}
	last_btn_ok_state = btn_ok_state;
}
/* Handle BTN_UP */
void handle_button_up() {
	btn_up_state = HAL_GPIO_ReadPin(GPIOE, BTN_DOWN_Pin); //ACHTUNG, UP und DOwN vertauscht!
	if (btn_up_state == GPIO_PIN_RESET && last_btn_up_state == GPIO_PIN_SET) {
		btn_up_pressed = 1;
	}
	last_btn_up_state = btn_up_state;
}
/* Handle BTN_DOWN */
void handle_button_down() {
	btn_down_state = HAL_GPIO_ReadPin(GPIOE, BTN_UP_Pin);
	if (btn_down_state == GPIO_PIN_RESET && last_btn_down_state == GPIO_PIN_SET) {
		btn_down_pressed = 1;
	}
	last_btn_down_state = btn_down_state;
}

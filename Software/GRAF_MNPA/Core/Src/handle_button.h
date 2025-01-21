/*
 * handle_button.h
 *
 *  Created on: Jan 17, 2025
 *      Author: Mark
 */

#ifndef SRC_HANDLE_BUTTON_H_
#define SRC_HANDLE_BUTTON_H_
#include <main.h>
#include <stdint.h>

void handle_button_up(void);
void handle_button_down(void);
void handle_button_ok(void);

extern uint8_t btn_ok_pressed;
extern uint8_t btn_up_pressed;
extern uint8_t btn_down_pressed;


#endif /* SRC_HANDLE_BUTTON_H_ */

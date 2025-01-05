/*
 * Menu.h
 *
 *  Created on: Jan 5, 2025
 *      Author: Mark
 */

#ifndef SRC_MENU_H_
#define SRC_MENU_H_
// menu.h
#ifndef MENU_H
#define MENU_H

#include "stm32f7xx_hal.h"
#include "display.h"
#include "encoder.h"
#include "AD5684RARUZ.h"

// Menüzustände
typedef enum {
    MAIN_MENU,
    STANDALONE_MENU,
    PC_MODE,          // Wird später implementiert
    STANDALONE_DEMO,
    STANDALONE_KURZTEST,
    STANDALONE_DAUERTEST
} MenuState;

// Globale Variablen
extern MenuState current_menu;
extern int selection_index;

// Menüfunktionen
void display_main_menu(void);
void display_standalone_menu(void);
void navigate_main_menu(int direction);
void navigate_standalone_menu(int direction);
void select_main_menu_option(void);
void select_standalone_menu_option(void);

// Testfunktionen (Platzhalter)
void run_demo_test(ad5684_dac_t *dac);
void run_kurztest(ad5684_dac_t *dac);
void run_dauertest(ad5684_dac_t *dac);
void run_pc_mode(void);

// Button-Handling Funktion
uint8_t is_button_pressed(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint32_t* last_press_time);

#endif // MENU_H



#endif /* SRC_MENU_H_ */

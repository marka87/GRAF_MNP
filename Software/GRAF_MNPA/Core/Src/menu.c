/*
 * Menu.c
 *
 *  Created on: Jan 5, 2025
 *      Author: Mark
 */

// menu.c
#include <stdio.h>
#include "menu.h"

// Globale Variablen
MenuState current_menu = MAIN_MENU;
int selection_index = 0;

// Debouncing Variablen
#define DEBOUNCE_DELAY_MS 50
uint32_t last_btn_down_press = 0;
uint32_t last_btn_up_press = 0;
uint32_t last_btn_ok_press = 0;

// Navigation Funktionen
void navigate_main_menu(int direction) { // direction: 1 = up, -1 = down
    selection_index += direction;
    if (selection_index < 0) {
        selection_index = MAIN_MENU_OPTIONS - 1;
    } else if (selection_index >= MAIN_MENU_OPTIONS) {
        selection_index = 0;
    }
}

void navigate_standalone_menu(int direction) { // direction: 1 = up, -1 = down
    selection_index += direction;
    if (selection_index < 0) {
        selection_index = STANDALONE_MENU_OPTIONS - 1;
    } else if (selection_index >= STANDALONE_MENU_OPTIONS) {
        selection_index = 0;
    }
}

void select_main_menu_option(void) {
    if (selection_index == 0) {
        current_menu = PC_MODE; // Wird später implementiert
    } else if (selection_index == 1) {
        current_menu = STANDALONE_MENU;
        selection_index = 0; // Reset für Untermenü
    }
}

void select_standalone_menu_option(void) {
    if (selection_index == 0) {
        current_menu = STANDALONE_DEMO;
    } else if (selection_index == 1) {
        current_menu = STANDALONE_KURZTEST;
    } else if (selection_index == 2) {
        current_menu = STANDALONE_DAUERTEST;
    }
}

// Display Funktionen
void display_main_menu(void) {
    sprintf(display_buffer[0], "Mode Auswahlen!");
    sprintf(display_buffer[1], "");

    // Optionen: PC Mode, Standalone Mode
    if (selection_index == 0) {
        sprintf(display_buffer[2], "< PC Mode          >");
        sprintf(display_buffer[3], "  Standalone Mode  ");
    } else if (selection_index == 1) {
        sprintf(display_buffer[2], "  PC Mode          ");
        sprintf(display_buffer[3], "< Standalone Mode  >");
    }

    // Anweisung für Bestätigung
    sprintf(display_buffer[5], "<         OK        >");

    // Update das Display
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
}

void display_standalone_menu(void) {
    sprintf(display_buffer[0], "Standalone Mode");
    sprintf(display_buffer[1], "");

    // Optionen: Demo, Kurztest, Dauertest
    if (selection_index == 0) {
        sprintf(display_buffer[2], "< Demo             >");
        sprintf(display_buffer[3], "  Kurztest         ");
        sprintf(display_buffer[4], "  Dauertest        ");
    } else if (selection_index == 1) {
        sprintf(display_buffer[2], "  Demo             ");
        sprintf(display_buffer[3], "< Kurztest         >");
        sprintf(display_buffer[4], "  Dauertest        ");
    } else if (selection_index == 2) {
        sprintf(display_buffer[2], "  Demo             ");
        sprintf(display_buffer[3], "  Kurztest         ");
        sprintf(display_buffer[4], "< Dauertest        >");
    }

    // Anweisung für Bestätigung
    sprintf(display_buffer[6], "<         OK        >");

    // Update das Display
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
}

// Testfunktionen (Platzhalter)
void run_demo_test(ad5684_dac_t *dac) {
    display_jazz_clear(&display1);
    sprintf(display_buffer[0], "Demo Test läuft...");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }

    // Beispielhafte Testlogik
    HAL_Delay(2000); // Simuliere Testdauer

    sprintf(display_buffer[0], "Demo Test abgeschlossen.");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
    HAL_Delay(1000);

    // Zurück zum Standalone Menü
    current_menu = STANDALONE_MENU;
    selection_index = 0;
    display_standalone_menu();
}

void run_kurztest(ad5684_dac_t *dac) {
    display_jazz_clear(&display1);
    sprintf(display_buffer[0], "Kurztest läuft...");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }

    // Beispielhafte Testlogik
    HAL_Delay(2000); // Simuliere Testdauer

    sprintf(display_buffer[0], "Kurztest abgeschlossen.");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
    HAL_Delay(1000);

    // Zurück zum Standalone Menü
    current_menu = STANDALONE_MENU;
    selection_index = 0;
    display_standalone_menu();
}

void run_dauertest(ad5684_dac_t *dac) {
    display_jazz_clear(&display1);
    sprintf(display_buffer[0], "Dauertest läuft...");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }

    // Beispielhafte Testlogik
    HAL_Delay(2000); // Simuliere Testdauer

    sprintf(display_buffer[0], "Dauertest abgeschlossen.");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
    HAL_Delay(1000);

    // Zurück zum Standalone Menü
    current_menu = STANDALONE_MENU;
    selection_index = 0;
    display_standalone_menu();
}

void run_pc_mode(void) {
    display_jazz_clear(&display1);
    sprintf(display_buffer[0], "PC Mode wird");
    sprintf(display_buffer[1], "implementiert...");
    for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
        display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
    }
    HAL_Delay(2000);

    // Zurück zum Hauptmenü
    current_menu = MAIN_MENU;
    selection_index = 0;
    display_main_menu();
}


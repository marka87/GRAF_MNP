///*
// * task.c
// *
// *  Created on: Dec 15, 2024
// *      Author: Mark
// */
//#include "DAC_task.h"
//#include "main.h"
//#include "AD5684RARUZ.h"
//
//
//// Grenzen der Spannung
//#define VOLTAGE_MIN 1.5f      // Minimale Spannung
//#define VOLTAGE_MAX 3.5f      // Maximale Spannung
//#define VOLTAGE_STEP 0.1f    // Schrittweite für Änderung
//#define a_mot 0x01		//Address for DAC-A...
//#define z_mot 0x02		//DAC-B...
//#define d_mot 0x04		//DAC-C.
//
//void DAC_Automatic_Adjustment(ad5684_dac_t* dac) {
//    static float voltage_a = VOLTAGE_MIN; // Startwert für DAC A
//    static float voltage_b = VOLTAGE_MIN; // Startwert für DAC B
//    static int direction = 1;             // Richtung: 1 = erhöhen, -1 = verringern
//
//    // Spannung für DAC A und DAC B erhöhen oder verringern
//    voltage_a += direction * VOLTAGE_STEP;
//    voltage_b += direction * VOLTAGE_STEP;
//
//    // Neue Spannungen setzen
////    ad5684_set_voltage(dac, voltage_a, a_mot);  // DAC A
//    ad5684_set_voltage(dac, voltage_b, z_mot);  // DAC B
//    ad5684_set_voltage(dac, voltage_b, d_mot);  // DAC B
//    // Richtungswechsel an den Grenzen
//    if (voltage_a >= VOLTAGE_MAX || voltage_a <= VOLTAGE_MIN) {
//        direction = -direction; // Richtung umkehren
//    }
//
//    HAL_Delay(50); // Langsame Anpassung durch Verzögerung
//}

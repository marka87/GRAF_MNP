/*
 * Z_PID_Control.h
 *
 *  Created on: Jan 3, 2025
 *      Author: Mark Angyal
 */

#ifndef SRC_Z_PID_CONTROL_H_
#define SRC_Z_PID_CONTROL_H_
#include "AD5684RARUZ.h"
#include <stdbool.h>
#include <stdint.h>

extern float voltage;

/* Führt einen 1ms-Regelzyklus aus. Gibt false zurück, wenn die Schutzüberwachung auslöst. */
bool Z_Axis_PIDControl(ad5684_dac_t* dac, uint32_t Z_Axis_TargetPosition);

/* Regler-Zustände (Integrale, Filter, Encoder-Referenz) sauber zurücksetzen */
void Z_PID_Reset(void);

/* Not-Stopp: Relais sofort abfallen lassen, DAC auf 2.5V, Regler resetten */
void Z_PID_EmergencyStop(ad5684_dac_t *dac);

/* DAC sofort auf 2.5V und Integrale löschen (ohne Relais-Abschaltung) */
void Z_PID_EmergencyNeutral(ad5684_dac_t *dac);

void Z_PID_SetSchedulerEnabled(bool enabled);
bool Z_PID_IsSchedulerEnabled(void);

void Z_PID_SetSpeedLevel(uint8_t level);
uint8_t Z_PID_GetSpeedLevel(void);

/* Äußerer Regler: Position -> Soll-Geschwindigkeit */
void Z_PID_SetPositionParameters(float kp, float ki, float kd);
void Z_PID_GetPositionParameters(float *kp, float *ki, float *kd);

/* Innerer Regler: Geschwindigkeit -> Spannung */
void Z_PID_SetVelocityParameters(float kp, float ki, float kd);
void Z_PID_GetVelocityParameters(float *kp, float *ki, float *kd);

/* Grund für die letzte Schutzabschaltung abfragen */
const char* Z_PID_GetTripReason(void);

#endif /* SRC_Z_PID_CONTROL_H_ */



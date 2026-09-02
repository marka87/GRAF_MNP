/* Z_PID_Control.c
 *
 *  	Created on: Jan 3, 2025
 *      Author: Mark Angyal
 *      Z-Achse Kaskaden-PID-Regelung & SchutzÃ¼berwachung
 */

#include "Z_PID_Control.h"
#include "encoder.h"
#include "main.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Kaskadenregelung:
 * - Ã„uÃŸerer Positions-Regler (P, optional I/D): Position -> Soll-Geschwindigkeit [Inc/ms]
 * - Innerer Geschwindigkeits-Regler (PID): Ist- vs. Soll-Geschwindigkeit -> Spannung [V]
 * - Der I-Anteil Ã¼bernimmt die Haltespannung gegen Schwerkraft (~2.20 - 2.27V).
 *
 * Standard-Parameter:
 * - Position: Kp=0.12, Ki=0, Kd=0 (reiner P-Lageregler)
 * - Velocity: Kp=0.025, Ki=0.00025, Kd=0 (reiner PI-Geschwindigkeitsregler fÃ¼r seidenweichen Lauf)
 */
#define POSITION_KP_DEFAULT 0.12f
#define POSITION_KI_DEFAULT 0.0f
#define POSITION_KD_DEFAULT 0.0f
#define VELOCITY_KP_DEFAULT 0.025f
#define VELOCITY_KI_DEFAULT 0.00025f
#define VELOCITY_KD_DEFAULT 0.0f

/* Geschwindigkeitsstufen 1-10 (Einheit: Encoder-Inc pro 1ms-Zyklus).
 * Perfekt abgestimmt auf die mechanische Bremsdynamik:
 * Stufen: 1, 2, 4, 8, 14, 22, 32, 44, 55, 65 Inc/ms */
#define SPEED_LEVEL_MAX_COUNT 10u
static const uint32_t speed_level_max_velocity[SPEED_LEVEL_MAX_COUNT] = {
	1u, 2u, 4u, 8u, 14u, 22u, 32u, 44u, 55u, 65u
};

/* Maximale Beschleunigung/VerzÃ¶gerung pro 1ms: 2.0 Inc/msÂ² (Ruckfreie Rampe) */
#define MAX_ACCEL_PER_MS 2.0f

/* Maximale Spannungs-AutoritÃ¤t des I-Anteils (verhindert Windup / Ãœberschwingen)
 * Haltespannung liegt ca. 0.25V unter 2.5V (2.25V) -> 0.45V Puffer reicht vollkommen. */
#define MAX_I_VOLTAGE_OFFSET 0.45f

/* Sicherheitsschwellen */
#define MAX_SAFE_VELOCITY 150.0f
#define SAFETY_POSITION_MARGIN 400

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...

extern uint32_t z_encoder_start;
extern uint32_t z_encoder_end;

static char s_trip_reason[64] = "OK";

typedef struct {
	float kp;
	float ki;
	float kd;
} z_pid_profile_t;

static z_pid_profile_t position_profile = { POSITION_KP_DEFAULT, POSITION_KI_DEFAULT, POSITION_KD_DEFAULT };
static z_pid_profile_t velocity_profile = { VELOCITY_KP_DEFAULT, VELOCITY_KI_DEFAULT, VELOCITY_KD_DEFAULT };

static float position_integral = 0.0f;
static float position_previous_error = 0.0f;
static float velocity_integral = 0.0f;
static float velocity_previous_error = 0.0f;

static float smoothed_velocity = 0.0f;
static float ramped_velocity = 0.0f;
static int last_encoder_value = 0;
static bool last_encoder_value_initialized = false;
static uint32_t last_velocity_tick = 0u;
static bool last_velocity_tick_initialized = false;

static uint8_t speed_level = 7u;
static bool scheduler_enabled = true;
float voltage = NEUTRAL_VOLTAGE;

const char* Z_PID_GetTripReason(void) {
	return s_trip_reason;
}

static void clamp_position_integral(void) {
	float limit = (position_profile.ki > 0.0f) ? (5.0f / position_profile.ki) : 0.0f;
	if (position_integral > limit) position_integral = limit;
	if (position_integral < -limit) position_integral = -limit;
}

static void clamp_velocity_integral(void) {
	float limit = (velocity_profile.ki > 0.0f) ? (MAX_I_VOLTAGE_OFFSET / velocity_profile.ki) : 0.0f;
	if (velocity_integral > limit) velocity_integral = limit;
	if (velocity_integral < -limit) velocity_integral = -limit;
}

static void set_profile_parameters(z_pid_profile_t *profile, float kp, float ki, float kd) {
	if (profile == NULL) {
		return;
	}
	if (kp < 0.0f || ki < 0.0f || kd < 0.0f) {
		return;
	}
	profile->kp = kp;
	profile->ki = ki;
	profile->kd = kd;
}

/* 1st-Order Low-Pass (EMA) Filter fÃ¼r Ist-Geschwindigkeit:
 * Eliminiert 1000-Hz Encoder-Quantisierungsrauschen und verhindert Rattern */
static float update_and_get_smoothed_velocity(float raw_velocity) {
	smoothed_velocity += 0.20f * (raw_velocity - smoothed_velocity);
	return smoothed_velocity;
}

void Z_PID_Reset(void) {
	position_integral = 0.0f;
	position_previous_error = 0.0f;
	velocity_integral = 0.0f;
	velocity_previous_error = 0.0f;
	smoothed_velocity = 0.0f;
	ramped_velocity = 0.0f;
	last_encoder_value = Encoder_GetPosition_Z_AXIS();
	last_encoder_value_initialized = true;
	last_velocity_tick = HAL_GetTick();
	last_velocity_tick_initialized = true;
	voltage = NEUTRAL_VOLTAGE;
}

bool Z_Axis_PIDControl(ad5684_dac_t *dac, uint32_t Z_Axis_TargetPosition) {
	/* Istwert aus Encoder */
	int encoder_value = Encoder_GetPosition_Z_AXIS();

	if (!last_encoder_value_initialized) {
		last_encoder_value = encoder_value;
		last_encoder_value_initialized = true;
	}
	
	/* Exakte Zeitdifferenz (ms) seit letztem Aufruf erfassen (schÃ¼tzt vor Display-/UART-Jitter) */
	uint32_t now = HAL_GetTick();
	if (!last_velocity_tick_initialized) {
		last_velocity_tick = now;
		last_velocity_tick_initialized = true;
	}
	uint32_t dt_ms = now - last_velocity_tick;
	last_velocity_tick = now;
	if (dt_ms == 0u) {
		dt_ms = 1u;
	}

	int raw_delta = encoder_value - last_encoder_value;
	last_encoder_value = encoder_value;

	float raw_velocity = 0.0f;
	if (dt_ms > 20u) {
		/* Hauptschleife war durch Display/UART blockiert -> keinen kÃ¼nstlichen Peak berechnen */
		raw_velocity = 0.0f;
		smoothed_velocity = 0.0f;
	} else {
		raw_velocity = (float)raw_delta / (float)dt_ms;
	}
	float actual_velocity = update_and_get_smoothed_velocity(raw_velocity);

	/* --- SchutzÃ¼berwachung (Not-Stopp) --- */
	/* 1. Ãœberdrehzahl / Runaway */
	if (fabsf(actual_velocity) > MAX_SAFE_VELOCITY) {
		snprintf(s_trip_reason, sizeof(s_trip_reason), "Speed: %.1f > %.0f Inc/ms", (double)fabsf(actual_velocity), (double)MAX_SAFE_VELOCITY);
		return false;
	}
	/* 2. Positionsgrenzen (wenn bereits referenziert) */
	if (z_encoder_start != 0u || z_encoder_end != 0u) {
		uint32_t lower = (z_encoder_start < z_encoder_end) ? z_encoder_start : z_encoder_end;
		uint32_t upper = (z_encoder_start > z_encoder_end) ? z_encoder_start : z_encoder_end;
		if (encoder_value < ((int)lower - SAFETY_POSITION_MARGIN)) {
			snprintf(s_trip_reason, sizeof(s_trip_reason), "Min-Limit: %d < %ld", encoder_value, (long)((int)lower - SAFETY_POSITION_MARGIN));
			return false;
		}
		if (encoder_value > ((int)upper + SAFETY_POSITION_MARGIN)) {
			snprintf(s_trip_reason, sizeof(s_trip_reason), "Max-Limit: %d > %ld", encoder_value, (long)((int)upper + SAFETY_POSITION_MARGIN));
			return false;
		}
	}

	/* --- Ã„uÃŸerer Regler: Position -> Soll-Geschwindigkeit --- */
	int position_error = encoder_value - (int)Z_Axis_TargetPosition;
	position_integral += (float)position_error;
	clamp_position_integral();
	float position_derivative = (float)position_error - position_previous_error;
	position_previous_error = (float)position_error;

	float desired_velocity = 0.0f;
	if (abs(position_error) > 1) {
		desired_velocity = -((position_profile.kp * (float)position_error)
				+ (position_profile.ki * position_integral)
				+ (position_profile.kd * position_derivative));
	}

	/* Geschwindigkeitsbegrenzung nach Speed-Stufe */
	uint32_t max_velocity = speed_level_max_velocity[speed_level - 1u];
	if (desired_velocity > (float)max_velocity) desired_velocity = (float)max_velocity;
	if (desired_velocity < -(float)max_velocity) desired_velocity = -(float)max_velocity;

	/* Beschleunigungs- und Bremsrampe: Sanfter, ruckfreier Geschwindigkeitsverlauf (kein 1-0 Schaltschock) */
	float vel_diff = desired_velocity - ramped_velocity;
	if (vel_diff > MAX_ACCEL_PER_MS) {
		ramped_velocity += MAX_ACCEL_PER_MS;
	} else if (vel_diff < -MAX_ACCEL_PER_MS) {
		ramped_velocity -= MAX_ACCEL_PER_MS;
	} else {
		ramped_velocity = desired_velocity;
	}

	/* --- Innerer Regler: Ist- vs. Gerampte Soll-Geschwindigkeit -> Spannung --- */
	float velocity_error = actual_velocity - ramped_velocity;
	velocity_integral += velocity_error;
	clamp_velocity_integral();
	float velocity_derivative = velocity_error - velocity_previous_error;
	velocity_previous_error = velocity_error;

	float output = (velocity_profile.kp * velocity_error)
			+ (velocity_profile.ki * velocity_integral)
			+ (velocity_profile.kd * velocity_derivative);

	/* Spannung berechnen */
	voltage = NEUTRAL_VOLTAGE + output;

	/* Begrenzen der Spannung */
	if (voltage < VOLTAGE_MIN)
		voltage = VOLTAGE_MIN;
	if (voltage > VOLTAGE_MAX)
		voltage = VOLTAGE_MAX;

	/* Spannung an den DAC senden */
	ad5684_set_voltage(dac, voltage, z_mot);
	return true;
}

void Z_PID_SetPositionParameters(float kp, float ki, float kd) {
	set_profile_parameters(&position_profile, kp, ki, kd);
	position_integral = 0.0f;
	position_previous_error = 0.0f;
}

void Z_PID_GetPositionParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = position_profile.kp;
	if (ki != NULL) *ki = position_profile.ki;
	if (kd != NULL) *kd = position_profile.kd;
}

void Z_PID_SetVelocityParameters(float kp, float ki, float kd) {
	set_profile_parameters(&velocity_profile, kp, ki, kd);
	velocity_integral = 0.0f;
	velocity_previous_error = 0.0f;
}

void Z_PID_GetVelocityParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = velocity_profile.kp;
	if (ki != NULL) *ki = velocity_profile.ki;
	if (kd != NULL) *kd = velocity_profile.kd;
}

void Z_PID_SetSpeedLevel(uint8_t level) {
	if (level < 1u || level > SPEED_LEVEL_MAX_COUNT) {
		return;
	}
	speed_level = level;
}

uint8_t Z_PID_GetSpeedLevel(void) {
	return speed_level;
}

void Z_PID_SetSchedulerEnabled(bool enabled) {
	scheduler_enabled = enabled;
}

bool Z_PID_IsSchedulerEnabled(void) {
	return scheduler_enabled;
}

void Z_PID_EmergencyStop(ad5684_dac_t *dac) {
	HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_RESET);
	Z_PID_Reset();
	voltage = NEUTRAL_VOLTAGE;
	ad5684_set_voltage(dac, voltage, z_mot);
}

void Z_PID_EmergencyNeutral(ad5684_dac_t *dac) {
	Z_PID_Reset();
	voltage = NEUTRAL_VOLTAGE;
	ad5684_set_voltage(dac, voltage, z_mot);
}
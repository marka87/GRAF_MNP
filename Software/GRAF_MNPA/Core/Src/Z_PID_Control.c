/* Z_PID_Control.c
 *
 *  	Created on: Jan 3, 2025
 *      Author: Mark Angyal
 *      Z-Achse PID-Regelung
 */

#include "Z_PID_Control.h"

#include <sys/_stdint.h>

#include "encoder.h"

#define FAST_KP_DEFAULT 0.0031f
#define FAST_KI_DEFAULT 0.000001f
#define FAST_KD_DEFAULT 0.050000f
#define SLOW_KP_DEFAULT 0.0050f
#define SLOW_KI_DEFAULT 0.000020f
#define SLOW_KD_DEFAULT 0.001000f
#define DIST_SLOW_ENTER_DEFAULT 100u
#define DIST_FAST_EXIT_DEFAULT  140u
#define FAST_HOLD_TARGET_DELTA_DEFAULT 50u
#define FAST_HOLD_CYCLES_DEFAULT 30u

/* Spannungsgrenzen und Neutralspannung */
#define VOLTAGE_MIN     0.0f
#define VOLTAGE_MAX     5.0f
#define NEUTRAL_VOLTAGE 2.5f

/* DAC-Adresse */
#define z_mot 0x02		//DAC-B...

/* Statische Variablen für PID-Zustände */
static float integral = 0.0f;
static float previous_error = 0.0f;
typedef struct {
	float kp;
	float ki;
	float kd;
} z_pid_profile_t;

static z_pid_profile_t fast_profile = { FAST_KP_DEFAULT, FAST_KI_DEFAULT, FAST_KD_DEFAULT };
static z_pid_profile_t slow_profile = { SLOW_KP_DEFAULT, SLOW_KI_DEFAULT, SLOW_KD_DEFAULT };
static z_pid_profile_t *active_profile = &fast_profile;
static uint32_t last_target = 0u;
static uint32_t fast_hold_cycles_left = 0u;
static uint32_t dist_slow_enter = DIST_SLOW_ENTER_DEFAULT;
static uint32_t dist_fast_exit = DIST_FAST_EXIT_DEFAULT;
static uint32_t fast_hold_target_delta = FAST_HOLD_TARGET_DELTA_DEFAULT;
static uint32_t fast_hold_cycles = FAST_HOLD_CYCLES_DEFAULT;
static bool scheduler_enabled = true;
/* Geschwindigkeitsstufen 1-5: das PID-Ziel wird pro Zyklus nur um max. N Encoder-Schritte
 * an das eigentliche Ziel herangeführt (Ziel-Rampe). Die Regelung selbst (PID-Gains, Spannung)
 * bleibt unangetastet -> keine Instabilität, nur die Anfahrt wird gebremst.
 * ponytail: Startwerte geschätzt, an realer Hardware feinjustieren. */
static const uint32_t speed_level_step_per_cycle[5] = { 1u, 3u, 8u, 20u, 0xFFFFFFFFu };
static uint8_t speed_level = 5u;
static uint32_t ramped_target = 0u;
static bool ramped_target_initialized = false;
float voltage;

static void clamp_integral_to_active_profile(void) {
	float integral_limit = (active_profile->ki > 0.0f) ? (2.5f / active_profile->ki) : 0.0f;
	if (integral > integral_limit) integral = integral_limit;
	if (integral < -integral_limit) integral = -integral_limit;
}

static void set_profile_parameters(z_pid_profile_t *profile, float kp, float ki, float kd) {
	if (profile == NULL) {
		return;
	}
	if (kp <= 0.0f || ki <= 0.0f || kd < 0.0f) {
		return;
	}
	profile->kp = kp;
	profile->ki = ki;
	profile->kd = kd;
	if (active_profile == profile) {
		integral = 0.0f;
		previous_error = 0.0f;
	}
}

void Z_Axis_PIDControl(ad5684_dac_t *dac, uint32_t Z_Axis_TargetPosition) {
	/* Istwert aus Encoder */
	int encoder_value = Encoder_GetPosition_Z_AXIS();

	if (!ramped_target_initialized) {
		ramped_target = (uint32_t)encoder_value;
		ramped_target_initialized = true;
	}

	/* Geschwindigkeitsstufe: das tatsächlich angefahrene Ziel nur schrittweise an
	 * Z_Axis_TargetPosition heranführen (Ziel-Rampe statt Sprung). */
	uint32_t max_step_per_cycle = speed_level_step_per_cycle[speed_level - 1u];
	if (ramped_target < Z_Axis_TargetPosition) {
		uint32_t remaining = Z_Axis_TargetPosition - ramped_target;
		ramped_target += (remaining < max_step_per_cycle) ? remaining : max_step_per_cycle;
	} else if (ramped_target > Z_Axis_TargetPosition) {
		uint32_t remaining = ramped_target - Z_Axis_TargetPosition;
		ramped_target -= (remaining < max_step_per_cycle) ? remaining : max_step_per_cycle;
	}

	/* Fehlerberechnung: Negative Werte => Spannung unter 2.5V, Positive => über 2.5V */
	int error = encoder_value - (int)ramped_target;
	uint32_t target_delta =
			(Z_Axis_TargetPosition > last_target)
					? (Z_Axis_TargetPosition - last_target)
					: (last_target - Z_Axis_TargetPosition);
	if (target_delta >= fast_hold_target_delta) {
		fast_hold_cycles_left = fast_hold_cycles;
	}
	uint32_t abs_error = (error < 0) ? (uint32_t)(-error) : (uint32_t)error;
	if (!scheduler_enabled) {
		if (active_profile != &fast_profile) {
			Z_PID_SetMode(true);
		}
		fast_hold_cycles_left = 0u;
	} else {
		if (fast_hold_cycles_left > 0u) {
			--fast_hold_cycles_left;
			if (active_profile != &fast_profile) {
				Z_PID_SetMode(true);
			}
		} else {
			if (active_profile == &fast_profile) {
				if (abs_error <= dist_slow_enter) {
					Z_PID_SetMode(false);
				}
			} else {
				if (abs_error >= dist_fast_exit) {
					Z_PID_SetMode(true);
				}
			}
		}
	}
	if (Z_Axis_TargetPosition != last_target) {
		previous_error = (float) error;
		last_target = Z_Axis_TargetPosition;
	}
	// Integralanteil
	integral += (float)error; // * DT
	clamp_integral_to_active_profile();
	/* Differentialanteil */
	float derivative = (float)(error - previous_error); // / DT;

	/* PID-Berechnung */
	float output = (active_profile->kp * (float)error)
			+ (active_profile->ki * integral)
			+ (active_profile->kd * derivative);

	/* Spannung berechnen (unskalierter PID-Output, volle Regelgüte) */
	voltage = NEUTRAL_VOLTAGE + output;

	/* Begrenzen der Spannung */
	if (voltage < VOLTAGE_MIN)
		voltage = VOLTAGE_MIN;
	if (voltage > VOLTAGE_MAX)
		voltage = VOLTAGE_MAX;

	/* Spannung an den DAC senden */
	ad5684_set_voltage(dac, voltage, z_mot);

	/* Fehler für den nächsten Zyklus speichern */
	previous_error = (float) error;
}

void Z_PID_SetParameters(float kp, float ki, float kd) {
	set_profile_parameters(active_profile, kp, ki, kd);
}

void Z_PID_GetParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = active_profile->kp;
	if (ki != NULL) *ki = active_profile->ki;
	if (kd != NULL) *kd = active_profile->kd;
}

void Z_PID_SetSpeedLevel(uint8_t level) {
	if (level < 1u || level > 5u) {
		return;
	}
	speed_level = level;
}

uint8_t Z_PID_GetSpeedLevel(void) {
	return speed_level;
}

void Z_PID_SetMode(bool fast_mode) {
	z_pid_profile_t *new_profile = fast_mode ? &fast_profile : &slow_profile;
	if (active_profile == new_profile) {
		return;
	}
	active_profile = new_profile;
	clamp_integral_to_active_profile();
}

void Z_PID_SetSchedulerEnabled(bool enabled) {
	scheduler_enabled = enabled;
	if (!scheduler_enabled) {
		fast_hold_cycles_left = 0u;
		Z_PID_SetMode(true);
	}
}

bool Z_PID_IsSchedulerEnabled(void) {
	return scheduler_enabled;
}

void Z_PID_SetFastParameters(float kp, float ki, float kd) {
	set_profile_parameters(&fast_profile, kp, ki, kd);
}

void Z_PID_SetSlowParameters(float kp, float ki, float kd) {
	set_profile_parameters(&slow_profile, kp, ki, kd);
}

void Z_PID_GetFastParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = fast_profile.kp;
	if (ki != NULL) *ki = fast_profile.ki;
	if (kd != NULL) *kd = fast_profile.kd;
}

void Z_PID_GetSlowParameters(float *kp, float *ki, float *kd) {
	if (kp != NULL) *kp = slow_profile.kp;
	if (ki != NULL) *ki = slow_profile.ki;
	if (kd != NULL) *kd = slow_profile.kd;
}

void Z_PID_SetSchedulerParameters(uint32_t slow_enter, uint32_t fast_exit, uint32_t hold_target_delta, uint32_t hold_cycles_value) {
	if (slow_enter == 0u || fast_exit == 0u || hold_target_delta == 0u) {
		return;
	}
	if (fast_exit <= slow_enter) {
		return;
	}
	dist_slow_enter = slow_enter;
	dist_fast_exit = fast_exit;
	fast_hold_target_delta = hold_target_delta;
	fast_hold_cycles = hold_cycles_value;
	if (fast_hold_cycles_left > fast_hold_cycles) {
		fast_hold_cycles_left = fast_hold_cycles;
	}
}

void Z_PID_GetSchedulerParameters(uint32_t *slow_enter, uint32_t *fast_exit, uint32_t *hold_target_delta, uint32_t *hold_cycles_value) {
	if (slow_enter != NULL) *slow_enter = dist_slow_enter;
	if (fast_exit != NULL) *fast_exit = dist_fast_exit;
	if (hold_target_delta != NULL) *hold_target_delta = fast_hold_target_delta;
	if (hold_cycles_value != NULL) *hold_cycles_value = fast_hold_cycles;
}

void Z_PID_EmergencyNeutral(ad5684_dac_t *dac) {
	integral = 0.0f;
	previous_error = 0.0f;
	voltage = NEUTRAL_VOLTAGE;
	ad5684_set_voltage(dac, voltage, z_mot);
}

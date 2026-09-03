/*
 * test_run.c - Testablauf (TEST_RUN State Handler)
 *
 * Verwaltet zwei Testmodi:
 *   1. TESTRUN_MODE_A_CLASSIC: Dauertest zwischen NO-Sensor und Unterkante (ohne Bauteil)
 *   2. TESTRUN_MODE_B_PROBE_SCATTER: Bauteil-Antastung mit automatischer
 *      Drucksensor-Offset-Kalibrierung und Streuungs-Ermittlung.
 *
 * Author: Mark Angyal
 */

#include "test_run.h"
#include "ADC_read.h"
#include "encoder.h"
#include "d_mot_control.h"
#include "data_buffer.h"
#include "Reference_Run.h"
#include "Z_PID_Control.h"
#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* Extern-Deklarationen */
extern ad5684_dac_t    dac;
extern ADC_HandleTypeDef hadc1;
extern uint32_t        z_ax_no_pos;
extern uint32_t        z_encoder_start;
extern uint32_t        z_encoder_end;

/* Druckmotor-Spannung waehrend der Testlaeufe (Gegendruck zur Vermeidung von Fehltriggerung) */
#define TEST_D_MOT_VOLTAGE_DEFAULT   3.5f

/* GO_UP-Puffer oberhalb der NO-Sensor-Position bis zur Zielposition */
#define GO_UP_OVERSHOOT              350u
#define GO_UP_SWITCH_MARGIN          100u

/* GO_DOWN-Zielpuffer ab Encoder-Startposition */
#define GO_DOWN_TARGET_OFFS          400u
#define GO_DOWN_SWITCH_MARGIN        500u

/* Test B Parameter */
#define TEST_B_FAST_SPEED_LEVEL      8u    /* Schnelle Verfahrfahrt (wie Naehmaschine) */
#define TEST_B_SETUP_PROBE_SPEED     3u    /* Sanfte Suchfahrt ueber die gesamte Hoehe (egal ob 2mm oder 10mm Bauteil) */
#define TEST_B_TRIGGER_DELTA_MV_DEF  1500u /* TTL-Logik: 1.5V Schaltschwelle ueber Ruhe-Baseline */

/* Ausblend-Zone fuer den oberen Totpunkt (wo der schlagartige Richtungswechsel stattfindet) */
#define REVERSAL_BLANKING_MARGIN     300u

typedef enum {
    /* Test A Phasen */
    PHASE_A_GO_UP,
    PHASE_A_GO_DOWN,

    /* Test B Phasen */
    PHASE_B_SETUP_SLOW_PROBE,
    PHASE_B_FAST_UP,
    PHASE_B_FAST_DOWN
} TestRunInternalPhase_t;

static TestRunMode_t          s_mode = TESTRUN_MODE_A_CLASSIC;
static uint32_t               s_num_total_cycles;
static uint32_t               s_current_cycle;
static TestRunInternalPhase_t s_phase;
static char                   s_error_msg[48];
static TestRunStats_t         s_stats;
static TestBScatterStats_t    s_scatter_stats;
static TestRunCycleMetrics_t  s_last_cycle_metrics;

static int32_t                s_cycle_start_pos;
static int32_t                s_cycle_trigger_pos;
static int32_t                s_cycle_end_pos;
static uint32_t               s_elapsed_ms;

static float                  s_dmot_voltage = TEST_D_MOT_VOLTAGE_DEFAULT;
static uint32_t               s_trigger_delta_mv = TEST_B_TRIGGER_DELTA_MV_DEF;
static uint16_t               s_ds_baseline_adc = 0;
static uint16_t               s_ds_trigger_threshold = 0;
static uint8_t                s_ds_trigger_debounce = 0;
static uint8_t                s_ds_accel_fault_debounce = 0;
static int64_t                s_probe_pos_sum = 0;
static uint32_t               s_probe_count = 0;
static uint8_t                s_fast_speed_level = 6u;
static uint32_t               s_ds_filter_acc = 0;

/* 4-Sample Exponential Moving Average Filter fÃ¼r Drucksensor */
static uint16_t read_filtered_ds(void) {
    uint16_t raw = ADC_Drucksensor(&hadc1);
    if (s_ds_filter_acc == 0) {
        s_ds_filter_acc = ((uint32_t)raw) << 4;
    } else {
        s_ds_filter_acc = (s_ds_filter_acc * 3 + (((uint32_t)raw) << 4)) / 4;
    }
    return (uint16_t)(s_ds_filter_acc >> 4);
}

void TestRun_SetDruckmotorVoltage(float voltage) {
    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > 5.0f) voltage = 5.0f;
    s_dmot_voltage = voltage;
}

float TestRun_GetDruckmotorVoltage(void) {
    return s_dmot_voltage;
}

void TestRun_SetTriggerDeltaMv(uint32_t mv) {
    if (mv < 20u) mv = 20u;
    if (mv > 4000u) mv = 4000u;
    s_trigger_delta_mv = mv;
}

uint32_t TestRun_GetTriggerDeltaMv(void) {
    return s_trigger_delta_mv;
}

TestRunMode_t TestRun_GetMode(void) {
    return s_mode;
}

const char *TestRun_GetPhaseName(void) {
    switch (s_phase) {
        case PHASE_A_GO_UP:           return "A_GO_UP";
        case PHASE_A_GO_DOWN:         return "A_GO_DOWN";
        case PHASE_B_SETUP_SLOW_PROBE:return "B_SETUP_PROBE";
        case PHASE_B_FAST_UP:         return "B_FAST_UP";
        case PHASE_B_FAST_DOWN:       return "B_FAST_DOWN";
        default:                      return "UNKNOWN";
    }
}

static void update_position_range(int32_t position) {
    if (position < s_stats.z_ist_min) s_stats.z_ist_min = position;
    if (position > s_stats.z_ist_max) s_stats.z_ist_max = position;
}

static void update_target_range(int32_t target) {
    if (target < s_stats.z_soll_min) s_stats.z_soll_min = target;
    if (target > s_stats.z_soll_max) s_stats.z_soll_max = target;
}

static void update_cycle_metrics(void) {
    int32_t down_target = (int32_t)(z_encoder_start + GO_DOWN_TARGET_OFFS);
    int32_t travel_to_trigger = s_cycle_trigger_pos - s_cycle_start_pos;
    int32_t overshoot = (s_cycle_end_pos > down_target) ? (s_cycle_end_pos - down_target) : 0;
    int32_t lost_steps = (s_cycle_end_pos < down_target) ? (down_target - s_cycle_end_pos) : 0;
    int32_t delta = s_cycle_end_pos - s_cycle_start_pos;

    s_last_cycle_metrics.cycle_index = s_current_cycle;
    s_last_cycle_metrics.start_pos = s_cycle_start_pos;
    s_last_cycle_metrics.trigger_pos = s_cycle_trigger_pos;
    s_last_cycle_metrics.down_target = down_target;
    s_last_cycle_metrics.end_pos = s_cycle_end_pos;
    s_last_cycle_metrics.travel_to_trigger = travel_to_trigger;
    s_last_cycle_metrics.overshoot = overshoot;
    s_last_cycle_metrics.lost_steps = lost_steps;

    s_stats.last_cycle_delta = delta;
    s_stats.last_cycle_overshoot = overshoot;
    s_stats.last_cycle_lost_steps = lost_steps;
}

void TestRun_Init(uint32_t num_cycles) {
    TestRun_InitEx(TESTRUN_MODE_A_CLASSIC, num_cycles);
}

void TestRun_InitEx(TestRunMode_t mode, uint32_t num_cycles) {
    s_mode             = mode;
    s_num_total_cycles = num_cycles;
    s_current_cycle    = 0;
    s_error_msg[0]     = '\0';
    s_elapsed_ms       = 0u;
    s_cycle_start_pos  = 0;
    s_cycle_trigger_pos = 0;
    s_cycle_end_pos    = 0;
    s_probe_pos_sum    = 0;
    s_probe_count      = 0;
    s_ds_filter_acc    = 0;

    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.total_cycles      = num_cycles;
    s_stats.no_sensor_pos     = (int32_t)z_ax_no_pos;
    s_stats.z_trigger_pos_min = INT32_MAX;
    s_stats.z_trigger_pos_max = INT32_MIN;
    s_stats.z_ist_min         = INT32_MAX;
    s_stats.z_ist_max         = INT32_MIN;
    s_stats.z_soll_min        = INT32_MAX;
    s_stats.z_soll_max        = INT32_MIN;

    memset(&s_scatter_stats, 0, sizeof(s_scatter_stats));
    s_scatter_stats.z_min_pos = INT32_MAX;
    s_scatter_stats.z_max_pos = INT32_MIN;

    /* Druckmotor aktivieren (hÃ¤lt Hebel mit eingestellter Spannung nach unten) */
    d_mot_control(&dac, s_dmot_voltage);
    HAL_Delay(100);

    /* Drucksensor Baseline unter Druckmotor-Vorspannung erfassen */
    uint32_t adc_sum = 0;
    for (int i = 0; i < 8; i++) {
        adc_sum += ADC_Drucksensor(&hadc1);
    }
    s_ds_baseline_adc = (uint16_t)(adc_sum / 8);

    /* Trigger = Baseline + Delta mV (z.B. +1500mV TTL = ca. 1228 ADC-Counts) */
    uint32_t delta_adc = (uint32_t)((float)s_trigger_delta_mv * (4095.0f / 5000.0f));
    if (delta_adc < 40u) delta_adc = 40u;
    s_ds_trigger_threshold = s_ds_baseline_adc + (uint16_t)delta_adc;
    if (s_ds_trigger_threshold < 75u) {
        s_ds_trigger_threshold = 75u;
    }
    s_scatter_stats.baseline_adc = s_ds_baseline_adc;
    s_scatter_stats.trigger_adc  = s_ds_trigger_threshold;

    /* Datenpuffer zurÃ¼cksetzen */
    data_buffer_reset();

    /* Startposition erfassen und Ziel sofort auf Istwert setzen */
    int32_t z_pos_now = Encoder_GetPosition_Z_AXIS();
    if (z_pos_now < 0) z_pos_now = 0;
    s_cycle_start_pos = z_pos_now;
    update_position_range(z_pos_now);
    update_target_range(z_pos_now);
    Z_Target_SetRequestedDirect((uint32_t)z_pos_now);

    s_ds_trigger_debounce = 0;
    s_ds_accel_fault_debounce = 0;

    /* Geschwindigkeit: Direkte Ãœbernahme ohne Level-1-VerfÃ¤lschung */
    s_fast_speed_level = Z_PID_GetSpeedLevel();
    if (s_fast_speed_level < 1u) s_fast_speed_level = 1u;
    if (s_fast_speed_level > 16u) s_fast_speed_level = 16u;

    if (s_mode == TESTRUN_MODE_B_PROBE_SCATTER) {
        /* Sanfte Suchfahrt von oben bis zum Bauteil auf Speed 3 */
        s_phase = PHASE_B_SETUP_SLOW_PROBE;
        Z_PID_SetSpeedLevel(TEST_B_SETUP_PROBE_SPEED);
    } else {
        s_phase = PHASE_A_GO_UP;
        Z_PID_SetSpeedLevel(s_fast_speed_level);
    }
}

TestRunResult_t TestRun_Tick(bool tick_100ms_elapsed) {
    /* Vorzeitiger Abschluss falls Zykluszahl bereits erreicht */
    if (s_current_cycle >= s_num_total_cycles) {
        return TESTRUN_COMPLETE;
    }

    int32_t z_pos = Encoder_GetPosition_Z_AXIS();
    int32_t z_target = (int32_t)Z_Target_GetRequested();
    uint16_t ds_value = read_filtered_ds();

    /* Globale Positions-, Zeit- und Logging-Aktualisierung fÃ¼r BEIDE Tests */
    if (tick_100ms_elapsed) {
        s_elapsed_ms += 100u;
        s_stats.test_time_ms = s_elapsed_ms;
        s_stats.last_ist_pos = z_pos;
        s_stats.last_soll_pos = z_target;
        update_position_range(z_pos);
        update_target_range(z_target);
        log_data_point(z_pos, z_target);
    }

    int32_t top_zone     = (int32_t)z_ax_no_pos - (int32_t)REVERSAL_BLANKING_MARGIN;
    int32_t contact_zone = s_scatter_stats.z_ref_pos + 150;
    int32_t takeoff_zone = s_scatter_stats.z_ref_pos + 1300;

    /* =========================================================================
     * TEST MODUS B: BAUTEIL-ANTASTUNG & STREUUNG
     * ========================================================================= */
    if (s_mode == TESTRUN_MODE_B_PROBE_SCATTER) {

        /* -----------------------------------------------------------------
         * 1. SETUP: Sanfte Referenz-Antastung Ã¼ber die gesamte Verfahrstrecke
         *    (FÃ¤hrt mit sanfter Stufe 3 von oben nach unten, egal wie dick das Bauteil ist)
         * ----------------------------------------------------------------- */
        if (s_phase == PHASE_B_SETUP_SLOW_PROBE) {
            Z_PID_SetSpeedLevel(TEST_B_SETUP_PROBE_SPEED);
            int32_t probe_target = (int32_t)z_encoder_start;
            update_target_range(probe_target);
            Z_Target_SetRequestedDirect((uint32_t)probe_target);

            /* Drucksensor / Lichtschranke hat ausgelÃ¶st: BauteilhÃ¶he gefunden! */
            if (ds_value >= s_ds_trigger_threshold) {
                s_ds_trigger_debounce++;
                if (s_ds_trigger_debounce >= 2u) {
                    s_ds_trigger_debounce = 0;
                    s_ds_accel_fault_debounce = 0;
                    int32_t touch_pos = z_pos;
                    Z_Target_SetRequestedDirect((uint32_t)touch_pos); // Sofort stoppen

                    s_scatter_stats.z_ref_pos = touch_pos;
                    s_scatter_stats.z_min_pos = touch_pos;
                    s_scatter_stats.z_max_pos = touch_pos;
                    s_scatter_stats.scatter_range = 0;
                    s_scatter_stats.last_delta = 0;
                    s_scatter_stats.z_last_probe_pos = touch_pos;
                    s_probe_pos_sum = 0;
                    s_probe_count = 0;

                    char ref_msg[64];
                    snprintf(ref_msg, sizeof(ref_msg), "TEST_B_REF:%ld\r\n", (long)touch_pos);
                    uart_send_text(ref_msg, 10);

                    /* Setup abgeschlossen -> Starte High-Speed Zyklen 1..N nach oben */
                    s_phase = PHASE_B_FAST_UP;
                    Z_PID_SetSpeedLevel(s_fast_speed_level);
                }
            } else {
                s_ds_trigger_debounce = 0;
            }

            /* Unteren Anschlag erreicht, aber kein Bauteil gefunden */
            if (z_pos <= (int32_t)(z_encoder_start + 15)) {
                snprintf(s_error_msg, sizeof(s_error_msg), "Kein Bauteil @ %ld", (long)z_pos);
                return TESTRUN_ERROR;
            }
        }

        /* -----------------------------------------------------------------
         * 2. HAUPT-ZYKLEN (1..N): VOLLSPEED NÃ„HMASCHINE (STABIL & GLEICHMÃ„SSIG)
         * ----------------------------------------------------------------- */
        else if (s_phase == PHASE_B_FAST_UP) {
            Z_PID_SetSpeedLevel(s_fast_speed_level);
            int32_t retract_target = (int32_t)(z_ax_no_pos + GO_UP_OVERSHOOT);
            update_target_range(retract_target);
            Z_Target_SetRequestedDirect((uint32_t)retract_target);

            /* Beschleunigungs-Überwachung beim Hochfahren auf freier Strecke */
            if (z_pos > takeoff_zone && z_pos < top_zone) {
                if (ds_value >= s_ds_trigger_threshold) {
                    s_ds_accel_fault_debounce++;
                    if (s_ds_accel_fault_debounce >= 20u) {
                        s_stats.ds_errors++;
                        snprintf(s_error_msg, sizeof(s_error_msg), "Beschl. UP: %u @ %ld", ds_value, (long)z_pos);
                        return TESTRUN_ERROR;
                    }
                } else {
                    s_ds_accel_fault_debounce = 0;
                }
            } else {
                s_ds_accel_fault_debounce = 0;
            }

            /* Nadel-oben Sensor (NO-Sensor) MUSS oben immer erreicht werden */
            if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) {
                s_stats.valid_sensor_events++;
                s_phase = PHASE_B_FAST_DOWN;
                s_ds_trigger_debounce = 0;
                s_ds_accel_fault_debounce = 0;
            } else if (z_pos >= (int32_t)(z_ax_no_pos + GO_UP_OVERSHOOT)) {
                /* Oben angekommen, aber NO-Sensor hat NICHT ausgelÃ¶st -> FEHLER! */
                s_stats.no_sensor_errors++;
                snprintf(s_error_msg, sizeof(s_error_msg), "NO-Sen fehlt @ %ld", (long)z_pos);
                return TESTRUN_ERROR;
            }
        }
        else if (s_phase == PHASE_B_FAST_DOWN) {
            Z_PID_SetSpeedLevel(s_fast_speed_level);

            int32_t down_target = s_scatter_stats.z_ref_pos - 25;
            if (down_target < (int32_t)z_encoder_start) down_target = (int32_t)z_encoder_start;
            update_target_range(down_target);
            Z_Target_SetRequestedDirect((uint32_t)down_target);

            /* Beschleunigungs-Überwachung auf freier Fahrt nach unten */
            if (z_pos < top_zone && z_pos > contact_zone) {
                if (ds_value >= s_ds_trigger_threshold) {
                    s_ds_accel_fault_debounce++;
                    if (s_ds_accel_fault_debounce >= 6u) {
                        s_stats.ds_errors++;
                        snprintf(s_error_msg, sizeof(s_error_msg), "Beschl. DOWN: %u @ %ld", ds_value, (long)z_pos);
                        return TESTRUN_ERROR;
                    }
                } else {
                    s_ds_accel_fault_debounce = 0;
                }
            }
            /* Unten am Bauteil angekommen (legitimer Kontaktbereich): */
            else if (z_pos <= contact_zone) {
                s_ds_accel_fault_debounce = 0;
                if (ds_value >= s_ds_trigger_threshold) {
                    int32_t touch_pos = z_pos;
                    int32_t delta = touch_pos - s_scatter_stats.z_ref_pos;
                    s_scatter_stats.last_delta = delta;
                    if (touch_pos < s_scatter_stats.z_min_pos) s_scatter_stats.z_min_pos = touch_pos;
                    if (touch_pos > s_scatter_stats.z_max_pos) s_scatter_stats.z_max_pos = touch_pos;
                    s_scatter_stats.scatter_range = s_scatter_stats.z_max_pos - s_scatter_stats.z_min_pos;

                    s_scatter_stats.z_last_probe_pos = touch_pos;
                    s_probe_pos_sum += touch_pos;
                    s_probe_count++;
                    s_scatter_stats.mean_pos = (float)s_probe_pos_sum / (float)s_probe_count;

                    s_current_cycle++;
                    s_stats.completed_cycles = s_current_cycle;

                    /* Telegramm für Windows GUI senden */
                    char cycle_msg[96];
                    snprintf(cycle_msg, sizeof(cycle_msg),
                             "TEST_B_CYCLE:%lu;%ld;%ld;%ld;%ld;%ld;%.1f\r\n",
                             (unsigned long)s_current_cycle, (long)touch_pos, (long)s_scatter_stats.last_delta,
                             (long)s_scatter_stats.z_min_pos, (long)s_scatter_stats.z_max_pos,
                             (long)s_scatter_stats.scatter_range, (double)s_scatter_stats.mean_pos);
                    uart_send_text(cycle_msg, 50);

                    if (s_current_cycle >= s_num_total_cycles) {
                        return TESTRUN_COMPLETE;
                    }

                    /* Nächster Zyklus: Sofort wieder nach oben */
                    s_phase = PHASE_B_FAST_UP;
                    s_ds_accel_fault_debounce = 0;
                } else if (z_pos <= (int32_t)(z_encoder_start + 15)) {
                    /* Notstopp falls Bauteil komplett fehlt */
                    snprintf(s_error_msg, sizeof(s_error_msg), "Kein Bauteil @ %ld", (long)z_pos);
                    return TESTRUN_ERROR;
                }
            } else {
                /* In der oberen Umkehrzone (z_pos >= top_zone): Hebel federt aus, keine FehlerauslÃ¶sung */
                s_ds_accel_fault_debounce = 0;
            }
        }

        return TESTRUN_CONTINUE;
    }

    /* =========================================================================
     * TEST MODUS A: KLASSISCHER DAUERTEST (OHNE BAUTEIL)
     * ========================================================================= */
    /* In Test A dient der Drucksensor als reiner Kollisionsschutz (z.B. Hindernis) */
    if (ds_value >= s_ds_trigger_threshold) {
        s_ds_accel_fault_debounce++;
        if (s_ds_accel_fault_debounce >= 8u) {
            s_stats.invalid_sensor_events++;
            s_stats.ds_errors++;
            s_stats.motor_faults++;
            snprintf(s_error_msg, sizeof(s_error_msg), "Druck-Sen Kollision: %u @ %ld", ds_value, (long)z_pos);
            return TESTRUN_ERROR;
        }
    } else {
        s_ds_accel_fault_debounce = 0;
    }

    if (s_phase == PHASE_A_GO_UP) {
        s_cycle_start_pos = Encoder_GetPosition_Z_AXIS();
        int32_t target_up = (int32_t)z_ax_no_pos + GO_UP_OVERSHOOT;
        update_target_range(target_up);
        Z_Target_SetRequestedDirect((uint32_t)target_up);

        /* Nadel-oben Sensor (NO-Sensor) MUSS oben immer erreicht werden */
        if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) {
            s_cycle_trigger_pos = z_pos;
            s_stats.no_sensor_pos = z_ax_no_pos;
            s_stats.valid_sensor_events++;
            if (s_cycle_trigger_pos < s_stats.z_trigger_pos_min) s_stats.z_trigger_pos_min = s_cycle_trigger_pos;
            if (s_cycle_trigger_pos > s_stats.z_trigger_pos_max) s_stats.z_trigger_pos_max = s_cycle_trigger_pos;
            s_phase = PHASE_A_GO_DOWN;
        } else if (z_pos >= (int32_t)(z_ax_no_pos + GO_UP_OVERSHOOT)) {
            s_stats.no_sensor_errors++;
            s_stats.invalid_sensor_events++;
            snprintf(s_error_msg, sizeof(s_error_msg), "NO-Sen fehlt @ %ld", (long)z_pos);
            return TESTRUN_ERROR;
        }
    } else if (s_phase == PHASE_A_GO_DOWN) {
        int32_t target_down = (int32_t)z_encoder_start + GO_DOWN_TARGET_OFFS;
        update_target_range(target_down);
        Z_Target_SetRequestedDirect((uint32_t)target_down);

        if (z_pos <= (int32_t)(z_encoder_start + GO_DOWN_SWITCH_MARGIN)) {
            s_cycle_end_pos = z_pos;
            update_cycle_metrics();
            s_current_cycle++;
            s_stats.completed_cycles = s_current_cycle;
            s_phase = PHASE_A_GO_UP;

            if (s_current_cycle >= s_num_total_cycles) {
                return TESTRUN_COMPLETE;
            }
        }
    }

    return TESTRUN_CONTINUE;
}

void TestRun_GetErrorMessage(char *buf, size_t len) {
    if (buf == NULL || len == 0) return;
    strncpy(buf, s_error_msg, len - 1);
    buf[len - 1] = '\0';
}

void TestRun_GetStats(TestRunStats_t *out) {
    if (out != NULL) *out = s_stats;
}

void TestRun_GetScatterStats(TestBScatterStats_t *out) {
    if (out != NULL) *out = s_scatter_stats;
}

void TestRun_GetLastCycleMetrics(TestRunCycleMetrics_t *out) {
    if (out != NULL) *out = s_last_cycle_metrics;
}

uint32_t TestRun_GetCurrentCycle(void) { return s_current_cycle; }
uint32_t TestRun_GetTotalCycles(void)  { return s_num_total_cycles; }
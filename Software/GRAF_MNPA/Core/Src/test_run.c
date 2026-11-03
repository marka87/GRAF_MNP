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
#include <math.h>

/* Extern-Deklarationen */
extern ad5684_dac_t    dac;
extern ADC_HandleTypeDef hadc1;
extern int32_t        z_ax_no_pos;
extern int32_t        z_encoder_start;
extern int32_t        z_encoder_end;

/* Druckmotor-Spannung waehrend der Testlaeufe (Gegendruck zur Vermeidung von Fehltriggerung) */
#define TEST_D_MOT_VOLTAGE_DEFAULT   3.5f

/* GO_UP-Puffer oberhalb der NO-Sensor-Position bis zur Zielposition */
#define GO_UP_OVERSHOOT              350u
#define GO_UP_SWITCH_MARGIN          100u

/* GO_DOWN-Zielpuffer ab Encoder-Startposition */
#define GO_DOWN_TARGET_OFFS          400u
#define GO_DOWN_SWITCH_MARGIN        500u

/* Test A Zielhöhe über NO-Sensor und geschwindigkeitsabhängige Bremsweg-Kompensation */
#define TEST_A_UP_TARGET_HEIGHT      250

static int32_t get_test_a_down_overshoot(uint8_t speed) {
    if (speed <= 6u) return 10;
    int32_t os = 60 + (int32_t)(speed - 8) * 31;
    return (os < 10) ? 10 : os;
}

static int32_t get_test_a_up_overshoot(uint8_t speed) {
    if (speed <= 4u) return 10;
    int32_t os = 75 + (int32_t)(speed - 8) * 22;
    return (os < 10) ? 10 : os;
}

/* Test B Parameter */
#define TEST_B_FAST_SPEED_LEVEL      8u    /* Schnelle Verfahrfahrt (wie Naehmaschine) */
#define TEST_B_SETUP_PROBE_SPEED     3u    /* Sanfte Suchfahrt ueber die gesamte Hoehe (egal ob 2mm oder 10mm Bauteil) */
#define TEST_B_LANDING_SPEED         4u    /* Landebahn vor Kontakt: Stufe 4 (8 inc/ms = 0.20V P-Kraft) verhindert Reibungshaenger */
#define TEST_B_TRIGGER_DELTA_MV_DEF  1500u /* TTL-Logik: 1.5V Schaltschwelle ueber Ruhe-Baseline */

/* 1+N+1 Prinzip: Ersttastung (Kalibrierung) und Abschluss-Bremsfahrt.
 * Neu entworfen nach Root-Cause-Analyse einer frueheren, fehlerhaften Variante:
 * - Sanfte Stufe 2 fuer Andruecken UND Zurueckfahren (statt hartem Ruck).
 * - Echtes Baseline-Warten (nicht nur "unter Schaltschwelle") vor Testfreigabe.
 * - Vollstaendiger PID-Reset (Integratoren!) vor Start der schnellen Zyklen,
 *   da sonst eine vorgespannte Regelung die ersten Zyklen unrund machte. */
#define TEST_B_INITIAL_PROBE_SPEED   2u    /* Sanfte Stufe fuer Ersttastung-Andruecken & Ruecklauf */
#define TEST_B_INITIAL_SETTLE_MIN_MS 300u  /* Mindest-Beruhigungszeit nach Ruecklauf */
#define TEST_B_INITIAL_SETTLE_TIMEOUT_MS 2000u /* Sicherheits-Timeout falls Sensor nicht zur Baseline zurueckkehrt */
#define TEST_B_BASELINE_TOLERANCE_ADC 50u  /* Toleranzband um die echte Ruhe-Baseline (statt nur "< Schaltschwelle") */

/* Abschluss-Bremsfahrt (+1 Takt) nach dem letzten Zyklus: verhindert das
 * Ueberschiessen der Achse beim finalen Hochfahren zu Z0, indem die
 * Zielposition (NO-Sensor + Freihub) bereits ueber die normale, geschwindigkeitsabhaengige
 * Bremszonen-Logik kontrolliert angefahren wird (statt eines abrupten Direktsprungs
 * mit voller Testgeschwindigkeit nach Testabschluss). */
#define TEST_B_FINAL_BRAKE_SPEED     3u    /* Sanfte Zielgeschwindigkeit kurz vor Z0 */
#define TEST_B_FINAL_TARGET_MARGIN   50    /* Freihub oberhalb des NO-Sensors (wie bisher: z_ax_no_pos + 50) */
#define TEST_B_FINAL_BRAKE_TIMEOUT_MS 3000u /* Sicherheits-Timeout fuer die Abschluss-Bremsfahrt */

/* Dynamische Ausblend-Zone für den oberen Totpunkt:
 * Bei Stufe 16 (bis zu 985 mm/s und +8.5 g Beschleunigung) federt die Nadel
 * durch die Massenträgheit über eine Strecke von ca. 6-8 mm (600-800 Inc) in den Sensor.
 * Der Ausblendbereich passt sich daher dynamisch an die gewählte Geschwindigkeit an. */
static int32_t get_reversal_blanking_margin(uint8_t speed) {
    if (speed <= 4u) return 300;
    int32_t margin = 300 + (int32_t)(speed - 4u) * 45;
    return (margin > 850) ? 850 : margin;
}

typedef enum {
    /* Test A Phasen */
    PHASE_A_GO_UP,
    PHASE_A_GO_DOWN,

    /* Test B Phasen: 1 (Ersttastung) + N (Dauertest) + 1 (Abschluss-Bremsfahrt) */
    PHASE_B_SETUP_SLOW_PROBE,
    PHASE_B_INITIAL_DEFLECT,
    PHASE_B_INITIAL_RETRACT,
    PHASE_B_FAST_UP,
    PHASE_B_FAST_DOWN,
    PHASE_B_FINAL_BRAKE
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
static int32_t                s_calib_touch_pos = 0;
static uint16_t               s_calib_max_adc = 0;
static uint32_t               s_calib_plateau_ticks = 0;
static uint32_t               s_calib_settle_start_tick = 0;
static uint32_t               s_final_brake_start_tick = 0;
static uint32_t               s_setup_start_tick = 0;
static int64_t                s_probe_pos_sum = 0;
static uint32_t               s_probe_count = 0;
static uint8_t                s_fast_speed_level = 6u;
static uint32_t               s_ds_filter_acc = 0;
static uint32_t               s_fast_cycles_start_tick = 0;
static bool                   s_no_sen_tripped_in_cycle = false;



/* RAM-Puffer fuer Zyklus-Deltas (Streuungs-Diagramm): waehrend der schnellen
 * Zyklen wird pro Kontakt nur der Delta-Wert (2 Byte) gespeichert - KEIN
 * UART-Call im Zeit-kritischen Regelkreis (das kostete zuvor reale ms pro
 * Zyklus und stoerte die 1ms-PID-Taktung bei hohen Geschwindigkeitsstufen).
 * Der komplette Verlauf wird erst nach Testende in einem Rutsch als
 * TEST_B_CYCLE-Telegramme (gleiches Format wie bisher) ausgegeben. */
#define TEST_B_MAX_TRACKED_CYCLES 2000u
static int16_t                s_cycle_deltas[TEST_B_MAX_TRACKED_CYCLES];

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
    if (mv > 4700u) mv = 4700u;
    s_trigger_delta_mv = mv;
}

uint32_t TestRun_GetTriggerDeltaMv(void) {
    return s_trigger_delta_mv;
}

void TestRun_RestoreSpeedLevel(void) {
    Trace_Freeze();
    Trace_Stop();
    if (s_fast_speed_level >= 1u && s_fast_speed_level <= 16u) {
        Z_PID_SetSpeedLevel(s_fast_speed_level);
    }
}

TestRunMode_t TestRun_GetMode(void) {
    return s_mode;
}

const char *TestRun_GetPhaseName(void) {
    switch (s_phase) {
        case PHASE_A_GO_UP:           return "A_GO_UP";
        case PHASE_A_GO_DOWN:         return "A_GO_DOWN";
        case PHASE_B_SETUP_SLOW_PROBE:return "B_SETUP_PROBE";
        case PHASE_B_INITIAL_DEFLECT: return "B_INIT_DEFLECT";
        case PHASE_B_INITIAL_RETRACT: return "B_INIT_RETRACT";
        case PHASE_B_FAST_UP:         return "B_FAST_UP";
        case PHASE_B_FAST_DOWN:       return "B_FAST_DOWN";
        case PHASE_B_FINAL_BRAKE:     return "B_FINAL_BRAKE";
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
    s_fast_cycles_start_tick = 0;
    s_calib_touch_pos = 0;
    s_calib_max_adc = 0;
    s_calib_plateau_ticks = 0;
    s_calib_settle_start_tick = 0;
    s_final_brake_start_tick = 0;


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
    if ((uint32_t)s_ds_baseline_adc + delta_adc > 4050u) {
        delta_adc = 4050u - (uint32_t)s_ds_baseline_adc;
    }
    s_ds_trigger_threshold = s_ds_baseline_adc + (uint16_t)delta_adc;
    if (s_ds_trigger_threshold < 75u) {
        s_ds_trigger_threshold = 75u;
    }
    s_scatter_stats.baseline_adc = s_ds_baseline_adc;
    s_scatter_stats.trigger_adc  = s_ds_trigger_threshold;

    /* High-Speed RAM-Trace zurücksetzen und starten */
    Trace_Start();

    /* Kinematik-Erfassung (v_max, a_max, Bremsen) für den Testlauf zurücksetzen */
    Z_PID_ResetKinematics();

    /* Startposition erfassen und Ziel sofort auf Istwert setzen */
    int32_t z_pos_now = Encoder_GetPosition_Z_AXIS();
    if (z_pos_now < 0) z_pos_now = 0;
    s_cycle_start_pos = z_pos_now;
    update_position_range(z_pos_now);
    update_target_range(z_pos_now);
    Z_Target_SetRequestedDirect((uint32_t)z_pos_now);

    s_ds_trigger_debounce = 0;
    s_ds_accel_fault_debounce = 0;

    /* Geschwindigkeit: Direkte Übernahme ohne Level-1-Verfälschung */
    s_fast_speed_level = Z_PID_GetSpeedLevel();
    if (s_fast_speed_level < 1u) s_fast_speed_level = 1u;
    if (s_fast_speed_level > 16u) s_fast_speed_level = 16u;

    s_no_sen_tripped_in_cycle = false;
    if (s_mode == TESTRUN_MODE_B_PROBE_SCATTER) {
        /* Sanfte Suchfahrt von oben bis zum Bauteil auf Speed 3 */
        s_phase = PHASE_B_SETUP_SLOW_PROBE;
        s_setup_start_tick = HAL_GetTick();
        Z_PID_SetSpeedLevel(TEST_B_SETUP_PROBE_SPEED);
    } else {
        int32_t init_target_down = (int32_t)z_encoder_start + GO_DOWN_TARGET_OFFS;
        int32_t init_target_up = (int32_t)z_ax_no_pos + TEST_A_UP_TARGET_HEIGHT;
        update_target_range(init_target_down);
        update_target_range(init_target_up);
        s_phase = PHASE_A_GO_UP;
        Z_PID_SetSpeedLevel(s_fast_speed_level);
    }
}

TestRunResult_t TestRun_Tick(bool tick_100ms_elapsed) {
    /* Vorzeitiger Abschluss falls Zykluszahl bereits erreicht */
    if (s_current_cycle >= s_num_total_cycles) {
        TestRun_RestoreSpeedLevel();
        return TESTRUN_COMPLETE;
    }

    int32_t z_pos = Encoder_GetPosition_Z_AXIS();
    int32_t z_target = (int32_t)Z_Target_GetRequested();
    uint16_t ds_value = read_filtered_ds();


    /* High-Speed 1-ms RAM-Trace aufzeichnen */
    if (Trace_IsActive()) {
        uint8_t flags = 0;
        int32_t b_dist = (int32_t)(s_fast_speed_level * 80);
        if (b_dist < 300) b_dist = 300;
        if (s_mode == TESTRUN_MODE_B_PROBE_SCATTER && s_scatter_stats.z_ref_pos > 0 && z_pos <= (s_scatter_stats.z_ref_pos + b_dist)) {
            flags |= 0x01; // Bit 0: In Bremszone
        }
        if (ds_value >= s_ds_trigger_threshold) {
            flags |= 0x02; // Bit 1: Kontakt erkannt
        }
        if (voltage >= 4.95f || voltage <= 0.05f) {
            flags |= 0x04; // Bit 2: Sättigung
        }
        Trace_RecordSample(z_pos, z_target,
                           (int16_t)Z_PID_GetCurrentActualVelocity(),
                           (int16_t)Z_PID_GetCurrentDesiredVelocity(),
                           voltage, ds_value, (uint8_t)s_phase, flags);
    }

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

    int32_t top_zone     = (int32_t)z_ax_no_pos - get_reversal_blanking_margin(s_fast_speed_level);
    if (s_mode == TESTRUN_MODE_B_PROBE_SCATTER && s_scatter_stats.z_ref_pos > 0) {
        if (top_zone < (s_scatter_stats.z_ref_pos + 150)) {
            top_zone = s_scatter_stats.z_ref_pos + 150;
        }
    }
    int32_t contact_zone = s_scatter_stats.z_ref_pos + 35;
    int32_t landing_zone = s_scatter_stats.z_ref_pos + 150;
    int32_t braking_dist = (int32_t)(s_fast_speed_level * 80);
    if (braking_dist < 300) braking_dist = 300;
    int32_t braking_zone = s_scatter_stats.z_ref_pos + braking_dist;
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

                    /* Setup abgeschlossen -> Vor Teststart Drucksensor-Messbereich aufnehmen */
                    s_phase = PHASE_B_INITIAL_DEFLECT;
                    s_calib_touch_pos = touch_pos;
                    s_calib_max_adc = ds_value;
                    s_calib_plateau_ticks = 0;
                    Z_PID_SetSpeedLevel(TEST_B_INITIAL_PROBE_SPEED);
                    int32_t calib_target = touch_pos - 32;
                    if (calib_target < (int32_t)z_encoder_start) calib_target = (int32_t)z_encoder_start;
                    update_target_range(calib_target);
                    Z_Target_SetRequestedDirect((uint32_t)calib_target);
                }
            } else {
                s_ds_trigger_debounce = 0;
            }

            /* Unteren Anschlag erreicht, aber kein Messobjekt gefunden */
            if (z_pos <= (int32_t)(z_encoder_start + 15)) {
                snprintf(s_error_msg, sizeof(s_error_msg), "Kein Messobjekt @ %ld", (long)z_pos);
                TestRun_RestoreSpeedLevel();
                return TESTRUN_ERROR;
            }

            /* Antast-Timeout: Achse kommt nicht voran (Encoder/Motor/Endstufe) */
            if (s_phase == PHASE_B_SETUP_SLOW_PROBE && HAL_GetTick() - s_setup_start_tick > 15000u) {
                snprintf(s_error_msg, sizeof(s_error_msg), "Antast-Timeout 15s @ %ld", (long)z_pos);
                TestRun_RestoreSpeedLevel();
                return TESTRUN_ERROR;
            }
        }

        /* -----------------------------------------------------------------
         * 1b. ERSTTASTUNG/KALIBRIERUNG (NEU): Drückt von touch_pos aus sanft
         *     (Stufe 2) bis zu max. 30 Inkremente ein, um realen Federweg
         *     (inc) und Spitzenspannung (V, Ziel ca. 4,6-4,8V) fuer das
         *     Mimot-Pruefprotokoll zu erfassen. Bis zum echten mechanischen
         *     Anschlag bleiben ca. 300 Inc Puffer - 30 Inc ist unkritisch.
         * ----------------------------------------------------------------- */
        else if (s_phase == PHASE_B_INITIAL_DEFLECT) {
            if (ds_value > s_calib_max_adc) {
                s_calib_max_adc = ds_value;
                s_calib_plateau_ticks = 0;
            } else {
                s_calib_plateau_ticks++;
            }

            int32_t travel = s_calib_touch_pos - z_pos;
            bool stop_sweep = false;

            /* Sicherheits-Abbruchbedingungen:
             * 1) Maximaler Federweg erreicht: >= 30 inc (schützt Nadelmechanik vor Überlastung)
             * 2) Sensor-Vollanschlag / Sättigung: >= 3890 ADC (ca. 4.75 V)
             * 3) Plateau bei hoher Spannung: >= 3500 ADC (ca. 4.27 V) und 20 Ticks unverändert
             * 4) Unterer Z-Endanschlag */
            if (travel >= 30) {
                stop_sweep = true;
            } else if (ds_value >= 3890u) {
                stop_sweep = true;
            } else if (ds_value >= 3500u && s_calib_plateau_ticks >= 20u) {
                stop_sweep = true;
            } else if (z_pos <= (int32_t)(z_encoder_start + 15)) {
                stop_sweep = true;
            }

            if (stop_sweep) {
                int32_t final_pos = z_pos;
                int32_t final_travel = s_calib_touch_pos - final_pos;
                if (final_travel < 0) final_travel = 0;

                s_scatter_stats.contact_travel_inc = (uint32_t)final_travel;
                s_scatter_stats.peak_ds_adc = s_calib_max_adc;

                char calib_msg[100];
                snprintf(calib_msg, sizeof(calib_msg),
                         "TEST_B_CALIB:travel=%lu,peak_v=%.3f,base_v=%.3f\r\n",
                         (unsigned long)s_scatter_stats.contact_travel_inc,
                         (double)((float)s_calib_max_adc * (5.0f / 4095.0f)),
                         (double)((float)s_ds_baseline_adc * (5.0f / 4095.0f)));
                uart_send_text(calib_msg, 20);

                /* Kalibrier-Messung abgeschlossen -> Sanft entlasten und ausschwingen */
                s_phase = PHASE_B_INITIAL_RETRACT;
                s_calib_settle_start_tick = HAL_GetTick();
                Z_PID_SetSpeedLevel(TEST_B_INITIAL_PROBE_SPEED);
                int32_t settle_target = s_calib_touch_pos + 10; /* Kontaktposition mit 10 inc Freihub */
                if (settle_target > (int32_t)z_ax_no_pos) settle_target = (int32_t)z_ax_no_pos;
                update_target_range(settle_target);
                Z_Target_SetRequestedDirect((uint32_t)settle_target);
            }
        }

        /* -----------------------------------------------------------------
         * 1c. BERUHIGUNG & SICHERHEITS-CHECK NACH KALIBRIERUNG (NEU):
         *     Fährt mit Stufe 2 sanft zurück auf Kontakt-/Freihubposition und
         *     wartet, bis der Sensor WIRKLICH zur Ruhe-Baseline zurueckgekehrt
         *     ist (Toleranzband, nicht nur "unter Schaltschwelle" wie zuvor -
         *     das reichte nicht immer aus, siehe Root-Cause-Analyse). Direkt
         *     vor Testfreigabe wird der PID-Regler vollstaendig zurueckgesetzt
         *     (Integratoren!), damit Test N garantiert aus neutralem Zustand
         *     startet (vorher: Integrator-Vorspannung liess hohe Stufen unrund
         *     laufen).
         * ----------------------------------------------------------------- */
        else if (s_phase == PHASE_B_INITIAL_RETRACT) {
            Z_PID_SetSpeedLevel(TEST_B_INITIAL_PROBE_SPEED);
            int32_t settle_target = s_calib_touch_pos + 10;
            if (settle_target > (int32_t)z_ax_no_pos) settle_target = (int32_t)z_ax_no_pos;
            update_target_range(settle_target);
            Z_Target_SetRequestedDirect((uint32_t)settle_target);

            uint32_t settle_elapsed = HAL_GetTick() - s_calib_settle_start_tick;
            bool near_baseline = (ds_value <= (uint32_t)s_ds_baseline_adc + TEST_B_BASELINE_TOLERANCE_ADC);

            /* Freigabe erst nach mind. 300 ms UND wenn der Sensor wirklich auf die Baseline zurueckgekehrt ist */
            if (settle_elapsed >= TEST_B_INITIAL_SETTLE_MIN_MS && near_baseline) {
                s_phase = PHASE_B_FAST_UP;
                s_fast_cycles_start_tick = HAL_GetTick();
                s_ds_trigger_debounce = 0;
                s_ds_accel_fault_debounce = 0;
                Z_PID_SetSpeedLevel(s_fast_speed_level);
                Z_PID_Reset();          /* Integratoren/Rampenstatus vollstaendig neutralisieren */
                Z_PID_ResetKinematics();
                Trace_Start();
            }
            /* Timeout-Schutz: Falls Sensor auch nach Timeout nicht zur Baseline zurueckkehrt -> Nadel klemmt */
            else if (settle_elapsed >= TEST_B_INITIAL_SETTLE_TIMEOUT_MS) {
                snprintf(s_error_msg, sizeof(s_error_msg), "Nadel klemmt nach Kalibrierung");
                TestRun_RestoreSpeedLevel();
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
                        TestRun_RestoreSpeedLevel();
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
                /* Oben angekommen, aber NO-Sensor hat NICHT ausgelöst -> FEHLER! */
                s_stats.no_sensor_errors++;
                snprintf(s_error_msg, sizeof(s_error_msg), "NO-Sen fehlt @ %ld", (long)z_pos);
                TestRun_RestoreSpeedLevel();
                return TESTRUN_ERROR;
            }
        }
        else if (s_phase == PHASE_B_FAST_DOWN) {
            /* 2-Stufen-Bremsung mit Landebahn:
             * 1. Freiflug oberhalb braking_zone: Volle Reisegeschwindigkeit
             * 2. Bremszone (braking_zone bis landing_zone): Stufe 6 (22 Inc/ms) baut Hauptenergie ab
             * 3. Landebahn (letzte 1.5 mm / 150 Inc): Kriechstufe 3 (4 Inc/ms) gleitet stabil zum Kontakt */
            if (z_pos > braking_zone) {
                Z_PID_SetSpeedLevel(s_fast_speed_level);
            } else if (z_pos > landing_zone) {
                uint8_t mid_speed = (s_fast_speed_level > 6u) ? 6u : s_fast_speed_level;
                Z_PID_SetSpeedLevel(mid_speed);
            } else {
                Z_PID_SetSpeedLevel(TEST_B_LANDING_SPEED);
            }

            int32_t down_target = s_scatter_stats.z_ref_pos - 25;
            if (down_target < (int32_t)z_encoder_start) down_target = (int32_t)z_encoder_start;
            update_target_range(down_target);
            Z_Target_SetRequestedDirect((uint32_t)down_target);

            /* Freiflug-Absicherung für Stufe 15 & 16:
             * Flankenüberwachung erst im Nahbereich scharfschalten (z_pos <= collision_zone),
             * damit der Hebel bei 8.5g Anfahrbeschleunigung frei abheben darf ohne Fehlalarm. */
            int32_t collision_zone = s_scatter_stats.z_ref_pos + 600;
            if (collision_zone > braking_zone) collision_zone = braking_zone;

            if (z_pos <= collision_zone && z_pos > contact_zone) {
                /* Im Nahbereich nur echte Kollisionen erkennen */
                uint16_t collision_threshold = s_ds_trigger_threshold + 600u;
                if (collision_threshold < 2000u) collision_threshold = 2000u; /* ca. 2.44 V */
                if (ds_value >= collision_threshold) {
                    s_ds_accel_fault_debounce++;
                    if (s_ds_accel_fault_debounce >= 20u) {
                        s_stats.ds_errors++;
                        snprintf(s_error_msg, sizeof(s_error_msg), "Beschl. DOWN: %u @ %ld", ds_value, (long)z_pos);
                        TestRun_RestoreSpeedLevel();
                        return TESTRUN_ERROR;
                    }
                } else {
                    s_ds_accel_fault_debounce = 0;
                }
            }
            /* Unten am Messobjekt angekommen (legitimer Kontaktbereich): */
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

                    /* Delta im RAM merken (kein UART hier -> Zeit-kritischer Regelkreis bleibt unberuehrt) */
                    if (s_current_cycle < TEST_B_MAX_TRACKED_CYCLES) {
                        s_cycle_deltas[s_current_cycle] = (int16_t)delta;
                    }

                    s_current_cycle++;
                    s_stats.completed_cycles = s_current_cycle;

                    /* Keine UART-Ausgabe während schneller Fahrt -> CPU & Takt entlasten */
                    if (s_current_cycle >= s_num_total_cycles) {
                        s_stats.test_time_ms = (s_fast_cycles_start_tick > 0) ? (HAL_GetTick() - s_fast_cycles_start_tick) : 0u;
                        s_phase = PHASE_B_FINAL_BRAKE;
                        s_final_brake_start_tick = HAL_GetTick();
                        s_ds_accel_fault_debounce = 0;
                        return TESTRUN_CONTINUE;
                    }

                    /* Nächster Zyklus: Sofort wieder nach oben */
                    s_phase = PHASE_B_FAST_UP;
                    s_ds_accel_fault_debounce = 0;
                } else if (z_pos <= (int32_t)(z_encoder_start + 15)) {
                    /* Notstopp falls Messobjekt komplett fehlt */
                    snprintf(s_error_msg, sizeof(s_error_msg), "Kein Messobjekt @ %ld", (long)z_pos);
                    TestRun_RestoreSpeedLevel();
                    return TESTRUN_ERROR;
                }
            } else {
                /* In der oberen Umkehrzone (z_pos >= top_zone): Hebel federt aus, keine Fehlerauslösung */
                s_ds_accel_fault_debounce = 0;
            }
        }
        else if (s_phase == PHASE_B_FINAL_BRAKE) {
            /* 1d. SANFTE ABSCHLUSS-BREMSFAHRT:
             *     Letzter Zyklus bremst vor dem oberen Parkpunkt sanft auf Stufe 2 herunter,
             *     um mechanische Schläge und Nadelprellen beim Testende zu verhindern. */
            int32_t final_target = (int32_t)(z_ax_no_pos + TEST_B_FINAL_TARGET_MARGIN);
            int32_t final_brake_start = final_target - braking_dist;

            if (z_pos < final_brake_start) {
                Z_PID_SetSpeedLevel(s_fast_speed_level);
            } else {
                Z_PID_SetSpeedLevel(TEST_B_FINAL_BRAKE_SPEED);
            }
            update_target_range(final_target);
            Z_Target_SetRequestedDirect((uint32_t)final_target);

            if (z_pos > takeoff_zone && z_pos < top_zone) {
                if (ds_value >= s_ds_trigger_threshold) {
                    s_ds_accel_fault_debounce++;
                    if (s_ds_accel_fault_debounce >= 20u) {
                        s_stats.ds_errors++;
                        snprintf(s_error_msg, sizeof(s_error_msg), "Beschl. FINAL: %u @ %ld", ds_value, (long)z_pos);
                        TestRun_RestoreSpeedLevel();
                        return TESTRUN_ERROR;
                    }
                } else {
                    s_ds_accel_fault_debounce = 0;
                }
            } else {
                s_ds_accel_fault_debounce = 0;
            }

            bool no_sen_tripped = (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET);
            if (no_sen_tripped || z_pos >= final_target) {
                s_stats.valid_sensor_events++;
                TestRun_RestoreSpeedLevel();
                return TESTRUN_COMPLETE;
            }

            if (HAL_GetTick() - s_final_brake_start_tick >= TEST_B_FINAL_BRAKE_TIMEOUT_MS) {
                snprintf(s_error_msg, sizeof(s_error_msg), "Abschluss-Bremsfahrt Timeout @ %ld", (long)z_pos);
                TestRun_RestoreSpeedLevel();
                return TESTRUN_ERROR;
            }
        }

        return TESTRUN_CONTINUE;
    }

    /* =========================================================================
     * TEST MODUS A: KLASSISCHER DAUERTEST (OHNE BAUTEIL)
     * ========================================================================= */
    /* In Test A dient der Drucksensor als reiner Kollisionsschutz (z.B. Hindernis).
     * In der oberen Umkehrzone (z_pos >= top_zone) federt der Hebel/Sensor durch Bremsung/Umkehr
     * aus -> dort wie in Test B ausblenden. */
    uint16_t test_a_col_thresh = s_ds_trigger_threshold + 600u;
    if (test_a_col_thresh < 2000u) test_a_col_thresh = 2000u;
    if (z_pos < top_zone && ds_value >= test_a_col_thresh) {
        s_ds_accel_fault_debounce++;
        if (s_ds_accel_fault_debounce >= 20u) {
            s_stats.invalid_sensor_events++;
            s_stats.ds_errors++;
            s_stats.motor_faults++;
            snprintf(s_error_msg, sizeof(s_error_msg), "Druck-Sen Kollision: %u @ %ld", ds_value, (long)z_pos);
            TestRun_RestoreSpeedLevel();
            return TESTRUN_ERROR;
        }
    } else {
        s_ds_accel_fault_debounce = 0;
    }

    if (s_phase == PHASE_A_GO_UP) {
        s_cycle_start_pos = Encoder_GetPosition_Z_AXIS();
        int32_t target_up = (int32_t)z_ax_no_pos + TEST_A_UP_TARGET_HEIGHT;
        update_target_range(target_up);
        Z_Target_SetRequestedDirect((uint32_t)target_up);

        int32_t switch_up = target_up - get_test_a_up_overshoot(s_fast_speed_level);
        if (switch_up < (int32_t)z_ax_no_pos) switch_up = (int32_t)z_ax_no_pos;

        /* Nadel-oben Sensor (NO-Sensor) erfassen */
        if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) {
            if (!s_no_sen_tripped_in_cycle) {
                s_no_sen_tripped_in_cycle = true;
                s_cycle_trigger_pos = z_pos;
                s_stats.no_sensor_pos = z_ax_no_pos;
                s_stats.valid_sensor_events++;
                if (s_cycle_trigger_pos < s_stats.z_trigger_pos_min) s_stats.z_trigger_pos_min = s_cycle_trigger_pos;
                if (s_cycle_trigger_pos > s_stats.z_trigger_pos_max) s_stats.z_trigger_pos_max = s_cycle_trigger_pos;
            }
        }

        if (s_no_sen_tripped_in_cycle) {
            if (z_pos >= switch_up) {
                s_phase = PHASE_A_GO_DOWN;
                s_no_sen_tripped_in_cycle = false;
            }
        } else if (z_pos >= (int32_t)(target_up + 100)) {
            s_stats.no_sensor_errors++;
            s_stats.invalid_sensor_events++;
            snprintf(s_error_msg, sizeof(s_error_msg), "NO-Sen fehlt @ %ld", (long)z_pos);
            TestRun_RestoreSpeedLevel();
            return TESTRUN_ERROR;
        }
    } else if (s_phase == PHASE_A_GO_DOWN) {
        int32_t target_down = (int32_t)z_encoder_start + GO_DOWN_TARGET_OFFS;
        update_target_range(target_down);
        Z_Target_SetRequestedDirect((uint32_t)target_down);

        int32_t switch_down = target_down + get_test_a_down_overshoot(s_fast_speed_level);
        if (z_pos <= switch_down) {
            s_cycle_end_pos = z_pos;
            update_cycle_metrics();
            s_current_cycle++;
            s_stats.completed_cycles = s_current_cycle;
            s_phase = PHASE_A_GO_UP;
            s_no_sen_tripped_in_cycle = false;

            if (s_current_cycle >= s_num_total_cycles) {
                TestRun_RestoreSpeedLevel();
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
    if (out != NULL) {
        s_stats.max_velocity_mms = Z_PID_GetMaxVelocity_mm_s();
        s_stats.max_accel_g = Z_PID_GetMaxAccel_g();
        s_stats.max_decel_g = Z_PID_GetMaxDecel_g();
        *out = s_stats;
    }
}

void TestRun_GetScatterStats(TestBScatterStats_t *out) {
    if (out != NULL) {
        s_scatter_stats.fast_speed_level = s_fast_speed_level;
        s_scatter_stats.max_velocity_mms = Z_PID_GetMaxVelocity_mm_s();
        s_scatter_stats.max_accel_g = Z_PID_GetMaxAccel_g();
        s_scatter_stats.max_decel_g = Z_PID_GetMaxDecel_g();
        *out = s_scatter_stats;
    }
}

/* Gibt die im RAM gesammelten Zyklus-Deltas nach Testende als TEST_B_CYCLE-Telegramme
 * aus (gleiches Format wie eine Live-Uebertragung waere, nur gesammelt zum sicheren
 * Zeitpunkt nach Testabschluss - kein Einfluss auf die Regelschleife waehrend des Tests). */
void TestRun_DumpCyclesUART(void) {
    uint32_t count = (s_current_cycle < TEST_B_MAX_TRACKED_CYCLES) ? s_current_cycle : TEST_B_MAX_TRACKED_CYCLES;
    for (uint32_t i = 0; i < count; i++) {
        int32_t touch_pos = s_scatter_stats.z_ref_pos + s_cycle_deltas[i];
        char cycle_msg[120];
        snprintf(cycle_msg, sizeof(cycle_msg),
                 "TEST_B_CYCLE:%lu;%ld;%d;%ld;%ld;%ld;%.1f;%lu\r\n",
                 (unsigned long)(i + 1), (long)touch_pos, (int)s_cycle_deltas[i],
                 (long)s_scatter_stats.z_min_pos, (long)s_scatter_stats.z_max_pos,
                 (long)s_scatter_stats.scatter_range, (double)s_scatter_stats.mean_pos,
                 0UL);
        uart_send_text(cycle_msg, 20);
    }
}

void TestRun_GetLastCycleMetrics(TestRunCycleMetrics_t *out) {
    if (out != NULL) *out = s_last_cycle_metrics;
}

uint32_t TestRun_GetCurrentCycle(void) { return s_current_cycle; }
uint32_t TestRun_GetTotalCycles(void)  { return s_num_total_cycles; }

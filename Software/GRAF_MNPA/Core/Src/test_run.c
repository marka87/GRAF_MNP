/*
 * test_run.c - Testablauf (TEST_RUN State Handler)
 *
 * Verwaltet den zyklischen Z-Achsen-Testablauf:
 *   - Druckmotor-Aktivierung während des Tests
 *   - GO_UP: Zielposition z_ax_no_pos+200, prüft NO-Sensor
 *   - GO_DOWN: Zielposition z_encoder_start+400, zählt Zyklen
 *   - Drucksensor-Plausibilitätsprüfung alle 100 ms
 *   - Statistikerfassung (Triggerposition, Fehlerzähler)
 *
 * Created: 2025
 * Author:  Mark Angyal
 */

#include "test_run.h"
#include "ADC_read.h"
#include "encoder.h"
#include "d_mot_control.h"
#include "data_buffer.h"
#include "Reference_Run.h"
#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Extern-Deklarationen (Definitionen in Reference_Run.c und Z_PID_Control.c) */
extern ad5684_dac_t    dac;
extern ADC_HandleTypeDef hadc1;
extern uint32_t        z_ax_no_pos;

/* Druckmotor-Spannung während des Tests */
#define TEST_D_MOT_VOLTAGE   3.5f

/* Drucksensor-Schwellwert: ADC > Threshold → Sensor NICHT aktiv → Fehler */
#define DS_ACTIVE_THRESHOLD  410u

/* GO_UP-Puffer oberhalb der NO-Sensor-Position bis zur Zielposition */
#define GO_UP_OVERSHOOT      200u
/* Mindestposition zum Wechseln von GO_UP zu GO_DOWN */
#define GO_UP_SWITCH_MARGIN  100u

/* GO_DOWN-Zielpuffer ab Encoder-Startposition */
#define GO_DOWN_TARGET_OFFS  400u
/* Wechselschwelle GO_DOWN → GO_UP */
#define GO_DOWN_SWITCH_MARGIN 500u

typedef enum {
    PHASE_GO_UP,
    PHASE_GO_DOWN,
} TestRunPhase_t;

static uint32_t        s_num_total_cycles;
static uint32_t        s_current_cycle;
static TestRunPhase_t  s_phase;
static char            s_error_msg[40];
static TestRunStats_t  s_stats;

void TestRun_Init(uint32_t num_cycles) {
    s_num_total_cycles = num_cycles;
    s_current_cycle    = 0;
    s_phase            = PHASE_GO_UP;
    s_error_msg[0]     = '\0';

    s_stats.total_cycles      = num_cycles;
    s_stats.completed_cycles  = 0;
    s_stats.ds_errors         = 0;
    s_stats.no_sensor_errors  = 0;
    s_stats.z_trigger_pos_min = (int32_t)0x7FFFFFFF;
    s_stats.z_trigger_pos_max = (int32_t)0x80000000;

    /* Datenpuffer zurücksetzen, damit der neue Test sauber startet */
    data_buffer_reset();

    /* Zielposition sofort auf Istwert setzen: kein Sprung beim Teststart */
    int32_t z_pos_now = Encoder_GetPosition_Z_AXIS();
    if (z_pos_now < 0) z_pos_now = 0;
    Z_Target_SetRequestedDirect((uint32_t)z_pos_now);
}

TestRunResult_t TestRun_Tick(bool tick_100ms_elapsed) {
    /* Druckmotor kontinuierlich aktiv halten */
    d_mot_control(&dac, TEST_D_MOT_VOLTAGE);

    /* Vorzeitiger Abschluss falls Zykluszahl bereits erreicht */
    if (s_current_cycle >= s_num_total_cycles) {
        return TESTRUN_COMPLETE;
    }

    /* Sensorlogik und Zielpositionswechsel nur im 100-ms-Raster */
    if (!tick_100ms_elapsed) {
        return TESTRUN_CONTINUE;
    }

    int32_t z_pos = Encoder_GetPosition_Z_AXIS();
    log_data_point(z_pos, (int32_t)Z_Target_GetRequested());

    /* Drucksensor muss aktiv sein (Hebel in Lichtschranke) */
    uint16_t ds_value = ADC_Drucksensor(&hadc1);
    if (ds_value > DS_ACTIVE_THRESHOLD) {
        s_stats.ds_errors++;
        snprintf(s_error_msg, sizeof(s_error_msg), "Fehler: Drucksensor");
        return TESTRUN_ERROR;
    }

    if (s_phase == PHASE_GO_UP) {
        Z_Target_SetRequestedDirect(z_ax_no_pos + GO_UP_OVERSHOOT);

        /* Nadel-oben-Bereich erreicht: NO-Sensor prüfen */
        if (z_pos > (int32_t)(z_ax_no_pos + GO_UP_SWITCH_MARGIN)) {
            if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) {
                /* NO-Sensor aktiv: Triggerposition für Statistik aufzeichnen */
                if (z_pos < s_stats.z_trigger_pos_min) s_stats.z_trigger_pos_min = z_pos;
                if (z_pos > s_stats.z_trigger_pos_max) s_stats.z_trigger_pos_max = z_pos;
                s_phase = PHASE_GO_DOWN;
            } else {
                s_stats.no_sensor_errors++;
                snprintf(s_error_msg, sizeof(s_error_msg), "Fehler: NO-Sensor");
                return TESTRUN_ERROR;
            }
        }

    } else { /* PHASE_GO_DOWN */
        Z_Target_SetRequestedDirect(z_encoder_start + GO_DOWN_TARGET_OFFS);

        if (z_pos <= (int32_t)(z_encoder_start + GO_DOWN_SWITCH_MARGIN)) {
            s_current_cycle++;
            s_stats.completed_cycles = s_current_cycle;
            s_phase = PHASE_GO_UP;

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

uint32_t TestRun_GetCurrentCycle(void) { return s_current_cycle; }
uint32_t TestRun_GetTotalCycles(void)  { return s_num_total_cycles; }

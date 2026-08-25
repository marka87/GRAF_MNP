/*
 * test_run.h - Testablauf (TEST_RUN State)
 *
 * Kapselt den zyklischen Z-Achsen-Testablauf (GO_UP / GO_DOWN) und
 * erfasst Statistiken (Zyklen, Sensorfehler, Triggerposition).
 *
 * Created: 2025
 * Author:  Mark Angyal
 */

#ifndef SRC_TEST_RUN_H_
#define SRC_TEST_RUN_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Rückgabewert von TestRun_Tick() */
typedef enum {
    TESTRUN_CONTINUE,   /* Test läuft weiter              */
    TESTRUN_COMPLETE,   /* Alle Zyklen abgeschlossen       */
    TESTRUN_ERROR,      /* Sensorfehler erkannt, Abbruch   */
} TestRunResult_t;

/* Statistiken eines abgeschlossenen Testablaufs */
typedef struct {
    uint32_t total_cycles;        /* Gesamtzahl der Soll-Zyklen                */
    uint32_t completed_cycles;    /* Tatsächlich abgeschlossene Zyklen         */
    uint32_t ds_errors;           /* Drucksensor-Fehlerzähler                  */
    uint32_t no_sensor_errors;    /* NO-Sensor-Fehlerzähler                    */
    uint32_t valid_sensor_events; /* Korrekt erkannte Drucksensor-Auslösungen */
    uint32_t invalid_sensor_events; /* Unplausible Drucksensor-Auslösungen     */
    uint32_t motor_faults;        /* Fehlerhafte Druckmotor-Betätigung         */
    int32_t  no_sensor_pos;       /* Z-Position des NO-Sensors                 */
    int32_t  z_trigger_pos_min;   /* Kleinste Z-Position beim NO-Sen-Auslösen   */
    int32_t  z_trigger_pos_max;   /* Größte Z-Position beim NO-Sen-Auslösen    */
    int32_t  z_ist_min;           /* Kleinster gemessener Istwert             */
    int32_t  z_ist_max;           /* Größter gemessener Istwert               */
    int32_t  z_soll_min;          /* Kleinster gemessener Sollwert            */
    int32_t  z_soll_max;          /* Größter gemessener Sollwert              */
    int32_t  last_ist_pos;        /* Letzte gemessene Ist-Position            */
    int32_t  last_soll_pos;       /* Letzter Sollwert                         */
    uint32_t test_time_ms;        /* Gesamtdauer des Testlaufs                */
    int32_t  last_cycle_delta;    /* Zugrundeliegende Bewegungsdifferenz       */
    int32_t  last_cycle_overshoot;/* Überschreitung der Ziel-Range in inc      */
    int32_t  last_cycle_lost_steps;/* Bewegungsverlust / fehlende Schritte     */
} TestRunStats_t;

typedef struct {
    uint32_t cycle_index;
    int32_t  start_pos;
    int32_t  trigger_pos;
    int32_t  down_target;
    int32_t  end_pos;
    int32_t  travel_to_trigger;
    int32_t  overshoot;
    int32_t  lost_steps;
} TestRunCycleMetrics_t;

/* Test initialisieren (vor jedem Teststart aufrufen) */
void             TestRun_Init(uint32_t num_cycles);

/* Tick-Funktion — einmal pro Hauptschleifen-Iteration aufrufen.
 * tick_100ms_elapsed: true wenn seit dem letzten Aufruf 100 ms vergangen sind. */
TestRunResult_t  TestRun_Tick(bool tick_100ms_elapsed);

/* Fehlermeldung des letzten TESTRUN_ERROR abrufen */
void             TestRun_GetErrorMessage(char *buf, size_t len);

/* Statistiken des letzten / laufenden Tests abrufen */
void             TestRun_GetStats(TestRunStats_t *out);

/* Messdaten des zuletzt abgeschlossenen Zyklus abrufen */
void             TestRun_GetLastCycleMetrics(TestRunCycleMetrics_t *out);

/* Hilfsfunktionen für Displayanzeige */
uint32_t         TestRun_GetCurrentCycle(void);
uint32_t         TestRun_GetTotalCycles(void);
const char      *TestRun_GetPhaseName(void);

#endif /* SRC_TEST_RUN_H_ */

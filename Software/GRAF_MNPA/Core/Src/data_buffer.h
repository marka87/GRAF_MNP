/*
 * data_buffer.h
 *
 * High-Speed RAM Flight-Recorder (1ms Takt)
 * Speichert bis zu TRACE_MAX_SAMPLES im STM32-SRAM für antriebstechnische Analysen.
 */

#ifndef SRC_DATA_BUFFER_H_
#define SRC_DATA_BUFFER_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define TRACE_MAX_SAMPLES 2000

typedef struct {
    uint32_t t_ms;           /* Relativer Zeitstempel in ms seit Trace-Start */
    int32_t  z_ist;          /* Ist-Position Z-Achse (Inkremente) */
    int32_t  z_soll;         /* Soll-Position Z-Achse (Inkremente) */
    int16_t  v_ist;          /* Ist-Geschwindigkeit (Inkremente / ms) */
    int16_t  v_soll;         /* Soll-Geschwindigkeit (Inkremente / ms) */
    int16_t  u_mot_mv;       /* Motorspannung DAC in mV (0..5000 mV, 2500mV = 0V) */
    uint16_t ds_mv;          /* Drucksensor-Spannung in mV */
    uint8_t  phase;          /* Aktuelle Phase */
    uint8_t  flags;          /* Bit 0: Bremszone, Bit 1: Kontakt getriggert, Bit 2: Sättigung */
} TraceSample_t;

typedef enum {
    TRACE_MODE_START = 0,  /* Zeichnet erste 2000 ms ab Start auf und stoppt */
    TRACE_MODE_RING  = 1   /* Kontinuierlicher Pre-Trigger Ringspeicher */
} TraceMode_t;

void        Trace_Reset(void);
void        Trace_Start(void);
void        Trace_Stop(void);
void        Trace_Freeze(void);
void        Trace_TriggerFreeze(uint16_t post_trigger_samples);
bool        Trace_IsFrozen(void);
bool        Trace_IsActive(void);
uint16_t    Trace_GetCount(void);
void        Trace_SetMode(TraceMode_t mode);
TraceMode_t Trace_GetMode(void);

void Trace_RecordSample(int32_t z_ist, int32_t z_soll, int16_t v_ist, int16_t v_soll,
                        float u_mot_v, uint16_t ds_adc, uint8_t phase, uint8_t flags);

void Trace_DumpUART(void);

/* Legacy-Kompatibilität */
void data_buffer_reset(void);
void log_data_point(int32_t position, int32_t target);
void save_data_to_uart(void);

#endif /* SRC_DATA_BUFFER_H_ */

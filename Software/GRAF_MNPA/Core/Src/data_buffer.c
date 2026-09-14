/*
 * data_buffer.c
 *
 * High-Speed RAM Flight-Recorder (1ms Takt)
 * Speichert bis zu TRACE_MAX_SAMPLES im STM32-SRAM für antriebstechnische Analysen.
 */

#include "data_buffer.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
void uart_send_text(const char *text, uint32_t timeout);

static TraceSample_t s_trace_buffer[TRACE_MAX_SAMPLES];
static uint16_t s_trace_count = 0;
static uint16_t s_head_idx = 0;
static bool     s_is_full = false;
static bool     s_frozen = false;
static uint16_t s_post_trigger_remaining = 0;
static uint32_t s_trace_start_tick = 0;
static uint32_t s_last_record_tick = 0xFFFFFFFF;
static bool     s_trace_active = false;
static TraceMode_t s_trace_mode = TRACE_MODE_RING;

void Trace_SetMode(TraceMode_t mode) {
    s_trace_mode = mode;
}

TraceMode_t Trace_GetMode(void) {
    return s_trace_mode;
}

void Trace_Freeze(void) {
    s_frozen = true;
    s_post_trigger_remaining = 0;
}

void Trace_TriggerFreeze(uint16_t post_trigger_samples) {
    if (!s_frozen && s_post_trigger_remaining == 0) {
        s_post_trigger_remaining = (post_trigger_samples > 0) ? post_trigger_samples : 1;
    }
}

bool Trace_IsFrozen(void) {
    return s_frozen;
}

void Trace_Reset(void) {
    s_trace_count = 0;
    s_head_idx = 0;
    s_is_full = false;
    s_frozen = false;
    s_post_trigger_remaining = 0;
    s_trace_start_tick = 0;
    s_last_record_tick = 0xFFFFFFFF;
    s_trace_active = false;
}

void Trace_Start(void) {
    s_trace_count = 0;
    s_head_idx = 0;
    s_is_full = false;
    s_frozen = false;
    s_post_trigger_remaining = 0;
    s_trace_start_tick = HAL_GetTick();
    s_last_record_tick = 0xFFFFFFFF;
    s_trace_active = true;
}

void Trace_Stop(void) {
    s_trace_active = false;
}

bool Trace_IsActive(void) {
    return s_trace_active && !s_frozen;
}

uint16_t Trace_GetCount(void) {
    return (s_trace_mode == TRACE_MODE_RING && s_is_full) ? TRACE_MAX_SAMPLES : s_trace_count;
}

void Trace_RecordSample(int32_t z_ist, int32_t z_soll, int16_t v_ist, int16_t v_soll,
                        float u_mot_v, uint16_t ds_adc, uint8_t phase, uint8_t flags) {
    if (!s_trace_active || s_frozen) {
        return;
    }

    if (s_trace_mode == TRACE_MODE_START && s_trace_count >= TRACE_MAX_SAMPLES) {
        s_trace_active = false;
        return;
    }

    uint32_t now = HAL_GetTick();
    if (now == s_last_record_tick) {
        return;
    }
    s_last_record_tick = now;

    if (s_post_trigger_remaining > 0) {
        s_post_trigger_remaining--;
        if (s_post_trigger_remaining == 0) {
            s_frozen = true;
        }
    }

    /* DAC Spannung in mV: 0..5000 mV */
    int16_t u_mv = (int16_t)(u_mot_v * 1000.0f);

    /* Drucksensor in mV: ADC * (5000 / 4095) */
    uint16_t ds_mv = (uint16_t)(((uint32_t)ds_adc * 5000u) / 4095u);

    uint16_t write_idx;
    if (s_trace_mode == TRACE_MODE_RING) {
        write_idx = s_head_idx;
        s_head_idx++;
        if (s_head_idx >= TRACE_MAX_SAMPLES) {
            s_head_idx = 0;
            s_is_full = true;
        }
        if (!s_is_full) {
            s_trace_count = s_head_idx;
        } else {
            s_trace_count = TRACE_MAX_SAMPLES;
        }
    } else {
        write_idx = s_trace_count;
        s_trace_count++;
    }

    s_trace_buffer[write_idx].t_ms = now;
    s_trace_buffer[write_idx].z_ist = z_ist;
    s_trace_buffer[write_idx].z_soll = z_soll;
    s_trace_buffer[write_idx].v_ist = v_ist;
    s_trace_buffer[write_idx].v_soll = v_soll;
    s_trace_buffer[write_idx].u_mot_mv = u_mv;
    s_trace_buffer[write_idx].ds_mv = ds_mv;
    s_trace_buffer[write_idx].phase = phase;
    s_trace_buffer[write_idx].flags = flags;
}

void Trace_DumpUART(void) {
    uint16_t total = (s_trace_mode == TRACE_MODE_RING && s_is_full) ? TRACE_MAX_SAMPLES : s_trace_count;
    if (total == 0) {
        uart_send_text("TRACE_EMPTY\r\n", 50);
        return;
    }

    char hdr[80];
    snprintf(hdr, sizeof(hdr), "TRACE_START:samples=%u\r\n", (unsigned int)total);
    uart_send_text(hdr, 100);
    uart_send_text("ms;z_ist;z_soll;v_ist;v_soll;u_mot_mv;ds_mv;phase;flags\r\n", 100);

    uint16_t start_idx = 0;
    if (s_trace_mode == TRACE_MODE_RING && s_is_full) {
        start_idx = s_head_idx;
    }
    uint32_t t_zero = s_trace_buffer[start_idx].t_ms;

    char chunk[512];
    uint16_t chunk_len = 0;

    for (uint16_t i = 0; i < total; i++) {
        uint16_t idx;
        if (s_trace_mode == TRACE_MODE_RING && s_is_full) {
            idx = (uint16_t)((start_idx + i) % TRACE_MAX_SAMPLES);
        } else {
            idx = i;
        }

        uint32_t rel_ms = (s_trace_buffer[idx].t_ms >= t_zero) ?
                          (s_trace_buffer[idx].t_ms - t_zero) : i;

        char line[64];
        int n = snprintf(line, sizeof(line), "%lu;%ld;%ld;%d;%d;%d;%u;%u;%u\r\n",
                         (unsigned long)rel_ms,
                         (long)s_trace_buffer[idx].z_ist,
                         (long)s_trace_buffer[idx].z_soll,
                         (int)s_trace_buffer[idx].v_ist,
                         (int)s_trace_buffer[idx].v_soll,
                         (int)s_trace_buffer[idx].u_mot_mv,
                         (unsigned int)s_trace_buffer[idx].ds_mv,
                         (unsigned int)s_trace_buffer[idx].phase,
                         (unsigned int)s_trace_buffer[idx].flags);
        if (n <= 0) continue;

        if (chunk_len + n >= sizeof(chunk)) {
            HAL_UART_Transmit(&huart1, (const uint8_t *)chunk, chunk_len, 1000);
            chunk_len = 0;
        }
        memcpy(&chunk[chunk_len], line, (size_t)n);
        chunk_len += (uint16_t)n;
    }

    if (chunk_len > 0) {
        HAL_UART_Transmit(&huart1, (const uint8_t *)chunk, chunk_len, 1000);
    }
    uart_send_text("TRACE_END\r\n", 100);
}

/* Legacy Kompatibilität */
void data_buffer_reset(void) {
    Trace_Reset();
}

void log_data_point(int32_t position, int32_t target) {
    (void)position;
    (void)target;
}

void save_data_to_uart(void) {
    Trace_DumpUART();
}

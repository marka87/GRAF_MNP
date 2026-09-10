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
static uint32_t s_trace_start_tick = 0;
static uint32_t s_last_record_tick = 0xFFFFFFFF;
static bool s_trace_active = false;

void Trace_Reset(void) {
    s_trace_count = 0;
    s_trace_start_tick = 0;
    s_last_record_tick = 0xFFFFFFFF;
    s_trace_active = false;
}

void Trace_Start(void) {
    s_trace_count = 0;
    s_trace_start_tick = HAL_GetTick();
    s_last_record_tick = 0xFFFFFFFF;
    s_trace_active = true;
}

void Trace_Stop(void) {
    s_trace_active = false;
}

bool Trace_IsActive(void) {
    return s_trace_active;
}

uint16_t Trace_GetCount(void) {
    return s_trace_count;
}

void Trace_RecordSample(int32_t z_ist, int32_t z_soll, int16_t v_ist, int16_t v_soll,
                        float u_mot_v, uint16_t ds_adc, uint8_t phase, uint8_t flags) {
    if (!s_trace_active || s_trace_count >= TRACE_MAX_SAMPLES) {
        if (s_trace_count >= TRACE_MAX_SAMPLES) {
            s_trace_active = false;
        }
        return;
    }

    uint32_t now = HAL_GetTick();
    if (now == s_last_record_tick) {
        return;
    }
    s_last_record_tick = now;
    uint32_t dt = (s_trace_start_tick > 0) ? (now - s_trace_start_tick) : 0u;

    /* DAC Spannung in mV: 0..5000 mV */
    int16_t u_mv = (int16_t)(u_mot_v * 1000.0f);

    /* Drucksensor in mV: ADC * (5000 / 4095) */
    uint16_t ds_mv = (uint16_t)(((uint32_t)ds_adc * 5000u) / 4095u);

    s_trace_buffer[s_trace_count].t_ms = dt;
    s_trace_buffer[s_trace_count].z_ist = z_ist;
    s_trace_buffer[s_trace_count].z_soll = z_soll;
    s_trace_buffer[s_trace_count].v_ist = v_ist;
    s_trace_buffer[s_trace_count].v_soll = v_soll;
    s_trace_buffer[s_trace_count].u_mot_mv = u_mv;
    s_trace_buffer[s_trace_count].ds_mv = ds_mv;
    s_trace_buffer[s_trace_count].phase = phase;
    s_trace_buffer[s_trace_count].flags = flags;

    s_trace_count++;
}

void Trace_DumpUART(void) {
    if (s_trace_count == 0) {
        uart_send_text("TRACE_EMPTY\r\n", 50);
        return;
    }

    char hdr[80];
    snprintf(hdr, sizeof(hdr), "TRACE_START:samples=%u\r\n", (unsigned int)s_trace_count);
    uart_send_text(hdr, 100);
    uart_send_text("ms;z_ist;z_soll;v_ist;v_soll;u_mot_mv;ds_mv;phase;flags\r\n", 100);

    char chunk[512];
    uint16_t chunk_len = 0;

    for (uint16_t i = 0; i < s_trace_count; i++) {
        char line[64];
        int n = snprintf(line, sizeof(line), "%lu;%ld;%ld;%d;%d;%d;%u;%u;%u\r\n",
                         (unsigned long)s_trace_buffer[i].t_ms,
                         (long)s_trace_buffer[i].z_ist,
                         (long)s_trace_buffer[i].z_soll,
                         (int)s_trace_buffer[i].v_ist,
                         (int)s_trace_buffer[i].v_soll,
                         (int)s_trace_buffer[i].u_mot_mv,
                         (unsigned int)s_trace_buffer[i].ds_mv,
                         (unsigned int)s_trace_buffer[i].phase,
                         (unsigned int)s_trace_buffer[i].flags);
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

/*
 * encoder_performance.c
 *
 * Performance measurement implementation for encoder position functions
 */

#include "encoder_performance.h"
#include "encoder.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

static uint32_t perf_sample_buffer[PERF_NUM_SAMPLES];

// Inline function to get CPU cycle count
static inline uint32_t get_cycle_count(void) {
    return DWT->CYCCNT;
}

// Quicksort for statistics calculation
static void quicksort_uint32(uint32_t *arr, int left, int right) {
    if (left >= right) return;
    
    int i = left, j = right;
    uint32_t pivot = arr[(left + right) / 2];
    
    while (i <= j) {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        if (i <= j) {
            uint32_t tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++;
            j--;
        }
    }
    
    if (left < j) quicksort_uint32(arr, left, j);
    if (i < right) quicksort_uint32(arr, i, right);
}

static void calculate_stats(uint32_t *samples, int count, encoder_perf_stats_t *stats) {
    if (count == 0) return;
    
    // Sort samples for percentile calculation
    quicksort_uint32(samples, 0, count - 1);
    
    stats->min_cycles = samples[0];
    stats->max_cycles = samples[count - 1];
    stats->median_cycles = samples[count / 2];
    stats->p95_cycles = samples[(int)(count * 0.95)];
    stats->p99_cycles = samples[(int)(count * 0.99)];
    
    // Calculate average
    uint64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    stats->avg_cycles = (uint32_t)(sum / count);
}

void encoder_perf_measure_init(void) {
    // Enable DWT (Data Watchpoint and Trace) for cycle counting
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    
    // Reset and enable cycle counter
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void encoder_perf_measure_a_axis(encoder_perf_stats_t *stats) {
    char msg[128];
    
    HAL_UART_Transmit(&huart1, (uint8_t*)"[PERF] Measuring A-AXIS encoder latency...\r\n", 45, 1000);
    
    // Warmup
    for (int i = 0; i < 100; i++) {
        Encoder_GetPosition_A_AXIS();
    }
    
    // Measurement
    for (int i = 0; i < PERF_NUM_SAMPLES; i++) {
        uint32_t start = get_cycle_count();
        int32_t pos = Encoder_GetPosition_A_AXIS();
        uint32_t end = get_cycle_count();
        
        perf_sample_buffer[i] = end - start;
    }
    
    calculate_stats(perf_sample_buffer, PERF_NUM_SAMPLES, stats);
    
    snprintf(msg, sizeof(msg), "[PERF A-AXIS] Min: %lu, Avg: %lu, Med: %lu, P95: %lu, P99: %lu, Max: %lu cycles\r\n",
             stats->min_cycles, stats->avg_cycles, stats->median_cycles,
             stats->p95_cycles, stats->p99_cycles, stats->max_cycles);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
}

void encoder_perf_measure_z_axis(encoder_perf_stats_t *stats) {
    char msg[128];
    
    HAL_UART_Transmit(&huart1, (uint8_t*)"[PERF] Measuring Z-AXIS encoder latency...\r\n", 45, 1000);
    
    // Warmup
    for (int i = 0; i < 100; i++) {
        Encoder_GetPosition_Z_AXIS();
    }
    
    // Measurement
    for (int i = 0; i < PERF_NUM_SAMPLES; i++) {
        uint32_t start = get_cycle_count();
        int32_t pos = Encoder_GetPosition_Z_AXIS();
        uint32_t end = get_cycle_count();
        
        perf_sample_buffer[i] = end - start;
    }
    
    calculate_stats(perf_sample_buffer, PERF_NUM_SAMPLES, stats);
    
    snprintf(msg, sizeof(msg), "[PERF Z-AXIS] Min: %lu, Avg: %lu, Med: %lu, P95: %lu, P99: %lu, Max: %lu cycles\r\n",
             stats->min_cycles, stats->avg_cycles, stats->median_cycles,
             stats->p95_cycles, stats->p99_cycles, stats->max_cycles);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
}

void encoder_perf_print_results(void) {
    encoder_perf_stats_t stats_a, stats_z;
    char msg[256];
    
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n=== ENCODER PERFORMANCE MEASUREMENT ===\r\n", 43, 1000);
    
    // Measure both axes
    encoder_perf_measure_a_axis(&stats_a);
    HAL_Delay(500);
    encoder_perf_measure_z_axis(&stats_z);
    
    // Get system clock frequency for cycle-to-microsecond conversion
    // STM32F7xx runs at typically 216 MHz
    uint32_t freq_mhz = 216;
    
    snprintf(msg, sizeof(msg), "\r\n[RESULTS] A-AXIS @ %lu MHz: Avg %.2f µs (%.0f%% improvement target)\r\n",
             freq_mhz, (float)stats_a.avg_cycles / freq_mhz, 96.0f);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
    
    snprintf(msg, sizeof(msg), "[RESULTS] Z-AXIS @ %lu MHz: Avg %.2f µs (%.0f%% improvement target)\r\n",
             freq_mhz, (float)stats_z.avg_cycles / freq_mhz, 96.0f);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
    
    HAL_UART_Transmit(&huart1, (uint8_t*)"=== END OF MEASUREMENT ===\r\n", 29, 1000);
}

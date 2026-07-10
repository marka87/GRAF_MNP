/*
 * encoder_performance.h
 *
 * Performance measurement for encoder position functions
 * Measures latency reduction from baseline (__disable_irq) to optimized (Double-Check-Locking)
 */

#ifndef SRC_ENCODER_PERFORMANCE_H_
#define SRC_ENCODER_PERFORMANCE_H_

#include <main.h>
#include <stdint.h>

#define PERF_NUM_SAMPLES 10000

typedef struct {
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t avg_cycles;
    uint32_t median_cycles;
    uint32_t p95_cycles;
    uint32_t p99_cycles;
} encoder_perf_stats_t;

// Measurement functions
void encoder_perf_measure_init(void);
void encoder_perf_measure_a_axis(encoder_perf_stats_t *stats);
void encoder_perf_measure_z_axis(encoder_perf_stats_t *stats);
void encoder_perf_print_results(void);

#endif /* SRC_ENCODER_PERFORMANCE_H_ */

/*
 * data_buffer.c
 *
 *  Created on: Jan 22, 2025
 *      Author: Mark
 *
 *  Ring-Buffer: speichert die letzten MAX_DATA_POINTS Messpunkte.
 *  Wenn der Puffer voll ist, wird der älteste Eintrag überschrieben
 *  (kein teures O(n)-Verschieben).
 */
#include "data_buffer.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
extern UART_HandleTypeDef huart1;

#define MAX_DATA_POINTS 1000

typedef struct {
    uint32_t timestamp;  /* Zeitstempel in Millisekunden */
    int32_t  position;   /* Encoder-Istwert              */
    int32_t  target;     /* Encoder-Sollwert             */
} DataPoint;

static DataPoint data_buffer[MAX_DATA_POINTS];
static uint16_t  ring_head  = 0;  /* Nächste Schreibposition   */
static uint16_t  ring_count = 0;  /* Anzahl gültiger Einträge  */

void data_buffer_reset(void) {
    ring_head  = 0;
    ring_count = 0;
}

void log_data_point(int32_t position, int32_t target) {
    data_buffer[ring_head].timestamp = HAL_GetTick();
    data_buffer[ring_head].position  = position;
    data_buffer[ring_head].target    = target;
    ring_head = (uint16_t)((ring_head + 1u) % MAX_DATA_POINTS);
    if (ring_count < MAX_DATA_POINTS) ring_count++;
}

void save_data_to_uart(void) {
    /* Ältester Eintrag: bei vollem Puffer ist es ring_head, sonst Index 0 */
    uint16_t start = (ring_count < MAX_DATA_POINTS) ? 0u : ring_head;
    for (uint16_t i = 0; i < ring_count; i++) {
        uint16_t idx = (uint16_t)((start + i) % MAX_DATA_POINTS);
        char line[100];
        snprintf(line, sizeof(line), "Zeit: %lu ms, Position: %ld, Ziel: %ld\r\n",
                 data_buffer[idx].timestamp,
                 (long)data_buffer[idx].position,
                 (long)data_buffer[idx].target);
        HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)strlen(line), 1000);
    }
    data_buffer_reset();
}

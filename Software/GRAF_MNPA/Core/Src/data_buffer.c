/*
 * data_buffer.c
 *
 *  Created on: Jan 22, 2025
 *      Author: Mark
 */
#include "data_buffer.h"
#include "main.h"
extern UART_HandleTypeDef huart1;


#define MAX_DATA_POINTS 1000  // Maximale Anzahl an Datenpunkten

typedef struct {
    uint32_t timestamp;  // Zeitstempel des Werts (in Millisekunden)
    int32_t position;    // Encoder-Position
    int32_t target;      // Zielposition
} DataPoint;

DataPoint data_buffer[MAX_DATA_POINTS];  // Array für Datenpunkte
uint16_t data_index = 0;

void log_data_point(int32_t position, int32_t target) {
    if (data_index < MAX_DATA_POINTS) {
        data_buffer[data_index].timestamp = HAL_GetTick();  // Aktuellen Zeitstempel speichern
        data_buffer[data_index].position = position;        // Encoder-Position speichern
        data_buffer[data_index].target = target;            // Zielposition speichern
        data_index++;                                       // Index erhöhen
    } else {
        // Buffer ist voll, optional: Älteste Daten überschreiben
        for (uint16_t i = 1; i < MAX_DATA_POINTS; i++) {
            data_buffer[i - 1] = data_buffer[i];
        }
        data_buffer[MAX_DATA_POINTS - 1].timestamp = HAL_GetTick();
        data_buffer[MAX_DATA_POINTS - 1].position = position;
        data_buffer[MAX_DATA_POINTS - 1].target = target;
    }
}
void save_data_to_uart() {
    for (uint16_t i = 0; i < data_index; i++) {
        char line[100];
        snprintf(line, sizeof(line), "Zeit: %lu ms, Position: %ld, Ziel: %ld\r\n",
                 data_buffer[i].timestamp,
                 data_buffer[i].position,
                 data_buffer[i].target);
        HAL_UART_Transmit(&huart1, (uint8_t *)line, strlen(line), 1000);
    }
    data_index = 0;  // Buffer zurücksetzen
}

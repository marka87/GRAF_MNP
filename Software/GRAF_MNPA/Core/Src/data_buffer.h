/*
 * data_buffer.h
 *
 *  Created on: Jan 22, 2025
 *      Author: Mark
 */

#ifndef SRC_DATA_BUFFER_H_
#define SRC_DATA_BUFFER_H_
#include "main.h"

void data_buffer_reset(void);
void log_data_point(int32_t position, int32_t target);
void save_data_to_uart(void);

#endif /* SRC_DATA_BUFFER_H_ */

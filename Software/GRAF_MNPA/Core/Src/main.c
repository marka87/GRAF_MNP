/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Mark Angyal, E4, 2024. GRAF MNP
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "display.h"
#include "AD5684RARUZ.h"
#include "ADC_read.h"
#include "encoder.h"
#include "encoder_performance.h"
#include "Reference_Run.h"
#include "PID_Control.h"
#include "Z_PID_Control.h"
#include "handle_button.h"
#include "d_mot_control.h"
#include "data_buffer.h"
#include "test_run.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define a_mot 0x01		//Address for DAC-A...
#define z_mot 0x02		//DAC-B...
#define d_mot 0x04		//DAC-C.
#define TARGET_VOLTAGE_NEUTRAL 2.5f
#define MAX_DATA_POINTS 1000  // Maximale Anzahl an Datenpunkten

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi4;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
ad5684_dac_t dac;

typedef enum {
	IDLE_START, EXEC_REFERENCE_RUN, TEST_START, TEST_RUN, STOP, COMPLETED, FEHLER
} run_state_t;

run_state_t current_state = IDLE_START;

extern uint32_t A_Axis_TargetPosition;
extern uint32_t Z_Axis_TargetPosition;
extern uint32_t a_encoder_start;
extern uint32_t a_encoder_end;
extern uint32_t z_encoder_start;
extern uint32_t z_encoder_end;
extern uint32_t z_ax_no_pos;
extern char display_buffer[DISPLAY_MAX_LINES][30];
extern float voltage;
/* UART Callback */
static char error_message[64];
static volatile uint8_t UART1_rxBuffer[1] = { 0 };

/* Ring buffer for UART commands — filled by ISR, drained in main loop */
#define UART_CMD_BUF_SIZE 32
static volatile uint8_t uart_cmd_buf[UART_CMD_BUF_SIZE];
static volatile uint8_t uart_cmd_head = 0;
static volatile uint8_t uart_cmd_tail = 0;
static bool tick_100ms_testrun_elapsed = false;
static uint32_t z_target_requested = 0;

uint32_t next_100ms_tick = 0;
uint32_t next_10ms_tick = 0;
uint32_t next_1ms_tick = 0;
uint32_t next_uart_status_tick = 0;

volatile bool perform_encoder_perf_test = false;  // Flag to trigger performance measurement

volatile bool ds_was_activated = false;
bool z_axis_success = false;
bool a_axis_success = false;
static bool completed_stats_sent = false;
static bool test_summary_sent = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI4_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void update_display(void);
void Process_UART_Command(const char *command);
void red_light(void);
void green_light(void);
void yellow_light(void);
void white_light(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
display_info_t display1 = {								//The Display init
		.spi_handle = &hspi2, .lcd_width = 128, .lcd_height = 64,
				.lcd_ram_pages = 8 };

static const char* get_state_name(void) {
	switch (current_state) {
		case IDLE_START:         return "IDLE_START";
		case EXEC_REFERENCE_RUN: return "EXEC_REFERENCE_RUN";
		case TEST_START:         return "TEST_START";
		case TEST_RUN:           return "TEST_RUN";
		case STOP:               return "STOP";
		case COMPLETED:          return "COMPLETED";
		case FEHLER:             return "FEHLER";
		default:                 return "UNKNOWN";
	}
}

static void uart_send_status(void) {
	char buf[32];
	snprintf(buf, sizeof(buf), "_STATUS_%s\r\n", get_state_name());
	HAL_UART_Transmit(&huart1, (const uint8_t*) buf, (uint16_t) strlen(buf), 100);
}

void uart_send_text(const char *text, uint32_t timeout) {
	HAL_UART_Transmit(&huart1, (const uint8_t*) text, (uint16_t) strlen(text),
			timeout);
}

void Z_Target_SetRequested(uint32_t target) {
	z_target_requested = target;
	Z_Axis_TargetPosition = target;
}

void Z_Target_SetRequestedDirect(uint32_t target) {
	z_target_requested = target;
	Z_Axis_TargetPosition = target;
}

uint32_t Z_Target_GetRequested(void) {
	return z_target_requested;
}

static uint32_t clamp_nonnegative_position(int32_t position) {
	return (position < 0) ? 0u : (uint32_t)position;
}

static bool parse_positive_decimal(const char *s, float *out) {
	if (s == NULL || out == NULL) {
		return false;
	}

	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
		++s;
	}
	if (*s == '\0') {
		return false;
	}
	if (*s == '+') {
		++s;
	}
	if (*s == '-') {
		return false;
	}

	const char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
		--end;
	}

	uint32_t int_part = 0;
	uint32_t frac_part = 0;
	uint32_t frac_div = 1;
	bool seen_digit = false;
	bool seen_dot = false;
	for (const char *p = s; p < end; ++p) {
		char c = *p;
		if (c >= '0' && c <= '9') {
			seen_digit = true;
			if (!seen_dot) {
				int_part = (int_part * 10u) + (uint32_t) (c - '0');
			} else if (frac_div < 1000000000u) {
				frac_part = (frac_part * 10u) + (uint32_t) (c - '0');
				frac_div *= 10u;
			}
		} else if (c == '.' && !seen_dot) {
			seen_dot = true;
		} else {
			return false;
		}
	}
	if (!seen_digit) {
		return false;
	}
	*out = (float) int_part + ((float) frac_part / (float) frac_div);
	return *out > 0.0f;
}

static bool parse_pid_triplet(const char *payload, float *kp, float *ki, float *kd) {
	if (payload == NULL || kp == NULL || ki == NULL || kd == NULL) {
		return false;
	}
	const char *c1 = strchr(payload, ',');
	if (c1 == NULL) {
		return false;
	}
	const char *c2 = strchr(c1 + 1, ',');
	if (c2 == NULL) {
		return false;
	}
	if (strchr(c2 + 1, ',') != NULL) {
		return false;
	}
	char p1[24] = { 0 };
	char p2[24] = { 0 };
	char p3[24] = { 0 };
	size_t l1 = (size_t) (c1 - payload);
	size_t l2 = (size_t) (c2 - (c1 + 1));
	size_t l3 = strlen(c2 + 1);
	if (l1 == 0 || l2 == 0 || l3 == 0 || l1 >= sizeof(p1) || l2 >= sizeof(p2)
			|| l3 >= sizeof(p3)) {
		return false;
	}
	memcpy(p1, payload, l1);
	memcpy(p2, c1 + 1, l2);
	memcpy(p3, c2 + 1, l3);
	if (!parse_positive_decimal(p1, kp)) {
		return false;
	}
	if (!parse_positive_decimal(p2, ki)) {
		return false;
	}
	if (!parse_positive_decimal(p3, kd)) {
		return false;
	}
	return true;
}

/* Update Display */
/* Update Display (Minimal & Ruckfrei - waehrend TEST_RUN komplett deaktiviert) */
void update_display() {
	if (current_state == TEST_RUN) {
		return;
	}

	GPIO_PinState no_sen_state = HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin);
	int32_t a_axis_position = Encoder_GetPosition_A_AXIS();
	int32_t z_axis_position = Encoder_GetPosition_Z_AXIS();
	float z_dac_voltage = voltage;

	uint16_t raw_value = ADC_Drucksensor(&hadc1);
	float sensorVoltage = (float) raw_value * (5.0f / 4095.0f);

	const char *st_name = "BEREIT";
	if (current_state == IDLE_START) st_name = "BITTE REFERENZIEREN";
	else if (current_state == EXEC_REFERENCE_RUN) st_name = "REFERENZIERE...";
	else if (current_state == TEST_START) st_name = "BEREIT (START-POS)";
	else if (current_state == STOP) st_name = "NOT-HALT / STOP";
	else if (current_state == FEHLER) st_name = "FEHLER!";
	else if (current_state == COMPLETED) st_name = "TEST BEENDET";

	snprintf(display_buffer[0], sizeof(display_buffer[0]), "   GRAF MNP TEST");
	snprintf(display_buffer[2], sizeof(display_buffer[2]), "Status: %-15s", st_name);
	snprintf(display_buffer[4], sizeof(display_buffer[4]), "Z-Ist:  %ld", (long)z_axis_position);
	snprintf(display_buffer[7], sizeof(display_buffer[7]), "  REF     HOME   RESET");

	display_jazz_write_string_5x7(&display1, 0, display_buffer[0]);
	display_jazz_write_string_5x7(&display1, 2, display_buffer[2]);
	display_jazz_write_string_5x7(&display1, 4, display_buffer[4]);
	display_jazz_write_string_5x7(&display1, 7, display_buffer[7]);

	char datablock[256];
	sprintf(datablock, "%d;%.3f;%ld;%ld;%ld;%.3f\r\n", no_sen_state,
			sensorVoltage, (long)a_axis_position, (long)z_axis_position,
			(long)Z_Axis_TargetPosition, z_dac_voltage);
	uart_send_text(datablock, 50);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart1) {
		/* Store received byte in ring buffer — never call blocking code from ISR */
		uint8_t next = (uart_cmd_head + 1) % UART_CMD_BUF_SIZE;
		if (next != uart_cmd_tail) { // drop silently if buffer is full
			uart_cmd_buf[uart_cmd_head] = UART1_rxBuffer[0];
			uart_cmd_head = next;
		}
		HAL_UART_Receive_IT(&huart1, (uint8_t*) UART1_rxBuffer, 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart1) {
		__HAL_UART_CLEAR_OREFLAG(huart);
		__HAL_UART_CLEAR_NEFLAG(huart);
		__HAL_UART_CLEAR_FEFLAG(huart);
		__HAL_UART_CLEAR_PEFLAG(huart);
		HAL_UART_Receive_IT(&huart1, (uint8_t*) UART1_rxBuffer, 1);
	}
}

// UART-Verarbeitung
void Process_UART_Command(const char *command) {
	uint32_t raw_min = (z_encoder_start < z_encoder_end) ? z_encoder_start : z_encoder_end;
	uint32_t raw_max = (z_encoder_start > z_encoder_end) ? z_encoder_start : z_encoder_end;

	/* Dynamische Sicherheits-Grenzwerte fuer den Bediener:
	 * Unteres Limit: gemessener harter Anschlag + 50
	 * Oberes Limit:  gemessener harter Anschlag - 150 */
	uint32_t lower_limit = raw_min + 50u;
	uint32_t upper_limit = (raw_max > 150u) ? (raw_max - 150u) : raw_max;
	if (upper_limit < lower_limit) upper_limit = lower_limit + 100u;

	char response[128] = { 0 }; 				// Rückmeldungspuffer
	uint32_t step_size = 100;

	if (command[0] == '+' || command[0] == '-') {
		if (command[1] != '\0') {
			unsigned long parsed_step = 0;
			if (sscanf(command + 1, "%lu", &parsed_step) != 1 || parsed_step == 0) {
				snprintf(response, sizeof(response), "Ungueltig: +<zahl> oder -<zahl>\r\n");
				uart_send_status();
				uart_send_text(response, 50);
				return;
			}
			step_size = (uint32_t) parsed_step;
		}
	}

	if (command[0] == '+') { // Zielposition erhöhen
		if (current_state != TEST_START) {
			snprintf(response, sizeof(response), "Ignoriert: kein TEST_START\r\n");
		} else {
			uint32_t req = Z_Target_GetRequested();
			uint32_t next = req;
			if (req + step_size <= upper_limit) {
				next = req + step_size;
				snprintf(response, sizeof(response), "Erhöht: Ziel = %lu\r\n", next);
			} else {
				next = upper_limit;
				snprintf(response, sizeof(response), "Limit: Ziel = %lu\r\n", next);
			}
			Z_Target_SetRequested(next);
		}

	} else if (command[0] == '-') { // Zielposition verringern
		if (current_state != TEST_START) {
			snprintf(response, sizeof(response), "Ignoriert: kein TEST_START\r\n");
		} else {
			uint32_t req = Z_Target_GetRequested();
			uint32_t next = req;
			if (req >= step_size + lower_limit) {
				next = req - step_size;
				snprintf(response, sizeof(response), "Verringert: Ziel = %lu\r\n", next);
			} else {
				next = lower_limit;
				snprintf(response, sizeof(response), "Limit: Ziel = %lu\r\n", next);
			}
			Z_Target_SetRequested(next);
		}

	} else if (strcmp(command, "r") == 0) { // Relais an/aus
		snprintf(response, sizeof(response), "System reset initiated.\r\n");
		NVIC_SystemReset();
	} else if (strcmp(command, "s") == 0) { // Referenzlauf starten
		Z_PID_Reset();
		Z_PID_SetSchedulerEnabled(true);
		current_state = EXEC_REFERENCE_RUN;
		snprintf(response, sizeof(response), "Referenzlauf gestartet.\r\n");
	} else if (strcmp(command, "e") == 0) { // Test / Manuell starten
		HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);
		Z_PID_Reset();
		Z_PID_SetSchedulerEnabled(true);
		uint32_t home_pos = (z_ax_no_pos > 0) ? (z_ax_no_pos + 50) : clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS());
		Z_Target_SetRequestedDirect(home_pos);
		current_state = TEST_START;
		snprintf(response, sizeof(response), "Test/Manuell gestartet.\r\n");
	} else if (strncmp(command, "TA=", 3) == 0 || strcmp(command, "TA") == 0) {
		int cycles = (command[2] == '=') ? atoi(command + 3) : 10;
		if (cycles <= 0) cycles = 1;
		if (cycles > 100000) cycles = 100000;
		Z_PID_SetSchedulerEnabled(false);
		TestRun_InitEx(TESTRUN_MODE_A_CLASSIC, (uint32_t)cycles);
		current_state = TEST_RUN;
		snprintf(response, sizeof(response), "TEST_A_START:%d Zyklen\r\n", cycles);
	} else if (strncmp(command, "TB=", 3) == 0 || strcmp(command, "TB") == 0) {
		int cycles = (command[2] == '=') ? atoi(command + 3) : 10;
		if (cycles <= 0) cycles = 1;
		if (cycles > 100000) cycles = 100000;
		Z_PID_SetSchedulerEnabled(false);
		TestRun_InitEx(TESTRUN_MODE_B_PROBE_SCATTER, (uint32_t)cycles);
		current_state = TEST_RUN;
		snprintf(response, sizeof(response), "TEST_B_START:%d Zyklen\r\n", cycles);
	} else if (strncmp(command, "CFG_TB:", 7) == 0) {
		const char *p = command + 7;
		char buf[48];
		strncpy(buf, p, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		char *semi = strchr(buf, ';');
		float dmot = 3.3f;
		unsigned long delta = 90;
		if (semi != NULL) {
			*semi = '\0';
			char *comma = strchr(buf, ',');
			if (comma) *comma = '.';
			parse_positive_decimal(buf, &dmot);
			delta = strtoul(semi + 1, NULL, 10);
		} else {
			char *comma = strchr(buf, ',');
			if (comma) *comma = '.';
			parse_positive_decimal(buf, &dmot);
		}
		if (dmot > 20.0f) dmot = dmot / 1000.0f; /* falls in mV übergeben */
		TestRun_SetDruckmotorVoltage(dmot);
		if (delta > 0) TestRun_SetTriggerDeltaMv((uint32_t)delta);
		snprintf(response, sizeof(response), "CFG_TB_OK:dmot=%.2f,delta=%lu\r\n", dmot, (unsigned long)TestRun_GetTriggerDeltaMv());
	} else if (strcmp(command, "CFG_TB?") == 0) {
		snprintf(response, sizeof(response), "CFG_TB:%.2f;%lu\r\n", TestRun_GetDruckmotorVoltage(), (unsigned long)TestRun_GetTriggerDeltaMv());
	} else if (command[0] == 'Z' && command[1] != '\0') { // Z-Achse Zielposition direkt setzen (z.B. "Z1500")
		if (current_state != TEST_START) {
			snprintf(response, sizeof(response), "Ignoriert: kein TEST_START\r\n");
		} else {
			uint32_t val = 0;
			if (sscanf(command + 1, "%lu", &val) == 1) {
				if (val >= lower_limit && val <= upper_limit) {
					Z_Target_SetRequested(val);
					snprintf(response, sizeof(response), "Z-Pos: %lu\r\n", val);
				} else {
					snprintf(response, sizeof(response), "Limit! %lu [%lu-%lu]\r\n", val, lower_limit, upper_limit);
				}
			} else {
				snprintf(response, sizeof(response), "Ungueltig: Z<zahl>\r\n");
			}
		}
	} else if (strcmp(command, "PP?") == 0) {
		float kp = 0.0f, ki = 0.0f, kd = 0.0f;
		Z_PID_GetPositionParameters(&kp, &ki, &kd);
		snprintf(response, sizeof(response), "PIDZP:%.7f;%.9f;%.7f\r\n", kp, ki, kd);
	} else if (strcmp(command, "PV?") == 0) {
		float kp = 0.0f, ki = 0.0f, kd = 0.0f;
		Z_PID_GetVelocityParameters(&kp, &ki, &kd);
		snprintf(response, sizeof(response), "PIDZV:%.7f;%.9f;%.7f\r\n", kp, ki, kd);
	} else if (strcmp(command, "L?") == 0) {
		snprintf(response, sizeof(response), "ZLIM:%lu;%lu\r\n", lower_limit, upper_limit);
	} else if (strcmp(command, "V?") == 0) {
		snprintf(response, sizeof(response), "ZV:%u\r\n", Z_PID_GetSpeedLevel());
	} else if (command[0] == 'P' && command[1] == 'P' && command[2] == '=') {
		float kp = 0.0f, ki = 0.0f, kd = 0.0f;
		if (parse_pid_triplet(command + 3, &kp, &ki, &kd)) {
			Z_PID_SetPositionParameters(kp, ki, kd);
			snprintf(response, sizeof(response), "PIDZP_SET:%.7f;%.9f;%.7f\r\n", kp, ki, kd);
		} else {
			snprintf(response, sizeof(response), "PIDZP_ERR:Format PP=kp,ki,kd\r\n");
		}
	} else if (command[0] == 'P' && command[1] == 'V' && command[2] == '=') {
		float kp = 0.0f, ki = 0.0f, kd = 0.0f;
		if (parse_pid_triplet(command + 3, &kp, &ki, &kd)) {
			Z_PID_SetVelocityParameters(kp, ki, kd);
			snprintf(response, sizeof(response), "PIDZV_SET:%.7f;%.9f;%.7f\r\n", kp, ki, kd);
		} else {
			snprintf(response, sizeof(response), "PIDZV_ERR:Format PV=kp,ki,kd\r\n");
		}
	} else if (command[0] == 'V' && command[1] == '=') {
		int level = atoi(command + 2);
		if (level >= 1 && level <= 16) {
			Z_PID_SetSpeedLevel((uint8_t)level);
			snprintf(response, sizeof(response), "ZV_SET:%u\r\n", (unsigned)level);
		} else {
			snprintf(response, sizeof(response), "ZV_ERR:Format V=1..16\r\n");
		}
	} else if (strcmp(command, "N") == 0) {
		Z_PID_EmergencyNeutral(&dac);
		Z_Target_SetRequestedDirect(clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS()));
		snprintf(response, sizeof(response), "Z_NEUTRAL:2.5V\r\n");
	} else if (strcmp(command, "q") == 0) {
		Z_PID_EmergencyStop(&dac);
		current_state = STOP;
		snprintf(response, sizeof(response), "Testablauf Abgebrochen.\r\n");
	} else if (strcmp(command, "p") == 0) { // Performance measurement
		perform_encoder_perf_test = true;
		snprintf(response, sizeof(response), "Performance measurement initiated...\r\n");
	}
	// Rückmeldung senden
	uart_send_status();
	uart_send_text(response, 50);
}
void red_light() {
	HAL_GPIO_WritePin(GPIOD, EN_G_Pin | EN_B_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, EN_R_Pin, GPIO_PIN_SET);
	HAL_GPIO_TogglePin(GPIOJ, LED_RED_Pin);
}
void green_light() {
	HAL_GPIO_WritePin(GPIOD, EN_R_Pin | EN_B_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, EN_G_Pin, GPIO_PIN_SET);
	HAL_GPIO_TogglePin(GPIOJ, LED_GREEN_Pin);

}
void yellow_light() {
	HAL_GPIO_WritePin(GPIOD, EN_R_Pin | EN_G_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, EN_B_Pin, GPIO_PIN_RESET);
	HAL_GPIO_TogglePin(GPIOJ, LED_YELLOW_Pin);

}
void white_light() {
	HAL_GPIO_WritePin(GPIOD, EN_R_Pin | EN_G_Pin | EN_B_Pin, GPIO_PIN_SET);
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_SPI2_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_USART1_UART_Init();
  MX_SPI4_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

	/* GPIO Enables Voltages */
	HAL_GPIO_WritePin(EN_5V_GPIO_Port, EN_5V_Pin, GPIO_PIN_SET); //Enables 5V
	HAL_GPIO_WritePin(EN_12V_GPIO_Port, EN_12V_Pin, GPIO_PIN_SET); //Enables 12V
	/* DAC Initialization */
	HAL_GPIO_WritePin(GPIOB, DAC_RESET_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(GPIOB, DAC_RESET_Pin, GPIO_PIN_SET);
	dac = (ad5684_dac_t ) { .spi_handle = &hspi4, .spi_cs_port =
	SPI4_NSS_GPIO_Port, .spi_cs_pin = SPI4_NSS_Pin,
	};
	ad5684_init(&dac);
	display_jazz_init(&display1);
	HAL_GPIO_WritePin(GPIOD, EN_G_Pin | EN_B_Pin | EN_R_Pin, GPIO_PIN_SET); //Display background ON
	Encoder_Init();
	encoder_perf_measure_init(); // Initialize cycle counter for performance measurement
	HAL_UART_Receive_IT(&huart1, (uint8_t*) UART1_rxBuffer, 1);
	Z_Axis_TargetPosition = clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS());
	Z_Target_SetRequestedDirect(Z_Axis_TargetPosition);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		TestRunStats_t stats;
		if (current_state == IDLE_START) {
			white_light();
			d_mot_control(&dac, 2.5f);
			sprintf(display_buffer[5], " ");
			sprintf(display_buffer[6], "Bitte Referenzieren:");
			sprintf(display_buffer[7], "        START");
			if (HAL_GetTick() >= next_uart_status_tick) {
				next_uart_status_tick = HAL_GetTick() + 500;
				uart_send_status();
				uart_send_text("Bitte Referenzieren\r\n", 100);
			}

			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				current_state = EXEC_REFERENCE_RUN;
			}
			//Referenzlauf starten
		} else if (current_state == EXEC_REFERENCE_RUN) {
			Z_Axis_ReferenceRun(&dac, &z_axis_success);
			if (z_axis_success) {
				uart_send_status();
				uart_send_text("Z-Achse Referenz OK\r\n", 1000);
			} else {
				uart_send_status();
				uart_send_text("Z-Achse Referenz Fehler\r\n", 1000);
			    strcpy(error_message, "Z-Achse Referenzfehler");
				red_light();
				current_state = FEHLER;
				continue;
			}
			HAL_Delay(1000);
			A_Axis_ReferenceRun(&dac, &a_axis_success);
			if (a_axis_success) {
				uart_send_status();
				uart_send_text("A-Achse Referenz OK\r\n", 1000);
			} else {
				uart_send_status();
				uart_send_text("A-Achse Referenz Fehler\r\n", 1000);
				sprintf(display_buffer[6], "A-Achse Ref. Fehler");
			    strcpy(error_message, "A-Achse Referenzfehler");
				red_light();
				current_state = FEHLER;
				continue;
			}
			d_mot_control(&dac, 2.5f);
			HAL_Delay(500);
			HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);
			Z_PID_Reset();
			Z_Target_SetRequestedDirect(Z_Axis_TargetPosition);
			current_state = TEST_START;

		} else if (current_state == TEST_START) {
			white_light();
			d_mot_control(&dac, 2.5f);
			sprintf(display_buffer[5], " ");
			sprintf(display_buffer[6], "      TEST-MODUS");
			sprintf(display_buffer[7], "DEMO     KURZ    LANG");
			if (HAL_GetTick() >= next_uart_status_tick) {
				next_uart_status_tick = HAL_GetTick() + 500;
				uart_send_status();
				uart_send_text("Modus?\r\n", 100);
			}
			if (btn_up_pressed) {
					btn_up_pressed = 0;
					TestRun_Init(100);
					current_state = TEST_RUN; // DEMO-Modus
				} else if (btn_ok_pressed) {
					btn_ok_pressed = 0;
					TestRun_Init(1000);
					current_state = TEST_RUN; // KURZ-Modus
				} else if (btn_down_pressed) {
					btn_down_pressed = 0;
					TestRun_Init(10000);
					current_state = TEST_RUN; // LANG-Modus
				}
			//Testlauf durchführen
			} else if (current_state == TEST_RUN) {
				white_light();

				bool tick = tick_100ms_testrun_elapsed;
				if (tick) tick_100ms_testrun_elapsed = false;

				TestRunResult_t result = TestRun_Tick(tick);
				uint32_t cyc = TestRun_GetCurrentCycle();
				uint32_t tot = TestRun_GetTotalCycles();
				sprintf(display_buffer[5], "Zyklus %lu/%lu", cyc, tot);
				display_buffer[6][0] = '\0';
				sprintf(display_buffer[7], "TEST ...");

				if (TestRun_GetMode() != TESTRUN_MODE_B_PROBE_SCATTER) {
					if (HAL_GetTick() >= next_uart_status_tick) {
						next_uart_status_tick = HAL_GetTick() + 500;
						uart_send_status();
						char info[40];
						snprintf(info, sizeof(info), "TEST %lu/%lu\r\n", cyc, tot);
						uart_send_text(info, 50);
					}
				}

				if (result == TESTRUN_COMPLETE) {
					completed_stats_sent = false;
					test_summary_sent = false;
					current_state = COMPLETED;
				} else if (result == TESTRUN_ERROR) {
					TestRun_GetErrorMessage(error_message, sizeof(error_message));
					TestRun_GetStats(&stats);
					char summary[400];
					uint32_t time_min = stats.test_time_ms / 60000u;
					uint32_t time_sec = (stats.test_time_ms / 1000u) % 60u;
					const char *phase = TestRun_GetPhaseName();
					snprintf(summary, sizeof(summary),
						"TEST_SUMMARY:status=ERROR,cycles=%lu,done=%lu,ds_err=%lu,no_err=%lu,valid_sensor=%lu,invalid_sensor=%lu,motor_fault=%lu,z_ist_min=%ld,z_ist_max=%ld,z_soll_min=%ld,z_soll_max=%ld,last_ist=%ld,last_soll=%ld,phase=%s,time_m=%lu,time_s=%lu,last_delta=%ld,overshoot=%ld,lost=%ld,no_sensor_pos=%ld,last_error=%s\r\n\r\n",
						stats.total_cycles, stats.completed_cycles, stats.ds_errors, stats.no_sensor_errors,
						stats.valid_sensor_events, stats.invalid_sensor_events, stats.motor_faults,
						(long)stats.z_ist_min, (long)stats.z_ist_max,
						(long)stats.z_soll_min, (long)stats.z_soll_max,
						(long)stats.last_ist_pos, (long)stats.last_soll_pos,
						phase,
						time_min, time_sec,
						(long)stats.last_cycle_delta, (long)stats.last_cycle_overshoot,
						(long)stats.last_cycle_lost_steps, (long)stats.no_sensor_pos, error_message);
					uart_send_text(summary, 100);
					test_summary_sent = true;
					red_light();
					current_state = FEHLER;
				}
			} else if (current_state == STOP) {
			d_mot_control(&dac, 2.5f);
			yellow_light();
			sprintf(display_buffer[5], "Test abgebrochen");
			sprintf(display_buffer[6], "Bitte neustarten");
			sprintf(display_buffer[7], "        START");
			if (HAL_GetTick() >= next_uart_status_tick) {
				next_uart_status_tick = HAL_GetTick() + 1000;
				uart_send_status();
				uart_send_text("Test abgebrochen\r\n", 50);
			}
			Z_Target_SetRequestedDirect((z_encoder_start + z_encoder_end) / 2);
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);
				Z_PID_Reset();
				uint32_t home_pos = (z_ax_no_pos > 0) ? (z_ax_no_pos + 50) : clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS());
				Z_Target_SetRequestedDirect(home_pos);
				current_state = TEST_START;
			}
		} else if (current_state == COMPLETED) {
			green_light();
			d_mot_control(&dac, 2.5f);

			TestRun_GetStats(&stats);
			sprintf(display_buffer[5], "OK:%lu/%lu Zyklen", stats.completed_cycles, stats.total_cycles);
			sprintf(display_buffer[6], "Bitte OK druecken");
			sprintf(display_buffer[7], "        OK");

			/* Statistiken und Pufferdaten einmalig nach Testabschluss senden */
			if (!completed_stats_sent) {
				completed_stats_sent = true;
				if (TestRun_GetMode() == TESTRUN_MODE_B_PROBE_SCATTER) {
					TestBScatterStats_t b_stats;
					TestRun_GetScatterStats(&b_stats);
					char stats_msg[380];
					snprintf(stats_msg, sizeof(stats_msg),
						"TEST_B_SUMMARY:status=OK,cycles=%lu,done=%lu,z_ref=%ld,z_min=%ld,z_max=%ld,delta_min=%ld,delta_max=%ld,range=%ld,mean=%.1f,baseline_v=%.3f,trig_v=%.3f\r\n\r\n",
						stats.total_cycles, stats.completed_cycles,
						(long)b_stats.z_ref_pos,
						(long)b_stats.z_min_pos, (long)b_stats.z_max_pos,
						(long)(b_stats.z_min_pos - b_stats.z_ref_pos),
						(long)(b_stats.z_max_pos - b_stats.z_ref_pos),
						(long)b_stats.scatter_range,
						(double)b_stats.mean_pos,
						(double)((float)b_stats.baseline_adc * (5.0f / 4095.0f)),
						(double)((float)b_stats.trigger_adc * (5.0f / 4095.0f)));
					uart_send_text(stats_msg, 100);
				} else {
					char stats_msg[320];
					uint32_t time_min = stats.test_time_ms / 60000u;
					uint32_t time_sec = (stats.test_time_ms / 1000u) % 60u;
					const char *phase = TestRun_GetPhaseName();
					snprintf(stats_msg, sizeof(stats_msg),
						"TEST_SUMMARY:status=OK,cycles=%lu,done=%lu,ds_err=%lu,no_err=%lu,valid_sensor=%lu,invalid_sensor=%lu,motor_fault=%lu,z_ist_min=%ld,z_ist_max=%ld,z_soll_min=%ld,z_soll_max=%ld,last_ist=%ld,last_soll=%ld,phase=%s,time_m=%lu,time_s=%lu,last_delta=%ld,overshoot=%ld,lost=%ld,no_sensor_pos=%ld\r\n\r\n",
						stats.total_cycles, stats.completed_cycles, stats.ds_errors, stats.no_sensor_errors,
						stats.valid_sensor_events, stats.invalid_sensor_events, stats.motor_faults,
						(long)stats.z_ist_min, (long)stats.z_ist_max,
						(long)stats.z_soll_min, (long)stats.z_soll_max,
						(long)stats.last_ist_pos, (long)stats.last_soll_pos,
						phase,
						time_min, time_sec,
						(long)stats.last_cycle_delta, (long)stats.last_cycle_overshoot,
						(long)stats.last_cycle_lost_steps, (long)stats.no_sensor_pos);
					uart_send_text(stats_msg, 100);
				}
			}

			if (HAL_GetTick() >= next_uart_status_tick) {
				next_uart_status_tick = HAL_GetTick() + 1000;
				uart_send_status();
				uart_send_text("Test abgeschlossen\r\n", 50);
			}
			Z_Target_SetRequestedDirect(z_ax_no_pos + 50);
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				current_state = TEST_START;
			}
		} else if (current_state == FEHLER) {
			red_light();
			d_mot_control(&dac, 2.5f);
			snprintf(display_buffer[5], sizeof(display_buffer[5]), "%.29s", error_message);
			sprintf(display_buffer[6], "Bitte neustarten");
			sprintf(display_buffer[7], "        START");
			if (HAL_GetTick() >= next_uart_status_tick) {
				next_uart_status_tick = HAL_GetTick() + 1000;
				uart_send_status();
				uart_send_text("FEHLER\r\n", 50);
			}
			if (!test_summary_sent) {
				TestRun_GetStats(&stats);
				char summary[400];
				uint32_t time_min = stats.test_time_ms / 60000u;
				uint32_t time_sec = (stats.test_time_ms / 1000u) % 60u;
				const char *phase = TestRun_GetPhaseName();
				snprintf(summary, sizeof(summary),
					"TEST_SUMMARY:status=ERROR,cycles=%lu,done=%lu,ds_err=%lu,no_err=%lu,valid_sensor=%lu,invalid_sensor=%lu,motor_fault=%lu,z_ist_min=%ld,z_ist_max=%ld,z_soll_min=%ld,z_soll_max=%ld,last_ist=%ld,last_soll=%ld,phase=%s,time_m=%lu,time_s=%lu,last_delta=%ld,overshoot=%ld,lost=%ld,no_sensor_pos=%ld,last_error=%s\r\n\r\n",
					stats.total_cycles, stats.completed_cycles, stats.ds_errors, stats.no_sensor_errors,
					stats.valid_sensor_events, stats.invalid_sensor_events, stats.motor_faults,
					(long)stats.z_ist_min, (long)stats.z_ist_max,
					(long)stats.z_soll_min, (long)stats.z_soll_max,
					(long)stats.last_ist_pos, (long)stats.last_soll_pos,
					phase,
					time_min, time_sec,
					(long)stats.last_cycle_delta, (long)stats.last_cycle_overshoot,
					(long)stats.last_cycle_lost_steps, (long)stats.no_sensor_pos, error_message);
				uart_send_text(summary, 100);
				test_summary_sent = true;
			}
			Z_Target_SetRequestedDirect(clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS()));
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);
				Z_PID_Reset();
				uint32_t home_pos = (z_ax_no_pos > 0) ? (z_ax_no_pos + 50) : clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS());
				Z_Target_SetRequestedDirect(home_pos);
				current_state = TEST_START;
			}
		}
		// Accumulate UART bytes into lines, process on '\n'
		// Single-char and multi-char commands (e.g. "Z1500\n") both work
		static char uart_line_buf[64];
		static uint8_t uart_line_len = 0;
		if (uart_cmd_tail != uart_cmd_head) {
			char c = (char)uart_cmd_buf[uart_cmd_tail];
			uart_cmd_tail = (uart_cmd_tail + 1) % UART_CMD_BUF_SIZE;
			if (c == '\n' || c == '\r') {
				if (uart_line_len > 0) {
					uart_line_buf[uart_line_len] = '\0';
					Process_UART_Command(uart_line_buf);
					uart_line_len = 0;
				}
			} else if (uart_line_len < (uint8_t)(sizeof(uart_line_buf) - 1)) {
				uart_line_buf[uart_line_len++] = c;
			} else {
				uart_line_len = 0; // overflow: discard
			}
		}
		// Trigger performance measurement if requested via 'p' command
		if (perform_encoder_perf_test) {
			perform_encoder_perf_test = false;
			encoder_perf_print_results();
		}
		if (HAL_GetTick() >= next_100ms_tick) {
			next_100ms_tick = HAL_GetTick() + 100;
			tick_100ms_testrun_elapsed = true;
			if (current_state != TEST_RUN) {
				update_display();
			}
		}
		if (HAL_GetTick() >= next_10ms_tick) {
			next_10ms_tick = HAL_GetTick() + 10;

			handle_button_ok();
			handle_button_up();
			handle_button_down();

			/* Globale Taster-Funktionen (REF / HOME / RESET) */
			if (btn_up_pressed) {
				btn_up_pressed = 0;
				/* Taste 1 (Links): REFERENZLAUF */
				if (current_state != TEST_RUN) {
					Z_PID_Reset();
					Z_PID_SetSchedulerEnabled(true);
					current_state = EXEC_REFERENCE_RUN;
					uart_send_text("Referenzlauf via Taste gestartet.\r\n", 50);
				}
			}
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				/* Taste 2 (Mitte): HOME / START-FREIGABE / FEHLER QUITTIEREN */
				if (current_state != TEST_RUN) {
					HAL_GPIO_WritePin(GPIOB, Z_AX_REL_EN_Pin, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOB, A_AX_REL_EN_Pin, GPIO_PIN_SET);
					Z_PID_Reset();
					Z_PID_SetSchedulerEnabled(true);
					uint32_t home_pos = (z_ax_no_pos > 0) ? (z_ax_no_pos + 50) : clamp_nonnegative_position(Encoder_GetPosition_Z_AXIS());
					Z_Target_SetRequestedDirect(home_pos);
					current_state = TEST_START;
					uart_send_text("Home / Freigabe via Taste OK.\r\n", 50);
				}
			}
			if (btn_down_pressed) {
				btn_down_pressed = 0;
				/* Taste 3 (Rechts): NOT-HALT (im Test) oder HARDWARE RESET (im Stillstand) */
				if (current_state == TEST_RUN) {
					Z_PID_EmergencyStop(&dac);
					current_state = STOP;
					uart_send_text("NOT-HALT via Taste DOWN!\r\n", 50);
				} else {
					uart_send_text("System Reset via Taste DOWN...\r\n", 50);
					HAL_Delay(50);
					NVIC_SystemReset();
				}
			}
		}
		if (HAL_GetTick() >= next_1ms_tick) {
			next_1ms_tick = HAL_GetTick() + 1;
			if (current_state == TEST_START || current_state == TEST_RUN || current_state == COMPLETED) {
				A_Axis_PIDControl(&dac, A_Axis_TargetPosition);
				if (!Z_Axis_PIDControl(&dac, Z_Axis_TargetPosition)) {
					char reason_buf[64];
					snprintf(reason_buf, sizeof(reason_buf), "%s", Z_PID_GetTripReason());
					Z_PID_EmergencyStop(&dac);
					snprintf(error_message, sizeof(error_message), "%s", reason_buf);
					red_light();
					current_state = FEHLER;
					uart_send_status();
					char uart_err[96];
					snprintf(uart_err, sizeof(uart_err), "NOT-STOPP: %s\r\n", reason_buf);
					uart_send_text(uart_err, 50);
				}
			} else if (current_state == IDLE_START || current_state == STOP || current_state == FEHLER) {
				ad5684_set_voltage(&dac, 2.5f, a_mot);
				ad5684_set_voltage(&dac, 2.5f, z_mot);
			}
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	}

	return 0;
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI4_Init(void)
{

  /* USER CODE BEGIN SPI4_Init 0 */

  /* USER CODE END SPI4_Init 0 */

  /* USER CODE BEGIN SPI4_Init 1 */

  /* USER CODE END SPI4_Init 1 */
  /* SPI4 parameter configuration*/
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_MASTER;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES;
  hspi4.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi4.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi4.Init.NSS = SPI_NSS_SOFT;
  hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi4.Init.CRCPolynomial = 7;
  hspi4.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi4.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI4_Init 2 */

  /* USER CODE END SPI4_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 215;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED3;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 1;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 1;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED3;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 15;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 15;
  if (HAL_TIM_Encoder_Init(&htim5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */
  __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
      HAL_NVIC_SetPriority(TIM5_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(TIM5_IRQn);
  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI4_NSS_GPIO_Port, SPI4_NSS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DISP_A0_Pin|DRUCK_REL_EN_Pin|DAC_RESET_Pin|A_AX_REL_EN_Pin
                          |Z_AX_REL_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RST_DISPLAY_GPIO_Port, RST_DISPLAY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EN_12V_GPIO_Port, EN_12V_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EN_5V_GPIO_Port, EN_5V_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, EN_G_Pin|EN_B_Pin|EN_R_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOJ, LED_GREEN_Pin|LED_RED_Pin|LED_YELLOW_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SPI4_NSS_Pin */
  GPIO_InitStruct.Pin = SPI4_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI4_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DISP_A0_Pin DRUCK_REL_EN_Pin DAC_RESET_Pin A_AX_REL_EN_Pin
                           Z_AX_REL_EN_Pin */
  GPIO_InitStruct.Pin = DISP_A0_Pin|DRUCK_REL_EN_Pin|DAC_RESET_Pin|A_AX_REL_EN_Pin
                          |Z_AX_REL_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : RST_DISPLAY_Pin */
  GPIO_InitStruct.Pin = RST_DISPLAY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RST_DISPLAY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EN_12V_Pin */
  GPIO_InitStruct.Pin = EN_12V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EN_12V_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EN_5V_Pin */
  GPIO_InitStruct.Pin = EN_5V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EN_5V_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : EN_G_Pin EN_B_Pin EN_R_Pin */
  GPIO_InitStruct.Pin = EN_G_Pin|EN_B_Pin|EN_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : NO_SEN_Pin */
  GPIO_InitStruct.Pin = NO_SEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(NO_SEN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin LED_YELLOW_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED_Pin|LED_YELLOW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_DOWN_Pin BTN_OK_Pin BTN_UP_Pin */
  GPIO_InitStruct.Pin = BTN_DOWN_Pin|BTN_OK_Pin|BTN_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
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
#include "display.h"
#include "AD5684RARUZ.h"
#include "ADC_read.h"
#include "encoder.h"
#include "Reference_Run.h"
#include "PID_Control.h"
#include "Z_PID_Control.h"
#include "handle_button.h"
#include "d_mot_control.h"
#include "data_buffer.h"

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

typedef enum {
	GO_DOWN, GO_UP
} test_run_mode_t;

test_run_mode_t test_run_mode = GO_UP;

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
static char error_message[30];
static char uart_rx_buffer[32] = { 0 }; // Empfangspuffer
static uint8_t uart_rx_index = 0;
static volatile uint8_t UART1_rxBuffer[1] = { 0 };
static volatile bool byte_handled = true;
static volatile uint8_t byte_received = 0;
static bool tick_100ms_testrun_elapsed = false;

uint32_t next_100ms_tick = 0;
uint32_t next_10ms_tick = 0;
uint32_t next_1ms_tick = 0;

uint32_t num_total_cycles = 0;
uint32_t current_cycle = 0;

int32_t target_position_final = 0;
int32_t target_position_running = 0;

volatile bool ds_was_activated = false;
volatile bool z_axis_success = false;
volatile bool a_axis_success = false;
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

/* Update Display */
void update_display() {
	GPIO_PinState no_sen_state = HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin);
	int32_t a_axis_position = Encoder_GetPosition_A_AXIS();
	int32_t z_axis_position = Encoder_GetPosition_Z_AXIS();
	if (no_sen_state == GPIO_PIN_SET) { // Zeile 0: Nadel oben (High/Low)
		sprintf(display_buffer[0], "Nadel oben:  HIGH");
	} else {
		sprintf(display_buffer[0], "Nadel oben:  LOW ");
	}
	uint16_t raw_value = ADC_Drucksensor(&hadc1); // Zeile 1: Drucksensor (ADC-Wert)
	float sensorVoltage = (float) raw_value * (5.0f / 4095.0f);
	sprintf(display_buffer[1], "Drucksensor: %.3f V", sensorVoltage);
	sprintf(display_buffer[2], "A-AX:%5lu", a_axis_position); // Zeile 2 & 3: Achspositionen
	sprintf(display_buffer[3], "Z-Ist:%5lu", z_axis_position);
	sprintf(display_buffer[4], "Z-Soll:%5lu", Z_Axis_TargetPosition); // Zeile 4: Sollwert der Z-Achse
	for (int i = 0; i < DISPLAY_MAX_LINES; i++) {
		display_jazz_write_string_5x7(&display1, i, display_buffer[i]);
	}
	char datablock[256];
	sprintf(datablock, "%d;%.3f;%5lu;%5lu;%5lu\r\n", no_sen_state,
			sensorVoltage, a_axis_position, z_axis_position,
			Z_Axis_TargetPosition);
	HAL_UART_Transmit(&huart1, datablock, strlen(datablock), 2000);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart1) {
		char received_char = UART1_rxBuffer[0]; // Empfangenes Zeichen
		HAL_UART_Receive_IT(&huart1, UART1_rxBuffer, 1); // Empfang erneut aktivieren

		char command[2] = { received_char, '\0' }; // Zeichen in String umwandeln
		Process_UART_Command(command);
	}
}

// UART-Verarbeitung
void Process_UART_Command(const char *command) {
	static const uint32_t step_size = 100; 	// Schrittgröße
	uint32_t lower_limit = z_encoder_start; // Untere Grenze
	uint32_t upper_limit = z_encoder_end;   // Obere Grenze
	char response[64] = { 0 }; 				// Rückmeldungspuffer

	if (command[0] == '+' && command[1] == '\0') { // Zielposition erhöhen
		if (Z_Axis_TargetPosition + step_size <= upper_limit) {
			Z_Axis_TargetPosition += step_size;
		} else {
			Z_Axis_TargetPosition = upper_limit;
		}
		snprintf(response, sizeof(response), "Erhöht: Ziel = %lu\r\n",
				Z_Axis_TargetPosition);

	} else if (command[0] == '-' && command[1] == '\0') { // Zielposition verringern
		if (Z_Axis_TargetPosition >= step_size + lower_limit) {
			Z_Axis_TargetPosition -= step_size;
		} else {
			Z_Axis_TargetPosition = lower_limit;
		}
		snprintf(response, sizeof(response), "Verringert: Ziel = %lu\r\n",
				Z_Axis_TargetPosition);

	} else if (strcmp(command, "r") == 0) { // Relais an/aus
		snprintf(response, sizeof(response), "System reset initiated.\r\n");
		NVIC_SystemReset();
	} else if (strcmp(command, "s") == 0) { // Referenzlauf starten
		current_state = EXEC_REFERENCE_RUN;
		snprintf(response, sizeof(response), "Referenzlauf gestartet.\r\n");
	} else if (strcmp(command, "e") == 0) { // Referenzlauf starten
		current_state = TEST_START;
		snprintf(response, sizeof(response), "Test gestartet.\r\n");
	} else if (strcmp(command, "1") == 0) { // Demo-Modus starten
		num_total_cycles = 100;
		current_cycle = 0;
		current_state = TEST_RUN;
		snprintf(response, sizeof(response),
				"Demo-Modus gestartet (10 Zyklen).\r\n");

	} else if (strcmp(command, "2") == 0) { // Kurz-Modus mit 100 Zyklen
		num_total_cycles = 1000;
		current_cycle = 0;
		current_state = TEST_RUN;
		snprintf(response, sizeof(response),
				"Kurz-Modus gestartet (100 Zyklen).\r\n");
	} else if (strcmp(command, "3") == 0) { // Lang-Modus mit 1000 Zyklen
		num_total_cycles = 10000;
		current_cycle = 0;
		current_state = TEST_RUN;
		snprintf(response, sizeof(response),
				"Lang-Modus gestartet (1000 Zyklen).\r\n");
	} else if (strcmp(command, "q") == 0) {
		current_state = STOP;
		snprintf(response, sizeof(response), "Testablauf Abbgebrochen. \r\n");
	}
	// Rückmeldung senden
	HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
	HAL_UART_Transmit(&huart1, (uint8_t*) response, strlen(response),
	HAL_MAX_DELAY);
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
	ad5684_dac_t dac = { .spi_handle = &hspi4, .spi_cs_port =
	SPI4_NSS_GPIO_Port, .spi_cs_pin = SPI4_NSS_Pin,
	};
	ad5684_init(&dac);
	display_jazz_init(&display1);
	HAL_GPIO_WritePin(GPIOD, EN_G_Pin | EN_B_Pin | EN_R_Pin, GPIO_PIN_SET); //Display background ON
	Encoder_Init();
//	HAL_UART_Receive_IT(&huart1, UART1_rxBuffer, 1);
	HAL_Delay(1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		if (current_state == IDLE_START) {
			white_light();
			d_mot_control(&dac, 2.5f);
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "Bitte Referenzieren\r\n", 21, 1000);
			sprintf(display_buffer[5], " ");
			sprintf(display_buffer[6], "Bitte Referenzieren:");
			sprintf(display_buffer[7], "        START");

			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				current_state = EXEC_REFERENCE_RUN;
			} else if (!byte_handled && byte_received == 's') {
				byte_handled = true;
				current_state = EXEC_REFERENCE_RUN;
			}
			//Referenzlauf starten
		} else if (current_state == EXEC_REFERENCE_RUN) {
			Z_Axis_ReferenceRun(&dac, &z_axis_success);
			if (z_axis_success) {
				HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
				HAL_UART_Transmit(&huart1, (uint8_t*) "Z-Achse Referenz OK\r\n", 21, 1000);
			} else {
				HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
				HAL_UART_Transmit(&huart1, (uint8_t*) "Z-Achse Referenz Fehler\r\n", 25, 1000);
			    strcpy(error_message, "Z-Achse Referenzfehler");
				red_light();
				current_state = FEHLER;
				continue;
			}
			HAL_Delay(1000);
			A_Axis_ReferenceRun(&dac, &a_axis_success);
			if (a_axis_success) {
				HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
				HAL_UART_Transmit(&huart1, (uint8_t*) "A-Achse Referenz OK\r\n", 28, 1000);
			} else {
				HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
				HAL_UART_Transmit(&huart1, (uint8_t*) "A-Achse Referenz Fehler\r\n", 28, 1000);
				sprintf(display_buffer[6], "A-Achse Ref. Fehler");
			    strcpy(error_message, "A-Achse Referenzfehler");
				red_light();
				current_state = FEHLER;
				continue;
			}
			d_mot_control(&dac, 2.5f);
			HAL_Delay(1000);
			current_state = TEST_START;

		} else if (current_state == TEST_START) {
			white_light();
			d_mot_control(&dac, 2.5f);
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "Modus?\r\n", 8, 1000);
			sprintf(display_buffer[5], " ");
			sprintf(display_buffer[6], "      TEST-MODUS");
			sprintf(display_buffer[7], "DEMO     KURZ    LANG");
			if (btn_up_pressed) {
				btn_up_pressed = 0;
				num_total_cycles = 100;
				current_cycle = 0;
				target_position_final = z_ax_no_pos + 200;
				target_position_running = Encoder_GetPosition_Z_AXIS();
				current_state = TEST_RUN; // DEMO-Modus
			} else if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				num_total_cycles = 1000;
				current_cycle = 0;
				target_position_final = z_ax_no_pos + 200;
				target_position_running = Encoder_GetPosition_Z_AXIS();
				current_state = TEST_RUN; // KURZ-Modus
			} else if (btn_down_pressed) {
				btn_down_pressed = 0;
				num_total_cycles = 10000;
				current_cycle = 0;
				target_position_final = z_ax_no_pos + 200;
				target_position_running = Encoder_GetPosition_Z_AXIS();
				current_state = TEST_RUN; // LANG-Modus
			}
			//Testlauf durchführen
		} else if (current_state == TEST_RUN) {
			white_light();
			 // Druck gegen Drucksensor
			d_mot_control(&dac, 3.5f); //3.83f = 20V bzw. 17V +3VO ffset
			int16_t ds_value = ADC_Drucksensor(&hadc1);
			if (ds_value > 410) {
				HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
				HAL_UART_Transmit(&huart1,(uint8_t*) "Fehler: Drucksensor\r\n", 21, 1000);
			    strcpy(error_message, "Fehler: Drucksensor");
				current_state = FEHLER;
				continue;
			}
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "TEST ...\r\n", 10, 1000);
			sprintf(display_buffer[7], "TEST ...");
			if (current_cycle >= num_total_cycles) {
				current_state = COMPLETED;
			}
			if (tick_100ms_testrun_elapsed) {
				tick_100ms_testrun_elapsed = false;
			    int32_t current_position = Encoder_GetPosition_Z_AXIS();
			    log_data_point(current_position, Z_Axis_TargetPosition);  // Daten speichern
				if (test_run_mode == GO_UP) {

					Z_Axis_TargetPosition = z_ax_no_pos + 200;

					if (Encoder_GetPosition_Z_AXIS() > z_ax_no_pos + 100) {
						if (HAL_GPIO_ReadPin(NO_SEN_GPIO_Port, NO_SEN_Pin) == GPIO_PIN_RESET) { // Sensor activated?
							test_run_mode = GO_DOWN;
						} else {
						    strcpy(error_message, "Fehler: NO-Sensor");
							current_state = FEHLER;
							continue;
						}
					}
				} else if (test_run_mode == GO_DOWN) {

					Z_Axis_TargetPosition = z_encoder_start + 400;

					if (Encoder_GetPosition_Z_AXIS()<= (z_encoder_start + 500)) {

						current_cycle++;

						test_run_mode = GO_UP;

						if (current_cycle > num_total_cycles) {
							current_state = COMPLETED;
						}
					}
				}
			}
		} else if (current_state == STOP) {
			//USER information
			d_mot_control(&dac, 2.5f);
			yellow_light();
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "Test abgebrochen\r\n", 18,1000);
			sprintf(display_buffer[5], "Test abgebrochen");
			sprintf(display_buffer[6], "Bitte neustarten");
			sprintf(display_buffer[7], "        START");
			//Z-Achse mittig setzen, warten auf OK
			Z_Axis_TargetPosition = (z_encoder_start + z_encoder_end) / 2;
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				current_state = TEST_START;
			}
		} else if (current_state == COMPLETED) {
			green_light();
			save_data_to_uart();
			d_mot_control(&dac, 2.5f);
			// Test abgeschlossen
			sprintf(display_buffer[5], "Test abgeschlossen");
			sprintf(display_buffer[6], "Bitte OK druecken");
			sprintf(display_buffer[7], "        OK");
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "Test abgeschlossen\r\n", 20,1000);

			Z_Axis_TargetPosition = z_ax_no_pos + 50; //z_ax_no_pos + 50 // (z_encoder_start + z_encoder_end) / 2

			if (btn_ok_pressed) {
			btn_ok_pressed = 0;
			current_state = TEST_START;
			}
		} else if (current_state == FEHLER){
			red_light();
			d_mot_control(&dac, 2.5f);
			HAL_UART_Transmit(&huart1, "_STATUS_", 8, 2000);
			HAL_UART_Transmit(&huart1, (uint8_t*) "FEHLER\r\n", 8,1000);
		    sprintf(display_buffer[5], "%s", error_message);
			sprintf(display_buffer[6], "Bitte neustarten");
			sprintf(display_buffer[7], "        START");
			//Z-Achse mittig setzen, warten auf OK
			Z_Axis_TargetPosition = Encoder_GetPosition_Z_AXIS();
			if (btn_ok_pressed) {
				btn_ok_pressed = 0;
				current_state = TEST_START;
			}
		}
		// UART, Ticks und Display aktualisieren
		if (!byte_handled) {
			Process_UART_Command(uart_rx_buffer);
			byte_handled = true;
		}
		if (HAL_GetTick() >= next_100ms_tick) {
			next_100ms_tick = HAL_GetTick() + 100;
			tick_100ms_testrun_elapsed = true;
			update_display();
		}
		if (HAL_GetTick() >= next_10ms_tick) {
			next_10ms_tick = HAL_GetTick() + 10;
//			tick_100ms_testrun_elapsed = true;

			handle_button_ok();
			handle_button_up();
			handle_button_down();
		}
		if (HAL_GetTick() >= next_1ms_tick) {
			next_1ms_tick = HAL_GetTick() + 1;
			A_Axis_PIDControl(&dac, A_Axis_TargetPosition);
			Z_Axis_PIDControl(&dac, Z_Axis_TargetPosition);
//			Move_Z_Axis_With_Voltage(&dac, Z_Axis_TargetPosition);
			ADC_Drucksensor(&hadc1);
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

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SPI4_NSS_Pin GPIO_PIN_4
#define SPI4_NSS_GPIO_Port GPIOE
#define SPI4_DAC_SCK_Pin GPIO_PIN_2
#define SPI4_DAC_SCK_GPIO_Port GPIOE
#define DISP_A0_Pin GPIO_PIN_8
#define DISP_A0_GPIO_Port GPIOB
#define TIM3CH1_Pin GPIO_PIN_5
#define TIM3CH1_GPIO_Port GPIOB
#define SPI4_DAC_MISO_Pin GPIO_PIN_5
#define SPI4_DAC_MISO_GPIO_Port GPIOE
#define SPI4_DAC_MOSI_Pin GPIO_PIN_6
#define SPI4_DAC_MOSI_GPIO_Port GPIOE
#define TIM4CH2_Pin GPIO_PIN_7
#define TIM4CH2_GPIO_Port GPIOB
#define TIM4CH1_Pin GPIO_PIN_6
#define TIM4CH1_GPIO_Port GPIOB
#define RST_DISPLAY_Pin GPIO_PIN_15
#define RST_DISPLAY_GPIO_Port GPIOG
#define SPI2_DISP_MOSI_Pin GPIO_PIN_3
#define SPI2_DISP_MOSI_GPIO_Port GPIOI
#define SPI2_DISP_SCK_Pin GPIO_PIN_1
#define SPI2_DISP_SCK_GPIO_Port GPIOI
#define SPI2_DISP_NSS_Pin GPIO_PIN_0
#define SPI2_DISP_NSS_GPIO_Port GPIOI
#define EN_12V_Pin GPIO_PIN_7
#define EN_12V_GPIO_Port GPIOC
#define TIM3CH2_Pin GPIO_PIN_6
#define TIM3CH2_GPIO_Port GPIOC
#define EN_5V_Pin GPIO_PIN_7
#define EN_5V_GPIO_Port GPIOF
#define EN_G_Pin GPIO_PIN_10
#define EN_G_GPIO_Port GPIOD
#define EN_B_Pin GPIO_PIN_9
#define EN_B_GPIO_Port GPIOD
#define EN_R_Pin GPIO_PIN_8
#define EN_R_GPIO_Port GPIOD
#define A_AXIS_CH2_Pin GPIO_PIN_1
#define A_AXIS_CH2_GPIO_Port GPIOA
#define A_AXIS_CH1_Pin GPIO_PIN_0
#define A_AXIS_CH1_GPIO_Port GPIOA
#define Z_AXIS_CH2_Pin GPIO_PIN_11
#define Z_AXIS_CH2_GPIO_Port GPIOH
#define ADC_NO_SEN_Pin GPIO_PIN_2
#define ADC_NO_SEN_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_2
#define LED_GREEN_GPIO_Port GPIOJ
#define Z_AXIS_CH1_Pin GPIO_PIN_10
#define Z_AXIS_CH1_GPIO_Port GPIOH
#define ADC_DRUCK_SEN_Pin GPIO_PIN_3
#define ADC_DRUCK_SEN_GPIO_Port GPIOA
#define ADC1_IN7_Pin GPIO_PIN_7
#define ADC1_IN7_GPIO_Port GPIOA
#define DRUCK_REL_EN_Pin GPIO_PIN_1
#define DRUCK_REL_EN_GPIO_Port GPIOB
#define DAC_RESET_Pin GPIO_PIN_0
#define DAC_RESET_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_0
#define LED_RED_GPIO_Port GPIOJ
#define LED_YELLOW_Pin GPIO_PIN_1
#define LED_YELLOW_GPIO_Port GPIOJ
#define BTN_DOWN_Pin GPIO_PIN_10
#define BTN_DOWN_GPIO_Port GPIOE
#define BTN_OK_Pin GPIO_PIN_12
#define BTN_OK_GPIO_Port GPIOE
#define BTN_UP_Pin GPIO_PIN_15
#define BTN_UP_GPIO_Port GPIOE
#define A_AX_REL_EN_Pin GPIO_PIN_11
#define A_AX_REL_EN_GPIO_Port GPIOB
#define Z_AX_REL_EN_Pin GPIO_PIN_14
#define Z_AX_REL_EN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

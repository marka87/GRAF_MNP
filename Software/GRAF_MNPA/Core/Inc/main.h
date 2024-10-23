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
#define BTN_UP_Pin GPIO_PIN_7
#define BTN_UP_GPIO_Port GPIOK
#define BTN_OK_Pin GPIO_PIN_6
#define BTN_OK_GPIO_Port GPIOK
#define BTN_DWN_Pin GPIO_PIN_5
#define BTN_DWN_GPIO_Port GPIOK
#define EN_R_Pin GPIO_PIN_0
#define EN_R_GPIO_Port GPIOF
#define EN_G_Pin GPIO_PIN_1
#define EN_G_GPIO_Port GPIOF
#define EN_B_Pin GPIO_PIN_2
#define EN_B_GPIO_Port GPIOF
#define DISP_A0_Pin GPIO_PIN_3
#define DISP_A0_GPIO_Port GPIOF
#define RST_DISPLAY_Pin GPIO_PIN_4
#define RST_DISPLAY_GPIO_Port GPIOF
#define Z_AX_REL_EN_Pin GPIO_PIN_3
#define Z_AX_REL_EN_GPIO_Port GPIOH
#define EN_5V_Pin GPIO_PIN_5
#define EN_5V_GPIO_Port GPIOF
#define A_AX_REL_EN_Pin GPIO_PIN_2
#define A_AX_REL_EN_GPIO_Port GPIOH
#define ADC_NOS_Pin GPIO_PIN_2
#define ADC_NOS_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_2
#define LED_GREEN_GPIO_Port GPIOJ
#define ADC_DS_Pin GPIO_PIN_3
#define ADC_DS_GPIO_Port GPIOA
#define LED_RED_Pin GPIO_PIN_0
#define LED_RED_GPIO_Port GPIOJ
#define LED_YELLOW_Pin GPIO_PIN_1
#define LED_YELLOW_GPIO_Port GPIOJ

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

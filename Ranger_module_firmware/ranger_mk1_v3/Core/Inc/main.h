/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 AceHigh ApS
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
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ENABLE_ENC_Pin GPIO_PIN_13
#define ENABLE_ENC_GPIO_Port GPIOC
#define LED1_Pin GPIO_PIN_0
#define LED1_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_1
#define LED2_GPIO_Port GPIOA
#define CS_INA_Pin GPIO_PIN_2
#define CS_INA_GPIO_Port GPIOA
#define WP_MRAM_Pin GPIO_PIN_3
#define WP_MRAM_GPIO_Port GPIOA
#define CS_MRAM_Pin GPIO_PIN_4
#define CS_MRAM_GPIO_Port GPIOA
#define PWM_STEP_DRV_Pin GPIO_PIN_0
#define PWM_STEP_DRV_GPIO_Port GPIOB
#define FAULT_DRV_Pin GPIO_PIN_1
#define FAULT_DRV_GPIO_Port GPIOB
#define MODE_DRV_Pin GPIO_PIN_2
#define MODE_DRV_GPIO_Port GPIOB
#define ENABLE_DRV_Pin GPIO_PIN_10
#define ENABLE_DRV_GPIO_Port GPIOB
#define SLEEP_DRV_Pin GPIO_PIN_11
#define SLEEP_DRV_GPIO_Port GPIOB
#define INTR_P0_Pin GPIO_PIN_8
#define INTR_P0_GPIO_Port GPIOA
#define INTR_P1_Pin GPIO_PIN_9
#define INTR_P1_GPIO_Port GPIOA
#define AUX_ENC_Pin GPIO_PIN_10
#define AUX_ENC_GPIO_Port GPIOA
#define DIR_DRV_Pin GPIO_PIN_15
#define DIR_DRV_GPIO_Port GPIOA
#define CS_P0_Pin GPIO_PIN_6
#define CS_P0_GPIO_Port GPIOB
#define CS_P1_Pin GPIO_PIN_7
#define CS_P1_GPIO_Port GPIOB
#define CS_DRV_Pin GPIO_PIN_9
#define CS_DRV_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

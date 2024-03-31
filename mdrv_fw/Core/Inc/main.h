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
#include "stm32g0xx_hal.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DRV_DIR_Pin GPIO_PIN_7
#define DRV_DIR_GPIO_Port GPIOA
#define DRV_MD3_Pin GPIO_PIN_0
#define DRV_MD3_GPIO_Port GPIOB
#define DRV_MD2_Pin GPIO_PIN_1
#define DRV_MD2_GPIO_Port GPIOB
#define DRV_MD1_Pin GPIO_PIN_2
#define DRV_MD1_GPIO_Port GPIOB
#define DRV_EN_Pin GPIO_PIN_10
#define DRV_EN_GPIO_Port GPIOB
#define DRV_MO_Pin GPIO_PIN_11
#define DRV_MO_GPIO_Port GPIOB
#define SEN_INT_Pin GPIO_PIN_12
#define SEN_INT_GPIO_Port GPIOB
#define SEN_INT_EXTI_IRQn EXTI4_15_IRQn
#define SEN_SCK_Pin GPIO_PIN_13
#define SEN_SCK_GPIO_Port GPIOB
#define SEN_SDO_Pin GPIO_PIN_14
#define SEN_SDO_GPIO_Port GPIOB
#define CD_CS_Pin GPIO_PIN_15
#define CD_CS_GPIO_Port GPIOA
#define LED_G_Pin GPIO_PIN_1
#define LED_G_GPIO_Port GPIOD
#define LED_R_Pin GPIO_PIN_2
#define LED_R_GPIO_Port GPIOD
#define CD_INT_Pin GPIO_PIN_3
#define CD_INT_GPIO_Port GPIOD
#define CD_INT_EXTI_IRQn EXTI2_3_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#include "stm32f1xx_hal.h"

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
#define LED_B_Pin GPIO_PIN_13
#define LED_B_GPIO_Port GPIOC
#define LED_G_Pin GPIO_PIN_14
#define LED_G_GPIO_Port GPIOC
#define LED_R_Pin GPIO_PIN_15
#define LED_R_GPIO_Port GPIOC
#define LIMIT_DET_Pin GPIO_PIN_5
#define LIMIT_DET_GPIO_Port GPIOA
#define LIMIT_DET_EXTI_IRQn EXTI9_5_IRQn
#define CDCTL_RST_N_Pin GPIO_PIN_0
#define CDCTL_RST_N_GPIO_Port GPIOB
#define CDCTL_INT_N_Pin GPIO_PIN_1
#define CDCTL_INT_N_GPIO_Port GPIOB
#define CDCTL_INT_N_EXTI_IRQn EXTI1_IRQn
#define CDCTL_NS_Pin GPIO_PIN_12
#define CDCTL_NS_GPIO_Port GPIOB
#define DRV_STEP_Pin GPIO_PIN_4
#define DRV_STEP_GPIO_Port GPIOB
#define DRV_DIR_Pin GPIO_PIN_5
#define DRV_DIR_GPIO_Port GPIOB
#define DRV_MD3_Pin GPIO_PIN_6
#define DRV_MD3_GPIO_Port GPIOB
#define DRV_MD2_Pin GPIO_PIN_7
#define DRV_MD2_GPIO_Port GPIOB
#define DRV_MD1_Pin GPIO_PIN_8
#define DRV_MD1_GPIO_Port GPIOB
#define DRV_EN_Pin GPIO_PIN_9
#define DRV_EN_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

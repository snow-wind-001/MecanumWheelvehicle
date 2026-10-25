/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f4xx_hal.h"

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
#define DIR_FL_Pin GPIO_PIN_0
#define DIR_FL_GPIO_Port GPIOG
#define DIR_FR_Pin GPIO_PIN_1
#define DIR_FR_GPIO_Port GPIOG
#define IMU_CS_Pin GPIO_PIN_12
#define IMU_CS_GPIO_Port GPIOB
#define PWR_ALERT_N_Pin GPIO_PIN_10
#define PWR_ALERT_N_GPIO_Port GPIOD
#define PWR_ALERT_N_EXTI_IRQn EXTI15_10_IRQn
#define LVL_SHIFT_OE_Pin GPIO_PIN_12
#define LVL_SHIFT_OE_GPIO_Port GPIOD
#define LD06_FAULT_N_Pin GPIO_PIN_11
#define LD06_FAULT_N_GPIO_Port GPIOD
#define LD06_PWR_EN_Pin GPIO_PIN_13
#define LD06_PWR_EN_GPIO_Port GPIOD
#define DRV_WAKE_Pin GPIO_PIN_14
#define DRV_WAKE_GPIO_Port GPIOD
#define DIR_RL_Pin GPIO_PIN_2
#define DIR_RL_GPIO_Port GPIOG
#define DIR_RR_Pin GPIO_PIN_3
#define DIR_RR_GPIO_Port GPIOG
#define FAULT_FL_Pin GPIO_PIN_4
#define FAULT_FL_GPIO_Port GPIOG
#define FAULT_FL_EXTI_IRQn EXTI4_IRQn
#define FAULT_FR_Pin GPIO_PIN_5
#define FAULT_FR_GPIO_Port GPIOG
#define FAULT_FR_EXTI_IRQn EXTI9_5_IRQn
#define FAULT_RL_Pin GPIO_PIN_6
#define FAULT_RL_GPIO_Port GPIOG
#define FAULT_RL_EXTI_IRQn EXTI9_5_IRQn
#define FAULT_RR_Pin GPIO_PIN_7
#define FAULT_RR_GPIO_Port GPIOG
#define FAULT_RR_EXTI_IRQn EXTI9_5_IRQn
#define GNSS_PPS_Pin GPIO_PIN_8
#define GNSS_PPS_GPIO_Port GPIOG
#define GNSS_PPS_EXTI_IRQn EXTI9_5_IRQn
#define PI_SHUTDOWN_REQ_Pin GPIO_PIN_6
#define PI_SHUTDOWN_REQ_GPIO_Port GPIOC
#define PI_PWR_GOOD_Pin GPIO_PIN_7
#define PI_PWR_GOOD_GPIO_Port GPIOC
#define AUX_PWR_GOOD_Pin GPIO_PIN_8
#define AUX_PWR_GOOD_GPIO_Port GPIOC
#define PI_READY_Pin GPIO_PIN_0
#define PI_READY_GPIO_Port GPIOD
#define PI_HALTED_Pin GPIO_PIN_1
#define PI_HALTED_GPIO_Port GPIOD
#define IMU_DRDY_Pin GPIO_PIN_2
#define IMU_DRDY_GPIO_Port GPIOD
#define IMU_DRDY_EXTI_IRQn EXTI2_IRQn
#define PI_PWR_CMD_Pin GPIO_PIN_3
#define PI_PWR_CMD_GPIO_Port GPIOD
#define AUX_PWR_EN_Pin GPIO_PIN_4
#define AUX_PWR_EN_GPIO_Port GPIOD
#define CHG_PRESENT_Pin GPIO_PIN_7
#define CHG_PRESENT_GPIO_Port GPIOD
#define MOTOR_ARM_REQ_Pin GPIO_PIN_10
#define MOTOR_ARM_REQ_GPIO_Port GPIOG
#define ESTOP_OK_Pin GPIO_PIN_11
#define ESTOP_OK_GPIO_Port GPIOG
#define ESTOP_OK_EXTI_IRQn EXTI15_10_IRQn
#define MCU_WDI_Pin GPIO_PIN_12
#define MCU_WDI_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */

#define ESP_EN_Pin GPIO_PIN_4
#define ESP_EN_GPIO_Port GPIOE
#define ESP_BOOT_Pin GPIO_PIN_6
#define ESP_BOOT_GPIO_Port GPIOE

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

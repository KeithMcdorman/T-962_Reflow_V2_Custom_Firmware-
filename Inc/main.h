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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_RS_Pin GPIO_PIN_13
#define LCD_RS_GPIO_Port GPIOC
#define LCD_RW_Pin GPIO_PIN_14
#define LCD_RW_GPIO_Port GPIOC
#define HEAT_CTRL_Pin GPIO_PIN_15
#define HEAT_CTRL_GPIO_Port GPIOC
#define LCD_DB0_Pin GPIO_PIN_0
#define LCD_DB0_GPIO_Port GPIOC
#define LCD_DB1_Pin GPIO_PIN_1
#define LCD_DB1_GPIO_Port GPIOC
#define LCD_DB2_Pin GPIO_PIN_2
#define LCD_DB2_GPIO_Port GPIOC
#define LCD_DB3_Pin GPIO_PIN_3
#define LCD_DB3_GPIO_Port GPIOC
#define TEMP_PROBE_1_Pin GPIO_PIN_0
#define TEMP_PROBE_1_GPIO_Port GPIOA
#define TEMP_PROBE_2_Pin GPIO_PIN_2
#define TEMP_PROBE_2_GPIO_Port GPIOA
#define P6_pin_1_Pin GPIO_PIN_4
#define P6_pin_1_GPIO_Port GPIOA
#define P6_pin_2_Pin GPIO_PIN_5
#define P6_pin_2_GPIO_Port GPIOA
#define COLD_JUNCTION_NTC_Pin GPIO_PIN_6
#define COLD_JUNCTION_NTC_GPIO_Port GPIOA
#define P16_PIN3_Pin GPIO_PIN_7
#define P16_PIN3_GPIO_Port GPIOA
#define LCD_DB4_Pin GPIO_PIN_4
#define LCD_DB4_GPIO_Port GPIOC
#define LCD_DB5_Pin GPIO_PIN_5
#define LCD_DB5_GPIO_Port GPIOC
#define P16_PIN4_Pin GPIO_PIN_0
#define P16_PIN4_GPIO_Port GPIOB
#define P16_PIN5_Pin GPIO_PIN_1
#define P16_PIN5_GPIO_Port GPIOB
#define P10_PIN1_Pin GPIO_PIN_10
#define P10_PIN1_GPIO_Port GPIOB
#define OK_START_Pin GPIO_PIN_11
#define OK_START_GPIO_Port GPIOB
#define P3_PIN3_Pin GPIO_PIN_12
#define P3_PIN3_GPIO_Port GPIOB
#define UP_Pin GPIO_PIN_13
#define UP_GPIO_Port GPIOB
#define P3_PIN2_Pin GPIO_PIN_14
#define P3_PIN2_GPIO_Port GPIOB
#define DOWN_Pin GPIO_PIN_15
#define DOWN_GPIO_Port GPIOB
#define LCD_DB6_Pin GPIO_PIN_6
#define LCD_DB6_GPIO_Port GPIOC
#define LCD_DB7_Pin GPIO_PIN_7
#define LCD_DB7_GPIO_Port GPIOC
#define LCD_E_Pin GPIO_PIN_9
#define LCD_E_GPIO_Port GPIOC
#define P1_PIN5_Pin GPIO_PIN_15
#define P1_PIN5_GPIO_Port GPIOA
#define LCD_CSA_Pin GPIO_PIN_10
#define LCD_CSA_GPIO_Port GPIOC
#define LCD_CSB_Pin GPIO_PIN_11
#define LCD_CSB_GPIO_Port GPIOC
#define FAN_CTRL_Pin GPIO_PIN_12
#define FAN_CTRL_GPIO_Port GPIOC
#define P1_PIN13_Pin GPIO_PIN_3
#define P1_PIN13_GPIO_Port GPIOB
#define P1_PIN3_Pin GPIO_PIN_4
#define P1_PIN3_GPIO_Port GPIOB
#define OPT_CH3_OUT_Pin GPIO_PIN_5
#define OPT_CH3_OUT_GPIO_Port GPIOB
#define P15_PIN2_Pin GPIO_PIN_8
#define P15_PIN2_GPIO_Port GPIOB
#define Buzzer_Pin GPIO_PIN_9
#define Buzzer_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

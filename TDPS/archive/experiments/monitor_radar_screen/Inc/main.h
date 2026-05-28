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
#define RADAR_GPIO_Pin GPIO_PIN_0
#define RADAR_GPIO_GPIO_Port GPIOB
#define LINE2_SIG1_Pin GPIO_PIN_4
#define LINE2_SIG1_GPIO_Port GPIOA
#define LINE2_SIG2_Pin GPIO_PIN_5
#define LINE2_SIG2_GPIO_Port GPIOA
#define ENC_L_A_Pin GPIO_PIN_6
#define ENC_L_A_GPIO_Port GPIOA
#define ENC_L_B_Pin GPIO_PIN_7
#define ENC_L_B_GPIO_Port GPIOA
#define MOT_L_PWM_Pin GPIO_PIN_9
#define MOT_L_PWM_GPIO_Port GPIOE
#define MOT_R_PWM_Pin GPIO_PIN_11
#define MOT_R_PWM_GPIO_Port GPIOE
#define RADAR_TX_Pin GPIO_PIN_10
#define RADAR_TX_GPIO_Port GPIOB
#define RADAR_RX_Pin GPIO_PIN_11
#define RADAR_RX_GPIO_Port GPIOB
#define MOT_L_DIR_Pin GPIO_PIN_12
#define MOT_L_DIR_GPIO_Port GPIOB
#define MOT_R_DIR_Pin GPIO_PIN_13
#define MOT_R_DIR_GPIO_Port GPIOB
#define LORA_LINK_Pin GPIO_PIN_8
#define LORA_LINK_GPIO_Port GPIOC
#define LORA_WAKE_Pin GPIO_PIN_9
#define LORA_WAKE_GPIO_Port GPIOC
#define LORA_AUX_Pin GPIO_PIN_8
#define LORA_AUX_GPIO_Port GPIOA
#define OLED_TX_Pin GPIO_PIN_5
#define OLED_TX_GPIO_Port GPIOD
#define OLED_RX_Pin GPIO_PIN_6
#define OLED_RX_GPIO_Port GPIOD
#define DEBUG_TX_Pin GPIO_PIN_6
#define DEBUG_TX_GPIO_Port GPIOC
#define DEBUG_RX_Pin GPIO_PIN_7
#define DEBUG_RX_GPIO_Port GPIOC
#define LORA_TX_Pin GPIO_PIN_10
#define LORA_TX_GPIO_Port GPIOC
#define LORA_RX_Pin GPIO_PIN_11
#define LORA_RX_GPIO_Port GPIOC
#define ENC_R_A_Pin GPIO_PIN_6
#define ENC_R_A_GPIO_Port GPIOB
#define ENC_R_B_Pin GPIO_PIN_7
#define ENC_R_B_GPIO_Port GPIOB
#define LORA_RST_Pin GPIO_PIN_0
#define LORA_RST_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
#define LED1_Pin GPIO_PIN_0  /* PC0: UART fresh frame */
#define LED1_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_1  /* PC1: PB0 radar GPIO target */
#define LED2_GPIO_Port GPIOC
#define LED3_Pin GPIO_PIN_2  /* PC2: left obstacle by UART distance */
#define LED3_GPIO_Port GPIOC
#define LED4_Pin GPIO_PIN_3  /* PC3: path command TURN_RIGHT */
#define LED4_GPIO_Port GPIOC
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

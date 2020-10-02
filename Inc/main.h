/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
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
#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx.h"
#include "stm32l0xx_ll_i2c.h"
#include "stm32l0xx_ll_lptim.h"
#include "stm32l0xx_ll_crs.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_system.h"
#include "stm32l0xx_ll_exti.h"
#include "stm32l0xx_ll_cortex.h"
#include "stm32l0xx_ll_utils.h"
#include "stm32l0xx_ll_pwr.h"
#include "stm32l0xx_ll_dma.h"
#include "stm32l0xx_ll_spi.h"
#include "stm32l0xx_ll_tim.h"
#include "stm32l0xx_ll_usart.h"
#include "stm32l0xx_ll_gpio.h"

#if defined(USE_FULL_ASSERT)
#include "stm32_assert.h"
#endif /* USE_FULL_ASSERT */

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
#define BATTERY_ADC_IN1_Pin LL_GPIO_PIN_1
#define BATTERY_ADC_IN1_GPIO_Port GPIOA
#define BUZZER_TIM2_CH3_Pin LL_GPIO_PIN_2
#define BUZZER_TIM2_CH3_GPIO_Port GPIOA
#define EPD_DC_Pin LL_GPIO_PIN_3
#define EPD_DC_GPIO_Port GPIOA
#define EPD_RST_Pin LL_GPIO_PIN_4
#define EPD_RST_GPIO_Port GPIOA
#define EPD_SCK_Pin LL_GPIO_PIN_5
#define EPD_SCK_GPIO_Port GPIOA
#define EPD_CS_Pin LL_GPIO_PIN_6
#define EPD_CS_GPIO_Port GPIOA
#define EPD_MOSI_Pin LL_GPIO_PIN_7
#define EPD_MOSI_GPIO_Port GPIOA
#define EPD_BUSY_EXTI0_Pin LL_GPIO_PIN_0
#define EPD_BUSY_EXTI0_GPIO_Port GPIOB
#define EPD_BUSY_EXTI0_EXTI_IRQn EXTI0_1_IRQn
#define EPD_POWER_Pin LL_GPIO_PIN_2
#define EPD_POWER_GPIO_Port GPIOB
#define LED_Pin LL_GPIO_PIN_8
#define LED_GPIO_Port GPIOA
#define SHT30_POWER_Pin LL_GPIO_PIN_11
#define SHT30_POWER_GPIO_Port GPIOA
#define SHT30_RST_Pin LL_GPIO_PIN_12
#define SHT30_RST_GPIO_Port GPIOA
#define BTN_SET_Pin LL_GPIO_PIN_15
#define BTN_SET_GPIO_Port GPIOA
#define BTN_UP_Pin LL_GPIO_PIN_3
#define BTN_UP_GPIO_Port GPIOB
#define BTN_DOWN_Pin LL_GPIO_PIN_4
#define BTN_DOWN_GPIO_Port GPIOB
#define I2C1_PULLUP_Pin LL_GPIO_PIN_5
#define I2C1_PULLUP_GPIO_Port GPIOB
#define DC_POWERSAVE_Pin LL_GPIO_PIN_8
#define DC_POWERSAVE_GPIO_Port GPIOB
#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0         ((uint32_t)0x00000007) /*!< 0 bit  for pre-emption priority,
                                                                 4 bits for subpriority */
#define NVIC_PRIORITYGROUP_1         ((uint32_t)0x00000006) /*!< 1 bit  for pre-emption priority,
                                                                 3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         ((uint32_t)0x00000005) /*!< 2 bits for pre-emption priority,
                                                                 2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         ((uint32_t)0x00000004) /*!< 3 bits for pre-emption priority,
                                                                 1 bit  for subpriority */
#define NVIC_PRIORITYGROUP_4         ((uint32_t)0x00000003) /*!< 4 bits for pre-emption priority,
                                                                 0 bit  for subpriority */
#endif
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

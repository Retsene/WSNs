/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bme280.h"
#include "sx1278.h"
#include "eink_display.h"
#include "power_manager.h"
#include "cmd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

#define STATION_ID 0x01

static const char *lora_mode_label(SX1278_Config cfg)
{
    switch (cfg) {
    case SX1278_CFG_POWER:       return "LoRa:Pwr";
    case SX1278_CFG_BALANCED:    return "LoRa:Bal";
    case SX1278_CFG_PERFORMANCE: return "LoRa:Perf";
    default:                     return "LoRa: ?";
    }
}

BME280_HandleTypeDef bme280;
BME280_Data sensor_data;
char print_buf[128];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

static void MX_GPIO_Init(void);

static void MX_I2C3_Init(void);

static void MX_SPI1_Init(void);

static void MX_SPI2_Init(void);

static void MX_USART2_UART_Init(void);

static void MX_RTC_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

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
    MX_I2C3_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();
    MX_RTC_Init();
    CMD_Init(&huart2);
    /* USER CODE BEGIN 2 */

    PM_Init();

    if (!BME280_Init(&bme280, &hi2c3, BME280_I2C_ADDR_PRIM))
    {
        Error_Handler();
    }

    if (!SX1278_Init(g_lora_cfg))
    {
        Error_Handler();
    }

    {
        const char *mode = lora_mode_label(g_lora_cfg);
        snprintf(print_buf, sizeof(print_buf), "%s\r\n", mode);
        HAL_UART_Transmit(&huart2, (uint8_t *)print_buf, strlen(print_buf), 1000);
    }

    {
        EINK_Config eink_cfg;
        eink_cfg.width = EINK_WIDTH;
        eink_cfg.height = EINK_HEIGHT;
        EINK_Init(&eink_cfg);
    }
    EINK_Clear();
    EINK_DrawStringScaled((200 - 12*12)/2, 86, "Booting...", 2);
    EINK_Update();
    EINK_Sleep();

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        BME280_ReadAll(&bme280, &sensor_data);
        sensor_data.temperature += g_offset_temp;
        sensor_data.humidity    += g_offset_hum;
        sensor_data.pressure    += g_offset_pres;

        const int t = (int) sensor_data.temperature;
        const int t_d = (int) ((sensor_data.temperature - (float) t) * 10.0f);
        const int h = (int) sensor_data.humidity;
        const int h_d = (int) ((sensor_data.humidity - (float) h) * 10.0f);
        const int p = (int) sensor_data.pressure;

        snprintf(print_buf, sizeof(print_buf),
                 "%s  Temp: %d.%d C  Humidity: %d.%d %%  Pres: %d Pa\r\n",
                 lora_mode_label(g_lora_cfg),
                 t, t_d < 0 ? -t_d : t_d,
                 h, h_d < 0 ? -h_d : h_d, p);
        HAL_UART_Transmit(&huart2, (uint8_t *)print_buf, strlen(print_buf), 1000);

        uint8_t payload[10];
        {
            int16_t te = (int16_t)(sensor_data.temperature * 10.0f);
            int16_t he = (int16_t)(sensor_data.humidity * 10.0f);
            uint16_t pe = (uint16_t)(sensor_data.pressure / 10.0f);

            payload[0] = STATION_ID;
            payload[1] = (te >> 8) & 0xFF;
            payload[2] = te & 0xFF;
            payload[3] = (he >> 8) & 0xFF;
            payload[4] = he & 0xFF;
            payload[5] = (pe >> 8) & 0xFF;
            payload[6] = pe & 0xFF;
            payload[7] = 0;
            for (int i = 0; i < 7; i++) payload[7] ^= payload[i];
            payload[8] = (uint8_t)(HAL_GetTick() & 0xFF);
            payload[9] = 0xAA;
        }

        SX1278_SendPacket(payload, 10);
        SX1278_EnterSleep();

        {
            int cx;

            EINK_Clear();

            snprintf(print_buf, sizeof(print_buf), "Station #01");
            cx = (EINK_WIDTH - (int)strlen(print_buf) * 12) / 2;
            EINK_DrawStringScaled(cx > 0 ? cx : 0, 8, print_buf, 2);

            snprintf(print_buf, sizeof(print_buf), "Temp: %d.%d C",
                     t, t_d < 0 ? -t_d : t_d);
            cx = (EINK_WIDTH - (int)strlen(print_buf) * 12) / 2;
            EINK_DrawStringScaled(cx > 0 ? cx : 0, 36, print_buf, 2);

            snprintf(print_buf, sizeof(print_buf), "Humidity: %d.%d %%",
                     h, h_d < 0 ? -h_d : h_d);
            cx = (EINK_WIDTH - (int)strlen(print_buf) * 12) / 2;
            EINK_DrawStringScaled(cx > 0 ? cx : 0, 62, print_buf, 2);

            snprintf(print_buf, sizeof(print_buf), "Pres: %d Pa", p);
            cx = (EINK_WIDTH - (int)strlen(print_buf) * 12) / 2;
            EINK_DrawStringScaled(cx > 0 ? cx : 0, 88, print_buf, 2);

            snprintf(print_buf, sizeof(print_buf), "%s", lora_mode_label(g_lora_cfg));
            cx = (EINK_WIDTH - (int)strlen(print_buf) * 12) / 2;
            EINK_DrawStringScaled(cx > 0 ? cx : 0, 114, print_buf, 2);
        }
        EINK_Update();
        EINK_Sleep();

        CMD_Poll();

        {
            uint32_t wait_end = HAL_GetTick() + g_interval_ms;
            while (HAL_GetTick() < wait_end) {
                CMD_Poll();
                if (CMD_ConsumeDirty()) break;
                HAL_Delay(100);
            }
        }
    }
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
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_I2C3
                                         | RCC_PERIPHCLK_RTC;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{
    /* USER CODE BEGIN I2C3_Init 0 */

    /* USER CODE END I2C3_Init 0 */

    /* USER CODE BEGIN I2C3_Init 1 */

    /* USER CODE END I2C3_Init 1 */
    hi2c3.Instance = I2C3;
    hi2c3.Init.Timing = 0x00000608;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c3) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Analogue filter
    */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Digital filter
    */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C3_Init 2 */

    /* USER CODE END I2C3_Init 2 */
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
    /* USER CODE BEGIN RTC_Init 0 */

    /* USER CODE END RTC_Init 0 */

    /* USER CODE BEGIN RTC_Init 1 */

    /* USER CODE END RTC_Init 1 */

    /** Initialize RTC Only
    */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /** Enable the WakeUp
    */
    if (HAL_RTCEx_SetWakeUpTimer(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN RTC_Init 2 */

    /* USER CODE END RTC_Init 2 */
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{
    /* USER CODE BEGIN SPI1_Init 0 */

    /* USER CODE END SPI1_Init 0 */

    /* USER CODE BEGIN SPI1_Init 1 */

    /* USER CODE END SPI1_Init 1 */
    /* SPI1 parameter configuration*/
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN SPI1_Init 2 */

    /* USER CODE END SPI1_Init 2 */
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
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN SPI2_Init 2 */

    /* USER CODE END SPI2_Init 2 */
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
    /* USER CODE BEGIN USART2_Init 0 */

    /* USER CODE END USART2_Init 0 */

    /* USER CODE BEGIN USART2_Init 1 */

    /* USER CODE END USART2_Init 1 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART2_Init 2 */

    /* USER CODE END USART2_Init 2 */
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
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, LORA_RST_Pin | EINK_CS_Pin | EINK_RES_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(EINK_D_C_GPIO_Port, EINK_D_C_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin : LORA_NSS_Pin */
    GPIO_InitStruct.Pin = LORA_NSS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_NSS_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : LORA_DIO0_Pin */
    GPIO_InitStruct.Pin = LORA_DIO0_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LORA_DIO0_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : LORA_RST_Pin EINK_CS_Pin EINK_RES_Pin */
    GPIO_InitStruct.Pin = LORA_RST_Pin | EINK_CS_Pin | EINK_RES_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pin : EINK_D_C_Pin */
    GPIO_InitStruct.Pin = EINK_D_C_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(EINK_D_C_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : EINK_BUSY_Pin */
    GPIO_InitStruct.Pin = EINK_BUSY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EINK_BUSY_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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

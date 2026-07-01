#include "power_manager.h"

extern RTC_HandleTypeDef hrtc;
extern void SystemClock_Config(void);

void PM_Init(void)
{
}

static void PM_ConfigureGPIOForSleep(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                          GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = 0xFFFF & ~(EINK_D_C_Pin | EINK_BUSY_Pin);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = 0xFFFF & ~(LORA_DIO0_Pin | LORA_RST_Pin | EINK_CS_Pin | EINK_RES_Pin);
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_SuspendTick();
}

void PM_EnterStop(uint32_t seconds)
{
    PM_ConfigureGPIOForSleep();

    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

    HAL_RTCEx_SetWakeUpTimer(&hrtc, (uint16_t)seconds,
                             RTC_WAKEUPCLOCK_CK_SPRE);

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_PWREx_EnableUltraLowPower();
    HAL_PWREx_EnableFastWakeUp();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    HAL_ResumeTick();
    SystemClock_Config();
}

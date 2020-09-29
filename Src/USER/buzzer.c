#include "buzzer.h"
#include <math.h>

/**
 * @brief  打开蜂鸣器定时器。
 */
void BUZZER_Enable(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    LL_TIM_SetCounter(BUZZER_TIMER, LL_TIM_GetAutoReload(BUZZER_TIMER) / 2);
    LL_TIM_EnableCounter(BUZZER_TIMER);
}

/**
 * @brief  关闭蜂鸣器定时器。
 */
void BUZZER_Disable(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    LL_TIM_DisableCounter(BUZZER_TIMER);
}

/**
 * @brief  设置蜂鸣器频率。
 * @param  freq 蜂鸣器频率。
 */
void BUZZER_SetFrqe(uint32_t freq)
{
    uint32_t autoreload;

    if (freq > BUZZER_CLOCK)
    {
        autoreload = 1;
    }
    else
    {
        autoreload = (BUZZER_CLOCK / freq) - 1;
        if (autoreload > 0xFFFF)
        {
            autoreload = 1;
        }
    }
    LL_TIM_SetAutoReload(BUZZER_TIMER, autoreload);
    if (LL_TIM_GetCounter(BUZZER_TIMER) > autoreload)
    {
        LL_TIM_SetCounter(BUZZER_TIMER, 0);
    }
}

/**
 * @brief  设置蜂鸣器音量。
 * @param  vol 蜂鸣器音量，范围为：0 ~ 10。
 */
void BUZZER_SetVolume(uint8_t vol)
{
    uint32_t autoreload;

    autoreload = LL_TIM_GetAutoReload(BUZZER_TIMER);
    vol = vol % 11;
    BUZZER_OC_SET_FUNC(BUZZER_TIMER, ((autoreload / 2.0) / 100) * pow(vol, 2.0));
}

/**
 * @brief  蜂鸣器响一声。
 * @param  time_ms 持续时间。
 */
void BUZZER_Beep(uint16_t time_ms)
{
    if (time_ms != 0)
    {
        LL_TIM_SetCounter(BUZZER_TIMER, LL_TIM_GetAutoReload(BUZZER_TIMER) / 2);
        LL_TIM_CC_EnableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    }
    LL_mDelay(time_ms);
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

/**
 * @brief  打开蜂鸣器，在关闭前持续鸣响。
 */
void BUZZER_Start(void)
{
    LL_TIM_SetCounter(BUZZER_TIMER, LL_TIM_GetAutoReload(BUZZER_TIMER) / 2);
    LL_TIM_CC_EnableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

/**
 * @brief  关闭蜂鸣器，停止鸣响。
 */
void BUZZER_Stop(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

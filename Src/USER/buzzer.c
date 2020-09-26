#include "buzzer.h"

uint8_t BUZZER_Enable(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    LL_TIM_SetCounter(BUZZER_TIMER, 0);
    LL_TIM_EnableCounter(BUZZER_TIMER);
    return 0;
}

uint8_t BUZZER_Disable(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    LL_TIM_DisableCounter(BUZZER_TIMER);
    LL_TIM_SetCounter(BUZZER_TIMER, 0);
    return 0;
}

void BUZZER_SetVolume(uint8_t vol)
{
    uint32_t autoreload;

    autoreload = LL_TIM_GetAutoReload(BUZZER_TIMER);
    vol = vol % 101;

    BUZZER_OC_SET_FUNC(BUZZER_TIMER, (autoreload / 2.0) / 100.0 * vol);
}

void BUZZER_SetFrqe(uint32_t freq)
{
    uint32_t autoreload;

    if (autoreload > 1000000)
    {
        autoreload = 1;
    }
    else
    {
        autoreload = 1000000 / freq;
        if (autoreload > 0xFFFF)
        {
            autoreload = 0;
        }
    }
    LL_TIM_SetAutoReload(BUZZER_TIMER, autoreload);
    if (LL_TIM_GetCounter(BUZZER_TIMER) > autoreload)
    {
        LL_TIM_SetCounter(BUZZER_TIMER, 0);
    }
}

void BUZZER_Beep(uint16_t freq, uint16_t time_ms)
{
    if (freq != 0)
    {
        BUZZER_SetFrqe(freq);
        LL_TIM_CC_EnableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
    }
    LL_mDelay(time_ms);
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

void BUZZER_Start(void)
{
    LL_TIM_CC_EnableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

void BUZZER_Stop(void)
{
    LL_TIM_CC_DisableChannel(BUZZER_TIMER, BUZZER_CHANNEL);
}

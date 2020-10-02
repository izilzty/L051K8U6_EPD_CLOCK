#include "lowpower.h"

/**
 * @brief  低功耗定时器初始化。
 * @param  auto_reload 定时器重载计数，1等于延时865毫秒
 */
static void lptim_init(uint16_t auto_reload)
{
    if (LL_LPTIM_IsEnabled(LP_LPTIM_NUM) != 0)
    {
        LL_LPTIM_Disable(LP_LPTIM_NUM);
        LL_LPTIM_DisableIT_ARRM(LP_LPTIM_NUM);
    }
    LL_LPTIM_Enable(LP_LPTIM_NUM);
    LL_LPTIM_SetAutoReload(LP_LPTIM_NUM, LP_LPTIM_FINAL_CLK * auto_reload);
    LL_LPTIM_ClearFLAG_ARRM(LP_LPTIM_NUM);
    LL_LPTIM_EnableIT_ARRM(LP_LPTIM_NUM);
    LL_LPTIM_StartCounter(LP_LPTIM_NUM, LL_LPTIM_OPERATING_MODE_ONESHOT);
}

/**
 * @brief  低功耗定时器中断回调，需手动将此函数添加在LPTIM1_IRQHandler()内。
 */
void LP_LPTIM_IRQCallback(void)
{
    LL_LPTIM_Disable(LP_LPTIM_NUM);        /* 关闭低功耗定时器 */
    LL_LPTIM_DisableIT_ARRM(LP_LPTIM_NUM); /* 关闭重载中断 */
}

/**
 * @brief  手动禁用调试，防止Keil下载完成后不清除标志会造成电流异常消耗（使用STM32 ST-LINK Utility下载无此问题）。
 */
void LP_DisableDebug(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_DBGMCUEN;                                              /* __HAL_RCC_DBGMCU_CLK_ENABLE(); */
    DBGMCU->CR &= ~(DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY); /* HAL_DBGMCU_DisableDBGSleepMode(); ... */
    RCC->APB2ENR &= ~RCC_APB2ENR_DBGMCUEN;                                             /* __HAL_RCC_DBGMCU_CLK_DISABLE(); */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_13, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_14, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_13, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_14, LL_GPIO_PULL_NO);
}

/**
 * @brief  获取系统复位类型。
 * @return LP_RESET_NORMALRESET：普通复位（nRST按键复位、看门狗复位...），LP_RESET_POWERON：上电复位（电源连接，安装电池...），LP_RESET_WKUPSTANDBY：从Standby模式唤醒。
 * @note   获取后将清除标志，无法获取第二次。
 */
uint8_t LP_GetResetInfo(void)
{
    uint8_t ret;
    if (LL_RCC_IsActiveFlag_PORRST() != 0) /* 上电复位 */
    {
        ret = LP_RESET_POWERON;
    }
    else if ((RCC->CSR & 0xFF000000) != 0) /* 所有普通复位，如果需要可以继续细分 */
    {
        ret = LP_RESET_NORMALRESET;
    }
    else if (LL_PWR_IsActiveFlag_SB() != 0) /* 从Standby模式唤醒 */
    {
        ret = LP_RESET_WKUPSTANDBY;
    }
    else /* 没有复位产生 */
    {
        ret = 0;
    }
    LL_PWR_ClearFlag_SB();    /* 清除从Standby模式唤醒标志 */
    LL_RCC_ClearResetFlags(); /* 清除普通复位标志 */
    return ret;
}

/**
 * @brief  进入Sleep模式。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒最快，电力消耗较多。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterSleep(void)
{
    volatile uint32_t imr_save;

    LL_EXTI_DisableIT_0_31(LP_WKUP_EXTI);       /* 禁用IO唤醒中断 */
    LL_EXTI_DisableIT_0_31(LP_LPTIM_WKUP_EXTI); /* 禁用定时器唤醒中断 */
    imr_save = EXTI->IMR;                       /* 保存中断寄存器设置值 */
    EXTI->IMR = 0x00000000;                     /* 禁用所有中断，防止意外唤醒 */
    EXTI->PR = 0x007BFFFF;                      /* 清除所有待执行中断 */
    LL_EXTI_EnableIT_0_31(LP_WKUP_EXTI);        /* 启用IO唤醒中断 */
    LL_EXTI_EnableIT_0_31(LP_LPTIM_WKUP_EXTI);  /* 启用定时器唤醒中断 */

    LL_LPM_EnableSleep();            /* 准备进入Sleep模式 */
    LL_FLASH_EnableSleepPowerDown(); /* 进入Sleep模式后，关闭Flash电源，减小电流消耗 */
    __WFI();                         /* 进入Sleep模式，等待唤醒引脚唤醒 */
    /* 
        唤醒后首先会进入EXTI0_1_IRQHandler()或LPTIM1_IRQHandler()
        在中断函数EXTI0_1_IRQHandler()里由LL_EXTI_ClearFlag_0_31()清除中断标志并返回（中断函数的内容由CubeMX自动生成，不需要进行修改。）
        在中断函数LPTIM1_IRQHandler()里由LP_LPTIM_IRQCallback()清除中断标志并返回（需要手动添加。）
    */
    EXTI->IMR = imr_save; /* 恢复中断寄存器设置 */
}

/**
 * @brief  进入Stop模式。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒较快，电力消耗较少。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterStop(void)
{
    volatile uint32_t imr_save;

    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */

    LL_EXTI_DisableIT_0_31(LP_WKUP_EXTI);       /* 禁用IO唤醒中断 */
    LL_EXTI_DisableIT_0_31(LP_LPTIM_WKUP_EXTI); /* 禁用定时器唤醒中断 */
    imr_save = EXTI->IMR;                       /* 保存中断寄存器设置值 */
    EXTI->IMR = 0x00000000;                     /* 禁用所有中断，防止意外唤醒 */
    EXTI->PR = 0x007BFFFF;                      /* 清除所有待执行中断 */
    LL_EXTI_EnableIT_0_31(LP_WKUP_EXTI);        /* 启IO用唤醒中断 */
    LL_EXTI_EnableIT_0_31(LP_LPTIM_WKUP_EXTI);  /* 启用定时器唤醒中断 */

    LL_PWR_EnableUltraLowPower();                                /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_DisableFastWakeUp();                                  /* 唤醒后等待VREFINT恢复 */
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI); /* 设置唤醒后的系统时钟源为HSI16，默认唤醒后为MSI */
    LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_LOW_POWER);        /* 设置进入低功耗模式后，稳压器为低功耗模式 */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STOP);                       /* 设置DeepSleep为Stop模式 */
    LL_LPM_EnableDeepSleep();                                    /* 准备进入Stop模式 */
    __WFI();                                                     /* 进入Stop模式，等待中断唤醒 */
    /* 
        唤醒后首先会进入EXTI0_1_IRQHandler()或LPTIM1_IRQHandler()
        在中断函数EXTI0_1_IRQHandler()里由LL_EXTI_ClearFlag_0_31()清除中断标志并返回（中断函数的内容由CubeMX自动生成，不需要进行修改。）
        在中断函数LPTIM1_IRQHandler()里由LP_LPTIM_IRQCallback()清除中断标志并返回（需要手动添加。）
    */
    EXTI->IMR = imr_save;          /* 恢复中断寄存器设置 */
    LL_PWR_DisableUltraLowPower(); /* 恢复电源配置 */
}

/**
 * @brief  进入Standby模式。
 * @note   进入后除唤醒IO以外的IO均自动变为高阻状态。
 * @note   唤醒最慢，电力消耗最少。
 * @note   唤醒后类似按键复位，程序重头开始执行。
 */
void LP_EnterStandby(void)
{
    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */
    LL_PWR_ClearFlag_SB();                        /* 清除Standby唤醒标志 */
    LL_PWR_EnableWakeUpPin(LP_STANDBY_WKUP_PIN);  /* 启用Standby唤醒引脚 */

    LL_RCC_DisableRTC();                      /* 关闭实时时钟 */
    LL_RCC_LSI_Disable();                     /* 关闭37kHz振荡器 */
    LL_RCC_LSE_Disable();                     /* 关闭外部低速振荡器 */
    LL_PWR_EnableUltraLowPower();             /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY); /* 设置DeepSleep为Standby模式 */
    LL_LPM_EnableDeepSleep();                 /* 准备进入Standby模式 */
    __WFI();                                  /* 进入Standby模式，等待唤醒引脚唤醒 */
}

/**
 * @brief  进入带超时退出的Sleep模式。
 * @param  auto_reload 定时器重载计数，1等于延时865毫秒
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒最快，电力消耗较多。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterSleepWithTimeout(uint16_t auto_reload)
{
    lptim_init(auto_reload);
    LP_EnterSleep();
}

/**
 * @brief  进入带超时退出的Stop模式。
 * @param  auto_reload 定时器重载计数，1等于延时865毫秒
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒较快，电力消耗较少。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterStopWithTimeout(uint16_t auto_reload)
{
    lptim_init(auto_reload);
    LP_EnterStop();
}

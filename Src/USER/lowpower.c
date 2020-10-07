#include "lowpower.h"

/**
 * @brief  开启唤醒外部中断。
 */
static void wkup_exti_init(void)
{
    LL_EXTI_DisableIT_0_31(LP_WKUP_EXTI); /* 关闭外部中断 */
    LL_EXTI_ClearFlag_0_31(LP_WKUP_EXTI); /* 清除中断标志 */
    NVIC_ClearPendingIRQ(LP_WKUP_IRQ);    /* 清除待处理中断 */
    LL_EXTI_EnableIT_0_31(LP_WKUP_EXTI);  /* 打开外部中断 */
    NVIC_EnableIRQ(LP_WKUP_IRQ);          /* 打开中断请求 */
    NVIC_SetPriority(LP_WKUP_IRQ, 0);     /* 设置中断请求优先级 */
}

/**
 * @brief  关闭唤醒外部中断。
 */
static void wkup_exti_deinit(void)
{
    NVIC_DisableIRQ(LP_WKUP_IRQ);         /* 关闭中断请求 */
    LL_EXTI_DisableIT_0_31(LP_WKUP_EXTI); /* 关闭外部中断 */
    LL_EXTI_ClearFlag_0_31(LP_WKUP_EXTI); /* 清除中断标志 */
    NVIC_ClearPendingIRQ(LP_WKUP_IRQ);    /* 清除待处理中断 */
}

/**
 * @brief  低功耗定时器初始化。
 * @param  ms 定时时间，单位为毫秒
 */
static void lptim_init(uint16_t ms)
{
    if (LL_LPTIM_IsEnabled(LP_LPTIM_NUM) == 0) /* 确保已打开低功耗定时器 */
    {
        LL_LPTIM_Enable(LP_LPTIM_NUM);
    }
    LL_LPTIM_SetAutoReload(LP_LPTIM_NUM, (ms / (1000 / LP_LPTIM_FINAL_CLK)) + 0.5); /* 设置重载数值 */
    LL_LPTIM_EnableIT_ARRM(LP_LPTIM_NUM);                                           /* 打开重载数值匹配中断 */
    LL_EXTI_EnableIT_0_31(LP_LPTIM_EXTI);                                           /* 打开外部中断 */
    NVIC_EnableIRQ(LP_LPTIM_WKUP_IRQ);                                              /* 打开中断请求 */
    NVIC_SetPriority(LP_LPTIM_WKUP_IRQ, 0);                                         /* 设置中断请求优先级 */
    LL_LPTIM_StartCounter(LP_LPTIM_NUM, LL_LPTIM_OPERATING_MODE_ONESHOT);           /* 开始计数 */
}

/**
 * @brief  低功耗定时器关闭。
 */
static void lptim_deinit(void)
{
    NVIC_DisableIRQ(LP_LPTIM_WKUP_IRQ);      /* 关闭中断请求 */
    LL_LPTIM_DisableIT_ARRM(LP_LPTIM_NUM);   /* 关闭重载数值匹配中断 */
    LL_LPTIM_ClearFLAG_ARRM(LP_LPTIM_NUM);   /* 清除中断标志 */
    LL_EXTI_ClearFlag_0_31(LP_LPTIM_EXTI);   /* 清除中断标志 */
    NVIC_ClearPendingIRQ(LP_LPTIM_WKUP_IRQ); /* 清除待处理中断 */
    LL_LPTIM_Disable(LP_LPTIM_NUM);          /* 关闭低功耗定时器 */
}

/**
 * @brief  手动禁用调试，防止Keil下载完成后不进行断电重启的话会造成电流异常消耗（使用STM32 ST-LINK Utility下载无此问题）。
 */
void LP_DisableDebug(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_DBGMCUEN;                                              /* __HAL_RCC_DBGMCU_CLK_ENABLE(); */
    DBGMCU->CR &= ~(DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY); /* HAL_DBGMCU_DisableDBGSleepMode(); ... */
    RCC->APB2ENR &= ~RCC_APB2ENR_DBGMCUEN;                                             /* __HAL_RCC_DBGMCU_CLK_DISABLE(); */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_13, LL_GPIO_MODE_ANALOG);                    /* 将调试IO设置为模拟输入 */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_14, LL_GPIO_MODE_ANALOG);                    /* 将调试IO设置为模拟输入 */
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_13, LL_GPIO_PULL_NO);                        /* 关闭调试IO内部上下拉电阻 */
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_14, LL_GPIO_PULL_NO);                        /* 关闭调试IO内部上下拉电阻 */
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
 * @brief  进入Sleep模式，等待中断唤醒。
 * @param  ms 超时时间，0为永不超时，每增加1超时时间大约增加1毫秒。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒最快，电力消耗较多。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterSleep(uint16_t ms)
{
    uint32_t voltage_scale;

    __disable_irq(); /* 暂停响应所有中断 */

    wkup_exti_init();
    if (ms != 0)
    {
        lptim_init(ms); /* 初始化低功耗定时器 */
    }

    voltage_scale = LL_PWR_GetRegulVoltageScaling();
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2); /* 设置Vcore电压等级到二级，CPU最高允许8Mhz */
    LL_LPM_EnableSleep();                                      /* 准备进入Sleep模式 */
    LL_FLASH_EnableSleepPowerDown();                           /* 进入Sleep模式后，关闭Flash电源，减小电流消耗 */
    __WFI();                                                   /* 进入Sleep模式，等待唤醒引脚唤醒 */
    LL_PWR_SetRegulVoltageScaling(voltage_scale);              /* 恢复Vcore电压等级 */

    wkup_exti_deinit();
    if (ms != 0)
    {
        lptim_deinit(); /* 关闭低功耗定时器 */
    }

    __enable_irq(); /* 重新响应所有中断 */
}

/**
 * @brief  进入Stop模式，等待中断唤醒，进入前需确保I2C没有数据传输或暂时关闭I2C，详见 dm00114897 第16页 2.5.1。
 * @param  ms 超时时间，0为永不超时，每增加1超时时间大约增加1毫秒。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒较快，电力消耗较少。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterStop(uint16_t ms)
{
    uint32_t voltage_scale;

    __disable_irq(); /* 暂停响应所有中断 */

    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */
    wkup_exti_init();
    if (ms != 0)
    {
        lptim_init(ms); /* 初始化低功耗定时器 */
    }

    voltage_scale = LL_PWR_GetRegulVoltageScaling();
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);   /* 设置Vcore电压等级到二级，CPU最高允许8Mhz */
    LL_PWR_EnableUltraLowPower();                                /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_DisableFastWakeUp();                                  /* 唤醒后等待VREFINT恢复 */
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI); /* 设置唤醒后的系统时钟源为HSI16，默认唤醒后为MSI */
    LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_LOW_POWER);        /* 设置进入低功耗模式后，稳压器为低功耗模式 */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STOP);                       /* 设置DeepSleep为Stop模式 */
    LL_LPM_EnableDeepSleep();                                    /* 准备进入Stop模式 */
    __WFI();                                                     /* 进入Stop模式，等待中断唤醒 */
    LL_PWR_SetRegulVoltageScaling(voltage_scale);                /* 恢复Vcore电压等级 */

    wkup_exti_deinit();
    LL_PWR_DisableUltraLowPower(); /* 恢复电源配置 */
    if (ms != 0)
    {
        lptim_deinit(); /* 关闭低功耗定时器 */
    }

    __enable_irq(); /* 重新响应所有中断 */
}

/**
 * @brief  进入Standby模式，等待WKUP引脚或复位唤醒。
 * @note   进入后除唤醒IO以外的IO均自动变为高阻状态。
 * @note   唤醒最慢，电力消耗最少。
 * @note   唤醒后类似按键复位，程序重头开始执行。
 */
void LP_EnterStandby(void)
{
    __disable_irq(); /* 暂停响应所有中断 */

    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */
    LL_PWR_ClearFlag_SB();                        /* 清除Standby唤醒标志 */
    LL_PWR_EnableWakeUpPin(LP_STANDBY_WKUP_PIN);  /* 启用Standby唤醒引脚 */

    LL_RCC_DisableRTC();  /* 关闭实时时钟 */
    LL_RCC_LSI_Disable(); /* 关闭37kHz振荡器 */
    LL_RCC_LSE_Disable(); /* 关闭外部低速振荡器 */

    LL_PWR_EnableUltraLowPower();             /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY); /* 设置DeepSleep为Standby模式 */
    LL_LPM_EnableDeepSleep();                 /* 准备进入Standby模式 */
    __WFI();                                  /* 进入Standby模式，等待唤醒引脚唤醒 */
}

/**
 * @brief  进入Sleep模式并等待一段时间后退出。
 * @param  ms 延时时间，每增加1时间增加LP_LPTIM_AUTORELOAD_MS。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒最快，电力消耗较多。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_DelayStop(uint16_t ms)
{
    uint32_t voltage_scale;

    if (ms == 0)
    {
        return;
    }
    __disable_irq(); /* 暂停响应所有中断 */

    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */
    wkup_exti_deinit();
    lptim_init(ms); /* 初始化低功耗定时器 */

    voltage_scale = LL_PWR_GetRegulVoltageScaling();             /* 保存当前电压等级 */
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);   /* 设置Vcore电压等级到二级，CPU最高允许8Mhz */
    LL_PWR_EnableUltraLowPower();                                /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_DisableFastWakeUp();                                  /* 唤醒后等待VREFINT恢复 */
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI); /* 设置唤醒后的系统时钟源为HSI16，默认唤醒后为MSI */
    LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_LOW_POWER);        /* 设置进入低功耗模式后，稳压器为低功耗模式 */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STOP);                       /* 设置DeepSleep为Stop模式 */
    LL_LPM_EnableDeepSleep();                                    /* 准备进入Stop模式 */
    __WFI();                                                     /* 进入Stop模式，等待中断唤醒 */
    LL_PWR_SetRegulVoltageScaling(voltage_scale);                /* 恢复Vcore电压等级 */

    LL_PWR_DisableUltraLowPower(); /* 恢复电源配置 */
    lptim_deinit();                /* 关闭低功耗定时器 */

    __enable_irq(); /* 重新响应所有中断 */
}

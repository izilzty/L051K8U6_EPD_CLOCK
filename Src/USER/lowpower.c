#include "lowpower.h"

/**
 * @brief  获取系统复位类型。
 * @return LP_RESET_NORMALRESET：普通复位（nRST按键复位、看门狗复位...），LP_RESET_POWERON：上电复位（电源连接，安装电池...），LP_RESET_WKUPSTANDBY：从Standby模式唤醒复位。
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
    uint32_t it_save;

    LL_EXTI_DisableIT_0_31(LP_STOP_WKUP_EXTI); /* 禁用唤醒中断 */
    it_save = EXTI->IMR;                       /* 保存中断寄存器设置值 */
    EXTI->IMR = 0x00000000;                    /* 禁用所有中断，防止意外唤醒 */
    EXTI->PR = 0x007BFFFF;                     /* 清除所有待执行中断 */
    LL_EXTI_EnableIT_0_31(LP_STOP_WKUP_EXTI);  /* 启用唤醒中断 */

    LL_LPM_EnableSleep(); /* 准备进入Sleep模式 */
    // LL_FLASH_DisableSleepPowerDown(); /* 进入Sleep模式后，不关闭Flash电源，增加唤醒速度 */
    LL_FLASH_EnableSleepPowerDown(); /* 进入Sleep模式后，关闭Flash电源，减小电流消耗 */
    __WFI();                         /* 进入Sleep模式，等待唤醒引脚唤醒 */
    /* 唤醒后首先会进入EXTI0_1_IRQHandler()，在中断函数里由LL_EXTI_ClearFlag_0_31()清除中断标志并返回（中断函数的内容全部由CubeMX自动生成，不需要进行修改。） */
    EXTI->IMR = it_save; /* 恢复中断寄存器设置 */
}

/**
 * @brief  进入Stop模式。
 * @note   进入后所有IO状态保持不变。
 * @note   唤醒较快，电力消耗较少。
 * @note   唤醒后程序从停止位置继续执行。
 */
void LP_EnterStop(void)
{
    uint32_t it_save;

    LL_PWR_DisableWakeUpPin(LP_STANDBY_WKUP_PIN); /* 禁用Standby唤醒引脚 */
    LL_PWR_ClearFlag_WU();                        /* 清除Standby唤醒标志 */

    LL_EXTI_DisableIT_0_31(LP_STOP_WKUP_EXTI); /* 禁用唤醒中断 */
    it_save = EXTI->IMR;                       /* 保存中断寄存器设置值 */
    EXTI->IMR = 0x00000000;                    /* 禁用所有中断，防止意外唤醒 */
    EXTI->PR = 0x007BFFFF;                     /* 清除所有待执行中断 */
    LL_EXTI_EnableIT_0_31(LP_STOP_WKUP_EXTI);  /* 启用唤醒中断 */

    LL_PWR_EnableUltraLowPower();                                /* 进入低功耗模式后，关闭VREFINT */
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI); /* 设置唤醒后的系统时钟源为HSI16，默认唤醒后为MSI */
    LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_LOW_POWER);        /* 设置进入低功耗模式后，稳压器为低功耗模式 */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STOP);                       /* 设置DeepSleep为Stop模式 */
    LL_LPM_EnableDeepSleep();                                    /* 准备进入Stop模式 */
    __WFI();                                                     /* 进入Stop模式，等待中断唤醒 */
    /* 唤醒后首先会进入EXTI0_1_IRQHandler()，在中断函数里由LL_EXTI_ClearFlag_0_31()清除中断标志并返回（中断函数的内容全部由CubeMX自动生成，不需要进行修改。） */
    EXTI->IMR = it_save; /* 恢复中断寄存器设置 */
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

    LL_PWR_EnableUltraLowPower();             /* 进入低功耗模式后，关闭VREFINT */
    LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY); /* 设置DeepSleep为Standby模式 */
    LL_LPM_EnableDeepSleep();                 /* 准备进入Standby模式 */
    __WFI();                                  /* 进入Standby模式，等待唤醒引脚唤醒 */
}

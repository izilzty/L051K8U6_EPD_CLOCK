#include "func.h"

void Init(void)
{
    LL_EXTI_DisableIT_0_31(EPD_BUSY_EXTI0_EXTI_IRQn);                /* 禁用唤醒中断 */
    LL_SYSCFG_VREFINT_SetConnection(LL_SYSCFG_VREFINT_CONNECT_NONE); /* 禁用VREFINT输出 */
    LL_mDelay(199);

    while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
    {
        LL_mDelay(0);
    }

    switch (LP_GetResetInfo())
    {
    case LP_RESET_POWERON:
        USART_SendStringRN("POWER ON");
        LL_GPIO_ResetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin);
        LL_GPIO_SetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin);
        LL_mDelay(0);
        I2C_Start(0xD0, 2, 0);
        I2C_WriteByte(0x0E);
        I2C_WriteByte(0x1C);
        I2C_Stop();
        LL_GPIO_ResetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin);
        LL_GPIO_SetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin);
        USART_SendStringRN("DS1302 RESET DONE");
        break;
    case LP_RESET_NORMALRESET:
        USART_SendStringRN("RESET");
        break;
    case LP_RESET_WKUPSTANDBY:
        USART_SendStringRN("EXIT STANDBY");
        break;
    }
    USART_DebugPrint("In Standby");
    LP_EnterStandby();
}

void Loop(void)
{
    USART_SendStringRN("loop...");
    LL_mDelay(999);
}

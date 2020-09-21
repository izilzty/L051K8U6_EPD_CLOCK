#include "func.h"

#include <stdio.h>

uint8_t ResetInfo;
struct RTC_Time clock;
char str_buffer[61];

static void FirstInit(void) /* 系统首次上电、时钟数据丢失、按下了复位按键 初始化 */
{
    LL_mDelay(0);
    I2C_Start(0xD0, 2, 0);
    I2C_WriteByte(0x0E);
    I2C_WriteByte(0x1C);
    I2C_Stop();
}

static void UpdateDisplay(void) /* 系统由实时时钟从Standby模式唤醒 */
{
    LL_mDelay(0); /* 唤醒按键消抖 */
    while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
    {
        LL_mDelay(9); /* 唤醒按键消抖 */
    }
}

static void OpenMenu(void) /* 系统由“设置”按键从Standby模式唤醒 */
{
    LL_mDelay(0); /* 唤醒按键消抖 */
    while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
    {
        LL_mDelay(9); /* 唤醒按键消抖 */
    }
}

void Init(void) /* 系统复位后执行一次 */
{
    USART_DebugPrint("SYSTEM RESET");
    LL_EXTI_DisableIT_0_31(EPD_BUSY_EXTI0_EXTI_IRQn);                /* 禁用唤醒中断 */
    LL_SYSCFG_VREFINT_SetConnection(LL_SYSCFG_VREFINT_CONNECT_NONE); /* 禁用VREFINT输出 */
    ResetInfo = LP_GetResetInfo();
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        USART_DebugPrint("POR reset");
        break;
    case LP_RESET_NORMALRESET:
        USART_DebugPrint("Normal reset");
        break;
    case LP_RESET_WKUPSTANDBY:
        USART_DebugPrint("Wakeup from standby");
        break;
    }
    LL_GPIO_ResetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin); /* 打开SHT30电源 */
    LL_GPIO_SetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin);   /* 打开I2C上拉电阻 */
}

void Loop(void) /* 在Init()执行完成后循环执行 */
{
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        FirstInit();
        break;
    case LP_RESET_NORMALRESET:
        FirstInit();
        break;
    case LP_RESET_WKUPSTANDBY:
        UpdateDisplay();
        OpenMenu();
        break;
    }

    while (1)
    {
        RTC_ReadTime(&clock);
        snprintf(str_buffer, sizeof(str_buffer), "RTC: 2%03d %d %d %d %02d:%02d:%02d T:%02d.%02d", clock.Year, clock.Month, clock.Date, clock.Day, clock.Hours, clock.Minutes, clock.Seconds, (int8_t)RTC_GetTemp(), (uint8_t)((RTC_GetTemp() - (int8_t)RTC_GetTemp()) * 100));
        USART_SendStringRN(str_buffer);
        LL_mDelay(99);
    }

    USART_DebugPrint("Wait in standby mode");
    LP_EnterStandby(); /* 程序停止，等待下一次唤醒复位 */

    /* 正常情况下程序会停止在此处 */

    USART_DebugPrint("In standby mode fail");
    NVIC_SystemReset(); /* 未成功进入Standby模式，手动软复位 */
}

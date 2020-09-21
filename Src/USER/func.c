#include "func.h"

#include <stdio.h>

uint8_t ResetInfo;
struct RTC_Time clock;
struct RTC_Alarm alarm;
char str_buffer[61];

static void Menu_MainMenu(void) /* 主菜单 */
{
}

static void Menu_SetTime(void) /* 时间设置页面 */
{
}

static void FirstInit(void) /* 重新初始化全部数据 */
{
    USART_DebugPrint("BEGIN RESET ALL DATA");
    RTC_ResetAllRegToDefault();
    USART_DebugPrint("RTC reset done");
    BKPR_ResetALL();
    USART_DebugPrint("BKPR reset done");
    EEPROM_EraseRange(0, 510); /* VREFINT测量值不擦除 */
    USART_DebugPrint("EEPROM reset done");
    USART_DebugPrint("All done");
}

static void UpdateDisplay(void) /* 更新显示的时间和温度等数据 */
{
    LL_mDelay(9); /* 唤醒按键消抖 */
    while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
    {
        LL_mDelay(0); /* 唤醒按键消抖 */
    }
}

static void DumpRTCReg(void)
{
    uint8_t i, j, reg_tmp;
    char byte_str[9];
    USART_SendStringRN("== DS3231 REG ==");
    for (i = 0; i < 19; i++)
    {
        reg_tmp = RTC_ReadREG(RTC_REG_SEC + i);
        for (j = 0; j < 8; j++)
        {
            if ((reg_tmp & (0x80 >> j)) != 0)
            {
                byte_str[j] = '1';
            }
            else
            {
                byte_str[j] = '0';
            }
        }
        byte_str[8] = '\0';
        USART_SendString(" ");
        USART_SendString(byte_str);
        snprintf(byte_str, sizeof(byte_str), " 0x%02X", reg_tmp);
        USART_SendStringRN(byte_str);
    }
    USART_SendStringRN("================");
}

/* ==================== 主循环 ==================== */

void Init(void) /* 系统复位后首先进入此函数并执行一次 */
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
        if (RTC_GetOSF() != 0)
        {
            Menu_SetTime();
            RTC_ClearOSF();
        }
        break;
    case LP_RESET_NORMALRESET:
        if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
        {
            LL_mDelay(9);
            if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
            {
                FirstInit();
                Menu_SetTime();
                RTC_ClearOSF();
            }
        }
        break;
    case LP_RESET_WKUPSTANDBY:
        if (RTC_GetA2F() != 0)
        {
            RTC_ClearA2F();
        }
        else
        {
            LL_mDelay(9);
            while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
            {
                LL_mDelay(0);
            }
            Menu_MainMenu();
        }
        break;
    }

    UpdateDisplay();

    while (1)
    {
        RTC_GetTime(&clock);
        snprintf(str_buffer, sizeof(str_buffer), "RTC: 2%03d %d %d %d %02d:%02d:%02d T:%02d.%02d", clock.Year, clock.Month, clock.Date, clock.Day, clock.Hours, clock.Minutes, clock.Seconds, (int8_t)RTC_GetTemp(), (uint8_t)((RTC_GetTemp() - (int8_t)RTC_GetTemp()) * 100));
        USART_SendStringRN(str_buffer);
        LL_mDelay(99);
    }

    USART_DebugPrint("Ready in standby mode");
    LP_EnterStandby(); /* 程序停止，等待下一次唤醒复位 */

    /* 正常情况下程序会停止在此处 */

    USART_DebugPrint("In standby mode fail");
    NVIC_SystemReset(); /* 未成功进入Standby模式，执行软复位 */
}

/*

alarm.Seconds = 00;
    alarm.Minutes = 9;

    alarm.Hours = 9;
    alarm.Is_12hr = 0;
    alarm.PM = 0;

    //alarm.DY = 1;
    //alarm.Day = 5;

    alarm.DY = 1;
    alarm.Date = 6;

    RTC_ModifyAM2Mask(0xFF);
    RTC_SetAlarm2(&alarm);
    RTC_GetAlarm2(&alarm);
    snprintf(str_buffer, sizeof(str_buffer), "ALARM1:%02d:%02d:%02d PM:%d 12HR:%d DY:%d DAY:%d DATE:%02d", alarm.Hours, alarm.Minutes, alarm.Seconds, alarm.PM, alarm.Is_12hr, alarm.DY, alarm.Day, alarm.Date);
    USART_SendStringRN(str_buffer);

*/

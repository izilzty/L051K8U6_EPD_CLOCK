#include "func.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

const struct FUNC_Setting DefaultSetting = {0x00, 1, 3, 1.40, 1.20, 0.00, 0.00, 0, 0}; /* 设置未完成，蜂鸣器开关，蜂鸣器音量，警告电压，关机电压，温度传感器偏移，湿度传感器偏移，内置参考电压偏移，实时时钟老化偏移 */
const struct RTC_Time DefaultTime = {00, 0, 0, 4, 1, 10, 20, 0, 0};                    /* 2020年10月1日，星期4，12:00:00，Is_12hr = 0，PM = 0  */

static uint8_t ResetInfo;
static struct RTC_Time Time;
static struct Lunar_Date Lunar;
static struct TH_Value Sensor;
static struct FUNC_Setting Setting;
static char String[256];

static void Delay_100ns(volatile uint16_t nsX100);

static void UpdateHomeDisplay(void);
static void FullInit(void);
static void Menu_MainMenu(void);
static void Menu_DrawSubmenuFrame(char *title, uint8_t button_style);
static void Menu_DrawSubmenuSaveSelect(uint8_t select);
static void Menu_Guide(void);
static void Menu_SetTime(void);
static void Menu_SetBuzzer(void);
static void Menu_SetBattery(void);
static void Menu_SetSensor(void);
static void Menu_SetVrefint(void);
static void Menu_SetRTCAging(void);
static void Menu_Info(void);
static void Menu_ResetAll(void);
static void Menu_SetHWVer(void);
static void EPD_DrawBattery(uint16_t x, uint8_t y_x8, float max_voltage, float min_voltage, float voltage);

static void SaveSetting(const struct FUNC_Setting *setting);
static void ReadSetting(struct FUNC_Setting *setting);

static uint8_t BTN_ReadUP(void);
static uint8_t BTN_ReadDOWN(void);
static uint8_t BTN_ReadSET(void);
static void BTN_WaitSET(void);
static void BTN_WaitAll(void);
static uint8_t BTN_ModifySingleDigit(uint8_t *number, uint8_t modify_digit, uint8_t max_val, uint8_t min_val);

static void BEEP_Fast(void);
static void BEEP_Button(void);
static void BEEP_OK(void);

static void Power_EnableGDEH029A1(void);
static void Power_DisableGDEH029A1(void);
static void Power_Enable_SHT30_I2C(void);
static void Power_Disable_I2C_SHT30(void);
static uint8_t Power_EnableADC(void);
static uint8_t Power_DisableADC(void);
static void Power_EnableBUZZER(void);
static void Power_DisableBUZZER(void);
static void Power_DisableUSART(void);

static void DumpRTCReg(void);
static void DumpEEPROM(void);
static void DumpBKPR(void);

/**
 * @brief  延时100ns的倍数（不准确，只是大概）。
 * @param  nsX100 延时时间。
 */
static void Delay_100ns(volatile uint16_t nsX100)
{
    while (nsX100)
    {
        nsX100--;
    }
    ((void)nsX100);
}

/* ==================== 主函数 ==================== */

void Init(void) /* 系统复位后首先进入此函数并执行一次 */
{
#ifdef ENABLE_DEBUG_PRINT
    SERIAL_SendStringRN("\r\n\r\n***** SYSTEM RESET *****\r\n");
    LL_mDelay(199);
    LP_DisableDebug();
#else
    Power_DisableUSART();
#endif
    ResetInfo = LP_GetResetInfo();
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        SERIAL_DebugPrint("Power on reset");
        break;
    case LP_RESET_NORMALRESET:
        SERIAL_DebugPrint("Normal reset");
        break;
    case LP_RESET_WKUPSTANDBY:
        SERIAL_DebugPrint("Wakeup from standby");
        break;
    default:
        SERIAL_DebugPrint("Unknow reset");
        break;
    }

    Power_Enable_SHT30_I2C(); /* 默认打开SHT30和I2C电源 */
    Power_EnableADC();        /* 默认打开ADC电源 */
    Power_EnableBUZZER();     /* 默认打开蜂鸣器定时器 */
    Power_DisableGDEH029A1(); /* 默认关闭电子纸电源 */

    if (ResetInfo == LP_RESET_NORMALRESET && ((BTN_ReadUP() == 0 && BTN_ReadDOWN() == 0) || BKPR_ReadByte(BKPR_ADDR_BYTE_REQINIT) == REQUEST_RESET_ALL_FLAG))
    {
        FullInit();
    }

    /* 读取保存的设置，如果没有则使用默认设置代替 */
    ReadSetting(&Setting);
    /* 设置电池和传感器偏移量 */
    TH_SetTemperatureOffset(Setting.sensor_temp_offset);
    TH_SetHumidityOffset(Setting.sensor_rh_offset);
    ADC_SetVrefintOffset(Setting.vrefint_offset);
    if (RTC_GetAging() != Setting.rtc_aging_offset)
    {
        RTC_ModifyAging(Setting.rtc_aging_offset);
    }
}

void Loop(void) /* 在Init()执行完成后循环执行 */
{
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
    case LP_RESET_NORMALRESET:
        BKPR_ResetAll();
        if (RTC_GetOSF() != 0 || Setting.available != SETTING_AVALIABLE_FLAG)
        {
            Power_EnableGDEH029A1();
            if (EEPROM_ReadDWORD(EEPROM_ADDR_DWORD_HWVERSION) == 0x00000000)
            {
                Menu_SetHWVer();
            }
            Menu_Guide();
            Menu_SetTime();
            Setting.available = SETTING_AVALIABLE_FLAG;
            SaveSetting(&Setting);
        }
        break;
    case LP_RESET_WKUPSTANDBY:
        if (RTC_GetA2F() != 0 || (BTN_ReadUP() != 0 && BTN_ReadDOWN() == 0)) /* 同时按下“菜单”和“上”按钮立刻更新显示 */
        {
            RTC_ClearA2F();
        }
        else
        {
            Power_EnableGDEH029A1();
            Menu_MainMenu();
        }
        break;
    }

    Power_EnableGDEH029A1();
    UpdateHomeDisplay();

    Power_DisableGDEH029A1();
    Power_Disable_I2C_SHT30();
    Power_DisableADC();
    Power_DisableBUZZER();

    BTN_WaitSET(); /* 等待设置按钮释放 */

    SERIAL_DebugPrint("Ready to enter standby mode");
    LP_EnterStandby(); /* 程序停止，等待下一次唤醒复位 */

    /* 正常情况下进入Standby模式后，程序会停止在此处，直到下次复位或唤醒再重头开始执行 */

    SERIAL_DebugPrint("Enter standby mode fail");
    LL_mDelay(999); /* 等待1000ms */
    SERIAL_DebugPrint("Try to enter standby mode again");
    LP_EnterStandby(); /* 再次尝试进入Standby模式 */
    SERIAL_DebugPrint("Enter standby mode fail");
    SERIAL_DebugPrint("Try to reset the system");
    NVIC_SystemReset(); /* 两次未成功进入Standby模式，执行软复位 */
}

/* ==================== 主要功能 ==================== */

static void UpdateHomeDisplay(void) /* 更新显示时间和温度等数据 */
{
    uint32_t battery_val_stor;
    float battery_value, sensor_tmp;
    int8_t temp_value[2], rh_value[2];

    RTC_GetTime(&Time); /* 获取当前时间 */

    RTC_ModifyAM2Mask(0x07); /* 设置闹钟2每分钟产生中断 */
    RTC_ModifyA2IE(1);       /* 打开闹钟2 */
    RTC_ClearA2F();          /* 清除闹钟2中断标志 */
    RTC_ModifyA1IE(0);       /* 关闭闹钟1 */
    RTC_ClearA1F();          /* 清除闹钟1中断标志 */
    RTC_ModifyINTCN(1);      /* 打开中断输出 */

    TH_GetValue_SingleShotWithCS(TH_ACC_HIGH, &Sensor); /* 获取当前温度 */

    EPD_Init(EPD_UPDATE_MODE_FAST);
    EPD_ClearRAM();

    battery_val_stor = BKPR_ReadDWORD(BKPR_ADDR_DWORD_ADCVAL); /* 读取上次屏幕刷新完成后的电量 */
    battery_value = *(float *)&battery_val_stor;
    if (battery_value < 0.1 || battery_value > 3.6) /* 超出此范围则判断为备份寄存器数据失效，重新读取当前电池数据 */
    {
        battery_value = ADC_GetChannel(ADC_CHANNEL_BATTERY);
    }
    if (battery_value < Setting.battery_stop) /* 电池已经低于最低工作电压，显示电量不足标志并停止更新 */
    {
        if (RTC_ReadREG(RTC_REG_AL1_DDT) != 0xAA) /* 借用RTC未使用的寄存器，存储低电量画面已显示标志 */
        {
            EPD_DrawImage(0, 0, EPD_Image_BatteryLow_296x128);
            EPD_Show(0);
            LP_EnterStop(EPD_TIMEOUT_MS);
            EPD_EnterDeepSleep();
            RTC_WriteREG(RTC_REG_AL1_DDT, 0xAA);
            SERIAL_DebugPrint("Lowpower display update done");
        }
        else
        {
            SERIAL_DebugPrint("Lowpower display already updated");
        }

        RTC_ModifyA2IE(0);
        RTC_ClearA2F();

        Power_DisableGDEH029A1();
        Power_DisableADC();
        Power_DisableBUZZER();
        Power_Disable_I2C_SHT30();

        while (1)
        {
            SERIAL_DebugPrint("Battery low, lock display update");
            LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);
            LP_DelayStop(50);
            LL_GPIO_SetOutputPin(LED_GPIO_Port, LED_Pin);
            LP_DelayStop(5000);
        }
    }
    RTC_WriteREG(RTC_REG_AL1_DDT, 0x00); /* 清除低电量画面已显示标志 */

    LUNAR_SolarToLunar(&Lunar, Time.Year + 2000, Time.Month, Time.Date); /* RTC读出的年份省去了2000，计算农历前要手动加上 */

    /* 将浮点温度分为两个整数温度 */
    if (Sensor.CEL < 0)
    {
        sensor_tmp = Sensor.CEL - 0.05;
    }
    else
    {
        sensor_tmp = Sensor.CEL + 0.05;
    }
    temp_value[0] = (int8_t)sensor_tmp;
    temp_value[1] = (int8_t)((sensor_tmp - temp_value[0]) * 10);

    /* 将浮点湿度分为两个整数 */
    sensor_tmp = Sensor.RH + 0.05;
    rh_value[0] = (int8_t)sensor_tmp;
    rh_value[1] = (int8_t)((sensor_tmp - rh_value[0]) * 10);

    EPD_DrawHLine(0, 28, 296, 2);
    EPD_DrawHLine(0, 104, 296, 2);
    EPD_DrawHLine(213, 67, 76, 2);
    EPD_DrawVLine(202, 39, 56, 2);
    EPD_DrawBattery(263, 0, BATT_MAX_VOLTAGE, Setting.battery_warn, battery_value);

    snprintf(String, sizeof(String), "2%03d/%02d/%02d 星期%s", Time.Year, Time.Month, Time.Date, Lunar_DayString[Time.Day]);
    EPD_DrawUTF8(0, 0, 1, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
    if (Time.Is_12hr != 0)
    {
        if (Time.PM != 0)
        {
            EPD_DrawUTF8(0, 9, 2, "PM", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
        }
        else
        {
            EPD_DrawUTF8(0, 5, 2, "AM", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
        }
    }
    snprintf(String, sizeof(String), "%02d:%02d", Time.Hours, Time.Minutes);
    if (Time.Is_12hr != 0)
    {
        EPD_DrawUTF8(34, 5, 6, String, EPD_FontAscii_27x56, EPD_FontUTF8_24x24_B);
    }
    else
    {
        EPD_DrawUTF8(22, 5, 6, String, EPD_FontAscii_27x56, EPD_FontUTF8_24x24_B);
    }
    if (temp_value[0] < -9)
    {
        snprintf(String, sizeof(String), "%02d ℃", temp_value[0]);
    }
    else if (temp_value[0] < 100)
    {
        snprintf(String, sizeof(String), "%02d.%d℃", temp_value[0], temp_value[1]);
    }
    else
    {
        snprintf(String, sizeof(String), "%03d ℃", temp_value[0]);
    }
    EPD_DrawUTF8(213, 5, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
    if (rh_value[0] < 100)
    {
        snprintf(String, sizeof(String), "%02d.%d％", rh_value[0], rh_value[1]);
    }
    else
    {
        snprintf(String, sizeof(String), "%03d ％", rh_value[0]);
    }
    EPD_DrawUTF8(213, 9, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
    snprintf(String, sizeof(String), "农历：%s%s%s", Lunar_MonthLeapString[Lunar.IsLeap], Lunar_MonthString[Lunar.Month], Lunar_DateString[Lunar.Date]);
    EPD_DrawUTF8(0, 14, 2, String, NULL, EPD_FontUTF8_16x16_B);

    EPD_Show(0);
    LP_EnterStop(EPD_TIMEOUT_MS);

    battery_value = ADC_GetChannel(ADC_CHANNEL_BATTERY);
    BKPR_WriteDWORD(BKPR_ADDR_DWORD_ADCVAL, *(uint32_t *)&battery_value);

    EPD_EnterDeepSleep();
}

static void FullInit(void) /* 重新初始化全部数据 */
{
    BUZZER_SetFrqe(4000);
    BUZZER_SetVolume(DefaultSetting.buzzer_volume);

    BUZZER_Beep(49);
    LL_mDelay(49);
    BUZZER_Beep(49);
    LL_mDelay(49);
    BUZZER_Beep(49);

    SERIAL_DebugPrint("BEGIN RESET ALL DATA");
    SERIAL_DebugPrint("RTC reset...");
    if (RTC_ResetAllRegToDefault() != 0)
    {
        SERIAL_DebugPrint("RTC reset fail");
    }
    else
    {
        SERIAL_DebugPrint("RTC reset done");
    }
    SERIAL_DebugPrint("SENSOR reset...");
    if (TH_SoftReset() != 0)
    {
        SERIAL_DebugPrint("SENSOR reset fail");
    }
    else
    {
        SERIAL_DebugPrint("SENSOR reset done");
    }
    SERIAL_DebugPrint("BKPR reset...");
    if (BKPR_ResetAll() != 0)
    {
        SERIAL_DebugPrint("BKPR reset fail");
    }
    else
    {
        SERIAL_DebugPrint("BKPR reset done");
    }
    SERIAL_DebugPrint("EEPROM reset...");
    if (EEPROM_EraseRange(0, 510) != 0)
    {
        SERIAL_DebugPrint("EEPROM reset fail");
    }
    else
    {
        SERIAL_DebugPrint("EEPROM reset done");
    }
    SERIAL_DebugPrint("Process done");

#ifdef ENABLE_DEBUG_PRINT
    DumpEEPROM();
    DumpRTCReg();
    DumpBKPR();
#endif

    BUZZER_Beep(499);
}

/* ==================== 主菜单 ==================== */

static void Menu_MainMenu(void)
{
    uint8_t i, select, exit, full_update, wait_btn, update_display;

    BEEP_OK();

    exit = 0;
    full_update = 1;
    select = 0;
    wait_btn = 0;
    while (exit == 0)
    {
        if (full_update == 0)
        {
            if (BTN_ReadDOWN() == 0)
            {
                if (select < 9)
                {
                    select += 1;
                }
                else
                {
                    select = 0;
                }
                wait_btn = 1;
            }
            else if (BTN_ReadUP() == 0)
            {
                if (select > 0)
                {
                    select -= 1;
                }
                else
                {
                    select = 9;
                }
                wait_btn = 1;
            }
            else if (BTN_ReadSET() == 0)
            {
                BEEP_OK();
                switch (select)
                {
                case 0:
                    exit = 1;
                    update_display = 0;
                    wait_btn = 0;
                    break;
                case 1:
                    Menu_SetTime();
                    break;
                case 2:
                    Menu_SetBuzzer();
                    break;
                case 3:
                    Menu_SetBattery();
                    break;
                case 4:
                    Menu_SetSensor();
                    break;
                case 5:
                    Menu_SetVrefint();
                    break;
                case 6:
                    Menu_SetRTCAging();
                    break;
                case 7:
                    Menu_Info();
                    break;
                case 8:
                    Menu_ResetAll();
                    break;
                case 9:
                    EPD_Init(EPD_UPDATE_MODE_FULL);
                    EPD_ClearRAM();
                    EPD_Show(0);
                    LP_EnterStop(EPD_TIMEOUT_MS);
                    LP_DelayStop(1000);
                    EPD_ClearArea(0, 0, 296, 16, 0x00);
                    EPD_Show(0);
                    LP_EnterStop(EPD_TIMEOUT_MS);
                    LP_DelayStop(1000);
                    EPD_ClearRAM();
                    EPD_Show(0);
                    LP_EnterStop(EPD_TIMEOUT_MS);
                    LP_DelayStop(1000);
                    BEEP_OK();
                    break;
                case 10:
                    /* code */
                    break;
                case 11:
                    /* code */
                    break;
                }
                full_update = 1;
            }
            if (wait_btn != 0)
            {
                update_display = 1;
            }
            if (update_display != 0)
            {
                if (EPD_CheckBusy() == 0)
                {
                    update_display = 0;
                    EPD_ClearArea(0, 4, 24, 12, 0xFF);
                    snprintf(String, sizeof(String), "▶");
                    EPD_DrawUTF8(0, 4 + ((select % 4) * 3), 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    snprintf(String, sizeof(String), "%d/%d页", (select / 4) + 1, (8 / 4) + 1);
                    EPD_DrawUTF8(236, 13, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    if (select <= 3)
                    {
                        snprintf(String, sizeof(String), "1.返回        ");
                        EPD_DrawUTF8(25, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "2.时间设置    ");
                        EPD_DrawUTF8(25, 7, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "3.铃声设置    ");
                        EPD_DrawUTF8(25, 10, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "4.电池设置    ");
                        EPD_DrawUTF8(25, 13, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    }
                    else if (select >= 4 && select <= 7)
                    {
                        snprintf(String, sizeof(String), "5.传感器设置  ");
                        EPD_DrawUTF8(25, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "6.参考电压设置");
                        EPD_DrawUTF8(25, 7, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "7.时钟老化设置");
                        EPD_DrawUTF8(25, 10, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "8.系统信息    ");
                        EPD_DrawUTF8(25, 13, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    }
                    else if (select >= 8 && select <= 11)
                    {
                        snprintf(String, sizeof(String), "9.恢复默认设置");
                        EPD_DrawUTF8(25, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "10.清除屏幕   ");
                        EPD_DrawUTF8(25, 7, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "              ");
                        EPD_DrawUTF8(25, 10, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                        snprintf(String, sizeof(String), "              ");
                        EPD_DrawUTF8(25, 13, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    }
                    EPD_Show(0);
                }
            }
            if (wait_btn != 0)
            {
                BEEP_Button();
                BTN_WaitAll();
                wait_btn = 0;
            }
        }
        else
        {
            full_update = 0;
            update_display = 1;
            EPD_Init(EPD_UPDATE_MODE_FAST);
            EPD_ClearRAM();
            for (i = 0; i < 2; i++)
            {
                EPD_DrawUTF8(0, 0, 0, "主菜单", NULL, EPD_FontUTF8_24x24_B);
                EPD_DrawImage(161, 0, EPD_Image_Arrow_8x8);
                EPD_DrawImage(209, 0, EPD_Image_Arrow_8x8);
                EPD_DrawImage(257, 0, EPD_Image_Arrow_8x8);
                EPD_DrawUTF8(149, 1, 0, "进入", NULL, EPD_FontUTF8_16x16_B);
                EPD_DrawUTF8(205, 1, 0, "上", NULL, EPD_FontUTF8_16x16_B);
                EPD_DrawUTF8(253, 1, 0, "下", NULL, EPD_FontUTF8_16x16_B);
                EPD_DrawHLine(0, 27, 296, 2);
                if (i == 0)
                {
                    EPD_Show(0);
                    LP_EnterStop(EPD_TIMEOUT_MS);
                    EPD_Init(EPD_UPDATE_MODE_PART);
                    EPD_ClearRAM();
                }
            }
            BTN_WaitAll();
        }
    }
}

/* ==================== 子菜单 ==================== */

static void Menu_DrawSubmenuFrame(char *title, uint8_t button_style)
{
    uint8_t i;

    EPD_Init(EPD_UPDATE_MODE_FAST);
    EPD_ClearRAM();
    for (i = 0; i < 2; i++)
    {
        EPD_DrawUTF8(0, 0, 0, title, NULL, EPD_FontUTF8_24x24_B);
        EPD_DrawImage(161, 0, EPD_Image_Arrow_8x8);
        EPD_DrawImage(209, 0, EPD_Image_Arrow_8x8);
        EPD_DrawImage(257, 0, EPD_Image_Arrow_8x8);
        switch (button_style)
        {
        case 0:
            EPD_DrawUTF8(149, 1, 0, "移动", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(205, 1, 0, "加", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(253, 1, 0, "减", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(211, 13, 0, "保存", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(258, 13, 0, "取消", NULL, EPD_FontUTF8_16x16_B);
            break;
        case 1:
            EPD_DrawUTF8(149, 1, 0, "移动", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(197, 1, 0, "选择", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(253, 1, 0, "空", NULL, EPD_FontUTF8_16x16_B);
            break;
        case 2:
            EPD_DrawUTF8(149, 1, 0, "继续", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(205, 1, 0, "空", NULL, EPD_FontUTF8_16x16_B);
            EPD_DrawUTF8(253, 1, 0, "空", NULL, EPD_FontUTF8_16x16_B);
            break;
        }
        EPD_DrawHLine(0, 27, 296, 2);
        if (i == 0)
        {
            EPD_Show(0);
            LP_EnterStop(EPD_TIMEOUT_MS);
            EPD_Init(EPD_UPDATE_MODE_PART);
            EPD_ClearRAM();
        }
    }
}

static void Menu_DrawSubmenuSaveSelect(uint8_t select)
{
    EPD_ClearArea(211, 15, 79, 1, 0xFF);
    switch (select)
    {
    case 1:
        EPD_DrawImage(221, 15, EPD_Image_Arrow_12x8);
        break;
    case 2:
        EPD_DrawImage(268, 15, EPD_Image_Arrow_12x8);
        break;
    }
    if (select != 0)
    {
        EPD_DrawUTF8(197, 1, 0, "选择", NULL, EPD_FontUTF8_16x16_B);
        EPD_DrawUTF8(253, 1, 0, "空", NULL, EPD_FontUTF8_16x16_B);
    }
    else
    {
        EPD_DrawUTF8(197, 1, 0, "    ", NULL, EPD_FontUTF8_16x16_B);
        EPD_DrawUTF8(205, 1, 0, "加  ", NULL, EPD_FontUTF8_16x16_B);
        EPD_DrawUTF8(253, 1, 0, "减", NULL, EPD_FontUTF8_16x16_B);
    }
}

static void Menu_SetTime(void) /* 时间设置页面 */
{
    struct RTC_Time new_time;
    uint8_t select, save, update_display, wait_btn, time_check, arrow_y;
    uint16_t arrow_x;

    Menu_DrawSubmenuFrame("时间设置", 0);
    BTN_WaitAll();

    RTC_GetTime(&new_time);
    if (RTC_GetOSF() != 0 || new_time.Month == 0)
    {
        memcpy(&new_time, &DefaultTime, sizeof(struct RTC_Time));
    }

    select = 0;
    update_display = 1;
    time_check = 0;
    wait_btn = 0;
    save = 0;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 17)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            if (select == 9 && new_time.Is_12hr == 0)
            {
                select += 1;
            }
            if (time_check != 0)
            {
                RTC_CheckTimeRange(&new_time);
                time_check = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                wait_btn = BTN_ModifySingleDigit(&new_time.Year, 2, 1, 0);
                break;
            case 1:
                wait_btn = BTN_ModifySingleDigit(&new_time.Year, 1, 9, 0);
                break;
            case 2:
                wait_btn = BTN_ModifySingleDigit(&new_time.Year, 0, 9, 0);
                time_check = 1;
                break;
            case 3:
                wait_btn = BTN_ModifySingleDigit(&new_time.Month, 1, 1, 0);
                break;
            case 4:
                wait_btn = BTN_ModifySingleDigit(&new_time.Month, 0, 9, 0);
                time_check = 1;
                break;
            case 5:
                wait_btn = BTN_ModifySingleDigit(&new_time.Date, 1, 3, 0);
                break;
            case 6:
                wait_btn = BTN_ModifySingleDigit(&new_time.Date, 0, 9, 0);
                time_check = 1;
                break;
            case 7:
                wait_btn = BTN_ModifySingleDigit(&new_time.Day, 0, 7, 1);
                break;
            case 8:
                wait_btn = BTN_ModifySingleDigit(&new_time.Is_12hr, 0, 1, 0);
                time_check = 1;
                break;
            case 9:
                wait_btn = BTN_ModifySingleDigit(&new_time.PM, 0, 1, 0);
                break;
            case 10:
                if (new_time.Is_12hr == 0)
                {
                    wait_btn = BTN_ModifySingleDigit(&new_time.Hours, 1, 2, 0);
                }
                else
                {
                    wait_btn = BTN_ModifySingleDigit(&new_time.Hours, 1, 1, 0);
                }
                break;
            case 11:
                wait_btn = BTN_ModifySingleDigit(&new_time.Hours, 0, 9, 0);
                time_check = 1;
                break;
            case 12:
                wait_btn = BTN_ModifySingleDigit(&new_time.Minutes, 1, 5, 0);
                break;
            case 13:
                wait_btn = BTN_ModifySingleDigit(&new_time.Minutes, 0, 9, 0);
                time_check = 1;
                break;
            case 14:
                wait_btn = BTN_ModifySingleDigit(&new_time.Seconds, 1, 5, 0);
                break;
            case 15:
                wait_btn = BTN_ModifySingleDigit(&new_time.Seconds, 0, 9, 0);
                time_check = 1;
                break;
            case 16:
                if (BTN_ReadUP() == 0)
                {
                    save = 2;
                    wait_btn = 0;
                    update_display = 0;
                }
                break;
            case 17:
                if (BTN_ReadUP() == 0)
                {
                    save = 1;
                    wait_btn = 0;
                    update_display = 0;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                snprintf(String, sizeof(String), "2%03d年%02d月%02d日 周%d", new_time.Year, new_time.Month, new_time.Date, new_time.Day);
                EPD_DrawUTF8(7, 4, 5, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                if (new_time.Is_12hr != 0)
                {
                    snprintf(String, sizeof(String), "时间格式：12小时制");
                }
                else
                {
                    snprintf(String, sizeof(String), "时间格式：24小时制");
                }
                EPD_DrawUTF8(5, 8, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                if (new_time.Is_12hr != 0)
                {
                    if (new_time.PM != 0)
                    {
                        snprintf(String, sizeof(String), "下午");
                    }
                    else
                    {
                        snprintf(String, sizeof(String), "上午");
                    }
                }
                else
                {
                    snprintf(String, sizeof(String), "    ");
                }
                EPD_DrawUTF8(5, 12, 0, String, NULL, EPD_FontUTF8_24x24_B);

                snprintf(String, sizeof(String), "%02d:%02d:%02d", new_time.Hours, new_time.Minutes, new_time.Seconds);
                EPD_DrawUTF8(58, 12, 5, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                if (select <= 2)
                {
                    arrow_x = 24 + (select * 17);
                    arrow_y = 7;
                }
                else if (select >= 3 && select <= 4)
                {
                    arrow_x = 104 + ((select - 3) * 17);
                    arrow_y = 7;
                }
                else if (select >= 5 && select <= 6)
                {
                    arrow_x = 167 + ((select - 5) * 17);
                    arrow_y = 7;
                }
                else if (select >= 7 && select <= 7)
                {
                    arrow_x = 276 + ((select - 7) * 17);
                    arrow_y = 7;
                }
                else if (select >= 8 && select <= 8)
                {
                    arrow_x = 132;
                    arrow_y = 11;
                }
                else if (select >= 9 && select <= 9)
                {
                    arrow_x = 23;
                    arrow_y = 15;
                }
                else if (select >= 10 && select <= 11)
                {
                    arrow_x = 58 + ((select - 10) * 17);
                    arrow_y = 15;
                }
                else if (select >= 12 && select <= 13)
                {
                    arrow_x = 109 + ((select - 12) * 17);
                    arrow_y = 15;
                }
                else if (select >= 14 && select <= 15)
                {
                    arrow_x = 160 + ((select - 14) * 17);
                    arrow_y = 15;
                }
                EPD_ClearArea(5, 7, 283, 1, 0xFF);
                EPD_ClearArea(5, 11, 283, 1, 0xFF);
                EPD_ClearArea(5, 15, 283, 1, 0xFF);

                if (select == 16 || select == 17)
                {
                    Menu_DrawSubmenuSaveSelect(select - 16 + 1);
                }
                else
                {
                    EPD_DrawImage(arrow_x, arrow_y, EPD_Image_Arrow_12x8);
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            RTC_SetTime(&new_time);
        }
        if (wait_btn != 0)
        {
            BEEP_Button();
            BTN_WaitAll();
            wait_btn = 0;
        }
    }
    BEEP_OK();
}

static void Menu_Guide(void) /* 首次使用时的引导 */
{
    Menu_DrawSubmenuFrame("欢迎使用", 2);
    BTN_WaitAll();

    EPD_DrawImage(0, 4, EPD_Image_Welcome_296x96);
    EPD_Show(0);
    LP_EnterStop(EPD_TIMEOUT_MS);

    while (BTN_ReadSET() != 0)
    {
    }

    BEEP_OK();
}

static void Menu_SetBuzzer(void) /* 设置蜂鸣器状态 */
{
    uint8_t select, save, update_display, wait_btn, volume, enable;

    Menu_DrawSubmenuFrame("铃声设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    enable = Setting.buzzer_enable;
    volume = Setting.buzzer_volume;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 3)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0 || BTN_ReadDOWN() == 0)
                {
                    if (Setting.buzzer_enable == 0)
                    {
                        Setting.buzzer_enable = 1;
                    }
                    else
                    {
                        Setting.buzzer_enable = 0;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    if (Setting.buzzer_volume < BUZZER_MAX_VOL)
                    {
                        Setting.buzzer_volume += 1;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if (Setting.buzzer_volume > 1)
                    {
                        Setting.buzzer_volume -= 1;
                    }
                    wait_btn = 1;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 3:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                EPD_DrawUTF8(0, 4, 0, "蜂鸣器开启状态：", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                if (Setting.buzzer_enable != 0)
                {
                    EPD_DrawUTF8(192, 4, 0, "开启", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                }
                else
                {
                    EPD_DrawUTF8(192, 4, 0, "关闭", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                }

                snprintf(String, sizeof(String), "蜂鸣器音量：%02d/%02d", Setting.buzzer_volume, BUZZER_MAX_VOL);
                EPD_DrawUTF8(0, 8, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(240, 4, 24, 3, 0xFF);
                EPD_ClearArea(204, 8, 24, 3, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawUTF8(240, 4, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                case 1:
                    EPD_DrawUTF8(204, 8, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                }

                if (select == 2 || select == 3)
                {
                    Menu_DrawSubmenuSaveSelect(select - 2 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            SaveSetting(&Setting);
        }
        else if (save == 1)
        {
            Setting.buzzer_enable = enable;
            Setting.buzzer_volume = volume;
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            BEEP_Button();
            BTN_WaitAll();
        }
    }
    BEEP_OK();
}

static void Menu_SetBattery(void) /* 设置电池信息 */
{
    uint8_t select, save, update_display, wait_btn, long_press;
    float bat_warn, bat_stop, tmp;

    Menu_DrawSubmenuFrame("电池设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    long_press = 0;
    bat_warn = Setting.battery_warn;
    bat_stop = Setting.battery_stop;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 3)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            long_press = 255;
            wait_btn = 1;
        }
        else
        {
            if (BTN_ReadSET() != 0 && BTN_ReadUP() != 0 && BTN_ReadDOWN() != 0)
            {
                long_press = 6;
            }
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    if ((bat_warn + 0.005) < BATT_MAX_VOLTAGE)
                    {
                        bat_warn += 0.01;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if ((bat_warn - 0.005) > DCDC_MIN_VOLTAGE)
                    {
                        bat_warn -= 0.01;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    if ((bat_stop + 0.005) < BATT_MAX_VOLTAGE)
                    {
                        bat_stop += 0.01;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if ((bat_stop - 0.005) > DCDC_MIN_VOLTAGE)
                    {
                        bat_stop -= 0.01;
                    }
                    wait_btn = 1;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 3:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                tmp = bat_warn + 0.005;
                snprintf(String, sizeof(String), "警告电压：%d.%02dV", (int8_t)tmp, (uint16_t)((tmp - (int8_t)tmp) * 100));
                EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                tmp = bat_stop + 0.005;
                snprintf(String, sizeof(String), "截止电压：%d.%02dV", (int8_t)(tmp), (uint16_t)(((tmp) - (int8_t)(tmp)) * 100));
                EPD_DrawUTF8(0, 8, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                tmp = ADC_GetChannel(ADC_CHANNEL_BATTERY) + 0.005;
                snprintf(String, sizeof(String), "[实时电压：%d.%02dV]", (int8_t)(tmp), (uint16_t)(((tmp) - (int8_t)(tmp)) * 100));
                EPD_DrawUTF8(0, 12, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(180, 4, 24, 3, 0xFF);
                EPD_ClearArea(180, 8, 24, 3, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawUTF8(180, 4, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                case 1:
                    EPD_DrawUTF8(180, 8, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                }

                if (select == 2 || select == 3)
                {
                    Menu_DrawSubmenuSaveSelect(select - 2 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            Setting.battery_warn = bat_warn;
            Setting.battery_stop = bat_stop;
            SaveSetting(&Setting);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            if (long_press != 0)
            {
                BEEP_Button();
            }
            else
            {
                BEEP_Fast();
            }
            while (long_press != 0 && (BTN_ReadDOWN() == 0 || BTN_ReadUP() == 0 || BTN_ReadSET() == 0))
            {
                LL_mDelay(0);
                if (long_press != 255)
                {
                    long_press -= 1;
                }
            }
        }
    }
    BEEP_OK();
}

static void Menu_SetSensor(void) /* 设置传感器信息 */
{
    uint8_t select, save, update_display, wait_btn, long_press;
    float temp_offset, rh_offset, tmp;

    Menu_DrawSubmenuFrame("传感器设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    long_press = 0;
    temp_offset = Setting.sensor_temp_offset;
    rh_offset = Setting.sensor_rh_offset;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 3)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
            long_press = 255;
        }
        else
        {
            if (BTN_ReadSET() != 0 && BTN_ReadUP() != 0 && BTN_ReadDOWN() != 0)
            {
                long_press = 6;
            }
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    if ((temp_offset + 0.005) < 10.00)
                    {
                        temp_offset += 0.01;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if ((temp_offset - 0.005) > -10.00)
                    {
                        temp_offset -= 0.01;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    if ((rh_offset + 0.005) < 10.00)
                    {
                        rh_offset += 0.01;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if ((rh_offset - 0.005) > -10.00)
                    {
                        rh_offset -= 0.01;
                    }
                    wait_btn = 1;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 3:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                if (temp_offset > 0)
                {
                    tmp = temp_offset + 0.005;
                }
                else
                {
                    tmp = temp_offset - 0.005;
                }
                if (tmp > -1.0 && tmp <= -0.01)
                {
                    snprintf(String, sizeof(String), "温度偏移：-%02d.%02d℃", (int8_t)tmp, abs((int16_t)((tmp - (int8_t)tmp) * 100)));
                }
                else
                {
                    snprintf(String, sizeof(String), "温度偏移：%+03d.%02d℃", (int8_t)tmp, abs((int16_t)((tmp - (int8_t)tmp) * 100)));
                }
                EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                if (rh_offset > 0)
                {
                    tmp = rh_offset + 0.005;
                }
                else
                {
                    tmp = rh_offset - 0.005;
                }
                if (tmp >= -1.0 && tmp <= -0.01)
                {
                    snprintf(String, sizeof(String), "湿度偏移：-%02d.%02d％", (int8_t)tmp, abs((int16_t)((tmp - (int8_t)tmp) * 100)));
                }
                else
                {
                    snprintf(String, sizeof(String), "湿度偏移：%+03d.%02d％", (int8_t)tmp, abs((int16_t)((tmp - (int8_t)tmp) * 100)));
                }
                EPD_DrawUTF8(0, 8, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(216, 4, 24, 3, 0xFF);
                EPD_ClearArea(216, 8, 24, 3, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawUTF8(216, 4, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                case 1:
                    EPD_DrawUTF8(216, 8, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                }

                if (select == 2 || select == 3)
                {
                    Menu_DrawSubmenuSaveSelect(select - 2 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            Setting.sensor_temp_offset = temp_offset;
            Setting.sensor_rh_offset = rh_offset;
            TH_SetTemperatureOffset(temp_offset);
            TH_SetHumidityOffset(rh_offset);
            SaveSetting(&Setting);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            if (long_press != 0)
            {
                BEEP_Button();
            }
            else
            {
                BEEP_Fast();
            }
            while (long_press != 0 && (BTN_ReadDOWN() == 0 || BTN_ReadUP() == 0 || BTN_ReadSET() == 0))
            {
                LL_mDelay(0);
                if (long_press != 255)
                {
                    long_press -= 1;
                }
            }
        }
    }
    BEEP_OK();
}

static void Menu_SetVrefint(void) /* 设置参考电压偏移 */
{
    uint8_t select, save, update_display, wait_btn;
    int16_t offset;
    float vrefint_factory;

    Menu_DrawSubmenuFrame("参考电压设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    offset = Setting.vrefint_offset;
    ADC_EnableVrefintOutput();
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 2)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    if (offset < 127)
                    {
                        offset += 1;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if (offset > -127)
                    {
                        offset -= 1;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                snprintf(String, sizeof(String), "偏移数值：%+04d", offset);
                EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                vrefint_factory = ADC_GetVrefintFactory() + (ADC_GetVrefintStep() * offset) + 0.0005;
                snprintf(String, sizeof(String), "[实际电压：%04d.%03dmV]", (int16_t)vrefint_factory, (int16_t)((vrefint_factory - (int16_t)vrefint_factory) * 1000));
                EPD_DrawUTF8(0, 8, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(168, 4, 24, 3, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawUTF8(168, 4, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                }

                if (select == 1 || select == 2)
                {
                    Menu_DrawSubmenuSaveSelect(select - 1 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            Setting.vrefint_offset = offset;
            ADC_SetVrefintOffset(offset);
            SaveSetting(&Setting);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            BEEP_Button();
            BTN_WaitAll();
        }
    }
    ADC_DisableVrefintOutput();
    BEEP_OK();
}

static void Menu_Info(void) /* 系统信息 */
{
    uint32_t eeprom_tmp;

    Menu_DrawSubmenuFrame("系统信息", 2);
    BTN_WaitAll();

    eeprom_tmp = EEPROM_ReadDWORD(EEPROM_ADDR_DWORD_HWVERSION) & 0x00FFFFFF;

    snprintf(String, sizeof(String), "软件版本  : %s", SOFT_VERSION);
    EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    snprintf(String, sizeof(String), "编译时间  : %s", __DATE__);
    EPD_DrawUTF8(0, 6, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    snprintf(String, sizeof(String), "硬件版本  : %s", (char *)&eeprom_tmp);
    EPD_DrawUTF8(0, 8, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    snprintf(String, sizeof(String), "MCU信息   : 0x%03X, 0x%04X", LL_DBGMCU_GetDeviceID(), LL_DBGMCU_GetRevisionID());
    EPD_DrawUTF8(0, 10, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    snprintf(String, sizeof(String), "SHT30状态 : 0x%02X", TH_GetStatus());
    EPD_DrawUTF8(0, 12, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    snprintf(String, sizeof(String), "DS3231状态: 0x%02X, 0x%02X, 0x%02X", RTC_ReadREG(RTC_REG_CTL), RTC_ReadREG(RTC_REG_STA), RTC_ReadREG(RTC_REG_AGI));
    EPD_DrawUTF8(0, 14, 0, String, EPD_FontAscii_8x16, EPD_FontUTF8_16x16);
    EPD_Show(0);
    LP_EnterStop(EPD_TIMEOUT_MS);

    while (BTN_ReadSET() != 0)
    {
        LP_DelayStop(50);
    }

    BEEP_OK();
}

static void Menu_SetRTCAging(void) /* 设置实时时钟老化偏移 */
{
    uint8_t select, save, update_display, wait_btn;
    int8_t offset;

    Menu_DrawSubmenuFrame("RTC老化设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    offset = Setting.rtc_aging_offset;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 2)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    if (offset < 127)
                    {
                        offset += 1;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if (offset > -127)
                    {
                        offset -= 1;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                snprintf(String, sizeof(String), "偏移数值：%+04d", offset);
                EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                EPD_DrawUTF8(0, 8, 1, "[每个偏移约为0.1ppm]", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(168, 4, 24, 3, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawUTF8(168, 4, 0, "◀", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                    break;
                }

                if (select == 1 || select == 2)
                {
                    Menu_DrawSubmenuSaveSelect(select - 1 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            Setting.rtc_aging_offset = offset;
            RTC_ModifyAging(offset);
            SaveSetting(&Setting);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            BEEP_Button();
            BTN_WaitAll();
        }
    }
    BEEP_OK();
}

static void Menu_ResetAll(void) /* 恢复初始设置 */
{
    uint8_t select, save, update_display, wait_btn;

    Menu_DrawSubmenuFrame("恢复设置", 1);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 1;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 1)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;
                EPD_ClearArea(211, 15, 79, 1, 0xFF);
                switch (select)
                {
                case 0:
                    EPD_DrawImage(221, 15, EPD_Image_Arrow_12x8);
                    break;
                case 1:
                    EPD_DrawImage(268, 15, EPD_Image_Arrow_12x8);
                    break;
                }
                EPD_DrawUTF8(0, 5, 0, "清除数据并恢复到初始设置", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                EPD_DrawUTF8(0, 9, 0, "继续吗？", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                EPD_DrawUTF8(211, 13, 0, "继续", NULL, EPD_FontUTF8_16x16_B);
                EPD_DrawUTF8(258, 13, 0, "取消", NULL, EPD_FontUTF8_16x16_B);
                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            memcpy(&Setting, &DefaultSetting, sizeof(struct FUNC_Setting));
            Setting.available = SETTING_AVALIABLE_FLAG;
            SaveSetting(&Setting);

            EPD_ClearArea(0, 5, 296, 11, 0xFF);
            EPD_DrawUTF8(0, 5, 0, "恢复完成", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
            EPD_DrawUTF8(0, 9, 0, "三秒后返回主菜单", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
            EPD_Show(0);

            if (Setting.buzzer_enable != 0)
            {
                BUZZER_SetFrqe(4000);
                BUZZER_SetVolume(Setting.buzzer_volume);
                BUZZER_Beep(499);
            }

            LP_EnterStop(EPD_TIMEOUT_MS);
            LP_DelayStop(3000);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            BEEP_Button();
            BTN_WaitAll();
        }
    }
    BEEP_OK();
}

static void Menu_SetHWVer(void) /* 设置硬件版本 */
{
    uint8_t select, save, update_display, wait_btn, hwver_1, hwver_2;
    uint32_t hwver_stor;

    Menu_DrawSubmenuFrame("硬件版本设置", 0);
    BTN_WaitAll();

    update_display = 1;
    wait_btn = 0;
    save = 0;
    select = 0;
    hwver_1 = 0;
    hwver_2 = 0;
    while (save == 0)
    {
        if (BTN_ReadSET() == 0)
        {
            if (select < 3)
            {
                select += 1;
            }
            else
            {
                select = 0;
            }
            wait_btn = 1;
        }
        else
        {
            switch (select)
            {
            case 0:
                if (BTN_ReadUP() == 0)
                {
                    if (hwver_1 < 9)
                    {
                        hwver_1 += 1;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if (hwver_1 > 0)
                    {
                        hwver_1 -= 1;
                    }
                    wait_btn = 1;
                }
                break;
            case 1:
                if (BTN_ReadUP() == 0)
                {
                    if (hwver_2 < 9)
                    {
                        hwver_2 += 1;
                    }
                    wait_btn = 1;
                }
                else if (BTN_ReadDOWN() == 0)
                {
                    if (hwver_2 > 0)
                    {
                        hwver_2 -= 1;
                    }
                    wait_btn = 1;
                }
                break;
            case 2:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 2;
                }
                break;
            case 3:
                if (BTN_ReadUP() == 0)
                {
                    wait_btn = 0;
                    update_display = 0;
                    save = 1;
                }
                break;
            }
        }
        if (wait_btn != 0)
        {
            update_display = 1;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                snprintf(String, sizeof(String), "硬件版本：V%d.%d", hwver_1, hwver_2);
                EPD_DrawUTF8(0, 4, 0, String, EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                EPD_DrawUTF8(0, 8, 0, "[注意：设置保存后不会再", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);
                EPD_DrawUTF8(0, 12, 0, "显示此菜单]", EPD_FontAscii_12x24_B, EPD_FontUTF8_24x24_B);

                EPD_ClearArea(132, 7, 36, 1, 0xFF);

                switch (select)
                {
                case 0:
                    EPD_DrawImage(132, 7, EPD_Image_Arrow_12x8);
                    break;
                case 1:
                    EPD_DrawImage(156, 7, EPD_Image_Arrow_12x8);
                    break;
                }

                if (select == 2 || select == 3)
                {
                    Menu_DrawSubmenuSaveSelect(select - 2 + 1);
                }
                else
                {
                    Menu_DrawSubmenuSaveSelect(0);
                }

                EPD_Show(0);
            }
        }
        if (save == 2)
        {
            hwver_stor = 0x00002E00;
            hwver_stor |= (hwver_2 + 48) << 16;
            hwver_stor |= (hwver_1 + 48);
            EEPROM_WriteDWORD(EEPROM_ADDR_DWORD_HWVERSION, hwver_stor);
        }
        if (wait_btn != 0)
        {
            wait_btn = 0;
            BEEP_Button();
            BTN_WaitAll();
        }
    }
    BEEP_OK();
}

/* ==================== 电池图标绘制 ==================== */

static void EPD_DrawBattery(uint16_t x, uint8_t y_x8, float max_voltage, float min_voltage, float voltage)
{
    uint8_t dis_ram[sizeof(EPD_Image_BattFrame)];
    uint8_t i, bar_size;

    if ((voltage < min_voltage) || ((max_voltage - min_voltage) < 0.001))
    {
        EPD_DrawImage(x, y_x8, EPD_Image_BattWarn);
        return;
    }
    if (voltage < max_voltage)
    {
        voltage -= min_voltage;
        bar_size = (voltage / ((max_voltage - min_voltage) / 20)) + 0.5;
        if (bar_size == 0)
        {
            bar_size = 1;
        }
        else if (bar_size > 20)
        {
            bar_size = 20;
        }
    }
    else
    {
        bar_size = 20;
    }
    memcpy(dis_ram, EPD_Image_BattFrame, sizeof(dis_ram));
    for (i = 0; i < bar_size; i++)
    {
        dis_ram[84 - (i * 3) + 0] &= 0xF8;
        dis_ram[84 - (i * 3) + 1] &= 0x00;
        dis_ram[84 - (i * 3) + 2] &= 0x1F;
    }
    EPD_DrawImage(x, y_x8, dis_ram);
}

/* ==================== 设置存储 ==================== */

static void SaveSetting(const struct FUNC_Setting *setting)
{
    uint16_t i;
    uint8_t *setting_ptr;
    setting_ptr = (uint8_t *)setting;
    for (i = 0; i < sizeof(struct FUNC_Setting); i++)
    {
        if (EEPROM_ReadByte(EEPROM_ADDR_BYTE_SETTING + i) != setting_ptr[i])
        {
            EEPROM_WriteByte(EEPROM_ADDR_BYTE_SETTING + i, setting_ptr[i]);
        }
    }
}

static void ReadSetting(struct FUNC_Setting *setting)
{
    uint16_t i;
    uint8_t *setting_ptr;
    setting_ptr = (uint8_t *)setting;
    for (i = 0; i < sizeof(struct FUNC_Setting); i++)
    {
        setting_ptr[i] = EEPROM_ReadByte(EEPROM_ADDR_BYTE_SETTING + i);
    }
    if (setting->available != SETTING_AVALIABLE_FLAG)
    {
        SERIAL_DebugPrint("Read setting fail, load default setting");
        memcpy(setting, &DefaultSetting, sizeof(struct FUNC_Setting));
    }
}

/* ==================== 按键读取 ==================== */

static uint8_t BTN_ReadUP()
{
    if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) == 0)
    {
        LL_mDelay(BTN_DEBOUNCE_MS);
        if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static uint8_t BTN_ReadDOWN(void)
{
    if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
    {
        LL_mDelay(BTN_DEBOUNCE_MS);
        if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static uint8_t BTN_ReadSET(void)
{
    if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
    {
        LL_mDelay(BTN_DEBOUNCE_MS);
        if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static void BTN_WaitSET(void)
{
    while (1)
    {
        if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) != 0)
        {
            LL_mDelay(BTN_DEBOUNCE_MS);
            if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) != 0)
            {
                return;
            }
        }
    }
}

static void BTN_WaitAll(void)
{
    while (1)
    {
        if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) != 0 &&
            LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) != 0 &&
            LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) != 0)
        {
            LL_mDelay(BTN_DEBOUNCE_MS);
            if (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) != 0 &&
                LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) != 0 &&
                LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) != 0)
            {
                return;
            }
        }
    }
}

static uint8_t BTN_ModifySingleDigit(uint8_t *number, uint8_t modify_digit, uint8_t max_val, uint8_t min_val)
{
    double pow_tmp;
    uint8_t digit_value;
    if (BTN_ReadUP() == 0)
    {
        pow_tmp = pow(10, modify_digit);
        digit_value = (uint32_t)(*number / pow_tmp) % 10;
        if (digit_value < max_val)
        {
            *number += (uint32_t)pow_tmp;
        }
        else
        {
            *number -= (uint32_t)pow_tmp * digit_value;
            *number += (uint32_t)pow_tmp * min_val;
        }
        return 1;
    }
    else if (BTN_ReadDOWN() == 0)
    {
        pow_tmp = pow(10, modify_digit);
        digit_value = (uint32_t)(*number / pow_tmp) % 10;
        if (digit_value > min_val)
        {
            *number -= (uint32_t)pow_tmp;
        }
        else
        {
            *number -= (uint32_t)pow_tmp * digit_value;
            *number += (uint32_t)pow_tmp * max_val;
        }
        return 1;
    }
    return 0;
}

/* ==================== 蜂鸣器 ==================== */

static void BEEP_Fast(void)
{
    if (Setting.buzzer_enable != 0)
    {
        BUZZER_SetFrqe(4000);
        BUZZER_SetVolume(Setting.buzzer_volume);
        BUZZER_Beep(4);
    }
}

static void BEEP_Button(void)
{
    if (Setting.buzzer_enable != 0)
    {
        BUZZER_SetFrqe(4000);
        BUZZER_SetVolume(Setting.buzzer_volume);
        BUZZER_Beep(19);
    }
}

static void BEEP_OK(void)
{
    if (Setting.buzzer_enable != 0)
    {
        BUZZER_SetFrqe(4000);
        BUZZER_SetVolume(Setting.buzzer_volume);
        BUZZER_SetFrqe(1000);
        BUZZER_Beep(39);
        BUZZER_SetFrqe(4000);
        BUZZER_Beep(39);
    }
}

/* ==================== 电源控制 ==================== */

static void Power_EnableGDEH029A1(void)
{
    LL_GPIO_ResetOutputPin(EPD_POWER_GPIO_Port, EPD_POWER_Pin);
    LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    LL_GPIO_SetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_SetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    Delay_100ns(100); /* 10us，未要求，短暂延时 */
    if (LL_SPI_IsEnabled(SPI1) == 0)
    {
        LL_SPI_Enable(SPI1);
    }
}

static void Power_DisableGDEH029A1(void)
{
    if (LL_SPI_IsEnabled(SPI1) != 0)
    {
        LL_SPI_Disable(SPI1);
    }
    LL_GPIO_ResetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    LL_GPIO_ResetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_ResetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    LL_GPIO_SetOutputPin(EPD_POWER_GPIO_Port, EPD_POWER_Pin);
}

static void Power_Enable_SHT30_I2C(void)
{
    LL_GPIO_ResetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin); /* 打开SHT30电源 */
    LL_GPIO_SetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin);   /* 打开I2C上拉电阻 */
    LL_GPIO_SetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);       /* 释放SHT30复位引脚 */
    Delay_100ns(20);                                                /* 最少1us宽度，设置为2us */
    LL_GPIO_ResetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);     /* SHT30硬复位 */
    Delay_100ns(20);                                                /* 最少1us宽度，设置为2us */
    LL_GPIO_SetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);       /* SHT30硬复位 */
    LL_mDelay(1);                                                   /* SHT30复位后需要最少1ms启动时间，设置为2ms */
    if (LL_I2C_IsEnabled(I2C1) == 0)                                /* 打开I2C */
    {
        LL_I2C_Enable(I2C1);
    }
}

static void Power_Disable_I2C_SHT30(void)
{
    if (LL_I2C_IsEnabled(I2C1) != 0) /* 关闭I2C */
    {
        LL_I2C_Disable(I2C1);
    }
    LL_GPIO_ResetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin); /* 关闭I2C上拉电阻 */
    LL_GPIO_ResetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);     /* 拉低SHT30复位引脚 */
    LL_GPIO_SetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin);   /* 关闭SHT30电源 */
}

static uint8_t Power_EnableADC(void)
{
    ADC_Disable();
    ADC_StartCal();
    return ADC_Enable();
}

static uint8_t Power_DisableADC(void)
{
    return ADC_Disable();
}

static void Power_EnableBUZZER(void)
{
    BUZZER_Enable();
}

static void Power_DisableBUZZER(void)
{
    BUZZER_Disable();
}

static void Power_DisableUSART(void)
{
    LL_USART_Disable(SERIAL_NUM);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_9, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_9, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_10, LL_GPIO_PULL_NO);
}

/* ==================== 辅助功能 ==================== */

static void DumpRTCReg(void)
{
    uint8_t i, j, reg_tmp;
    char byte_str[9];
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("DS3231 REG DUMP:");
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
        SERIAL_SendString(byte_str);
        snprintf(byte_str, sizeof(byte_str), " 0x%02X", reg_tmp);
        SERIAL_SendStringRN(byte_str);
    }
    SERIAL_SendStringRN("DS3231 REG DUMP END");
    SERIAL_SendStringRN("");
}

static void DumpEEPROM(void)
{
    uint16_t i;
    char str_buffer[10];
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("EEPROM DUMP:");
    SERIAL_SendStringRN("INDEX:    00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F");
    for (i = 0; i < 2048; i++)
    {
        if (i % 16 == 0)
        {
            SERIAL_SendStringRN("");
        }
        if (i % 16 == 0)
        {
            snprintf(str_buffer, sizeof(str_buffer), "0x%04X    ", i);
            SERIAL_SendString(str_buffer);
        }
        snprintf(str_buffer, sizeof(str_buffer), "0x%02X ", EEPROM_ReadByte(i));
        SERIAL_SendString(str_buffer);
    }
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("EEPROM DUMP END");
    SERIAL_SendStringRN("");
}

static void DumpBKPR(void)
{
    uint16_t i;
    char str_buffer[10];
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("BKPR DUMP:");
    SERIAL_SendStringRN("INDEX:    00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F");
    for (i = 0; i < 20; i++)
    {
        if (i % 16 == 0)
        {
            SERIAL_SendStringRN("");
        }
        if (i % 16 == 0)
        {
            snprintf(str_buffer, sizeof(str_buffer), "0x%04X    ", i);
            SERIAL_SendString(str_buffer);
        }
        snprintf(str_buffer, sizeof(str_buffer), "0x%02X ", BKPR_ReadByte(i));
        SERIAL_SendString(str_buffer);
    }
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("BKPR DUMP END");
    SERIAL_SendStringRN("");
}

/*

 RTC_GetTime(&Time);
    snprintf(String, sizeof(String), "RTC: 2%03d %d %d %d PM:%d %02d:%02d:%02d TEMP:%5.2f", Time.Year, Time.Month, Time.Date, Time.Day, Time.PM, Time.Hours, Time.Minutes, Time.Seconds, RTC_GetTemp());
    SERIAL_SendStringRN(String);

    TH_GetValue_SingleShotWithCS(TH_ACC_HIGH, &Sensor);
    snprintf(String, sizeof(String), "TH : TEMP:%5.2f RH:%5.2f STATUS:0x%02X", Sensor.CEL, Sensor.RH, TH_GetStatus());
    SERIAL_SendStringRN(String);

    snprintf(String, sizeof(String), "ADC: VDDA:%5.2f TEMP:%5.2f CH1:%5.2f", ADC_GetVDDA(), ADC_GetTemp(), ADC_GetChannel(ADC_CHANNEL_BATTERY));
    SERIAL_SendStringRN(String);


*/

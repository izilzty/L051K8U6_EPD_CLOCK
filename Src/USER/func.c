#include "func.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

const struct FUNC_Setting DefaultSetting = {0xAA, 1.2, 1.0, 1, 3, 0.1};
const struct RTC_Time DefaultTime = {00, 0, 0, 4, 1, 10, 20, 0, 0}; /* 2020年10月1日，星期4，12:00:00，Is_12hr = 0，PM = 0  */

uint8_t ResetInfo;
struct RTC_Time Time;
struct Lunar_Date Lunar;
struct TH_Value Sensor;
struct FUNC_Setting Setting;
char String[256];

static void Power_EnableGDEH029A1(void);
static void Power_DisableGDEH029A1(void);
static void Power_Enable_SHT30_I2C(void);
static void Power_Disable_I2C_SHT30(void);
static void Power_EnableADC(void);
static void Power_DisableADC(void);
static void Power_EnableBUZZER(void);
static void Power_DisableBUZZER(void);

static uint8_t BTN_ReadUP(void);
static void BTN_WaitUP(void);
static uint8_t BTN_ReadDOWN(void);
static void BTN_WaitDOWN(void);
static uint8_t BTN_ReadSET(void);
static void BTN_WaitSET(void);
static void BTN_WaitAll(void);
static void BTN_ModifySingleDigit(uint8_t *number, uint8_t modify_digit, uint8_t max_val, uint8_t min_val, uint8_t *wait_btn_release, uint8_t *update_display);

static void EPD_DrawBattery(uint16_t x, uint8_t y_x8, float bat_voltage, float min_voltage, float voltage);

void FUNC_SaveSetting(const struct FUNC_Setting *setting);
void FUNC_ReadSetting(struct FUNC_Setting *setting);

void BEEP_Button(void);
void BEEP_OK(void);

static void Menu_SetTime(void);
static void Menu_MainMenu(void);
static void Menu_Guide(void);

static void DumpRTCReg(void);
static void DumpEEPROM(void);
static void DumpBKPR(void);
static void FullInit(void);

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

/* ==================== 主要功能 ==================== */

static void UpdateHomeDisplay(void) /* 更新显示时间和温度等数据 */
{
    uint32_t adc_val_stor;
    float battery_value;
    int8_t temp_value[2];
    int8_t rh_value[2];

    RTC_GetTime(&Time);
    TH_GetValue_SingleShotWithCS(TH_ACC_HIGH, &Sensor);

    RTC_ModifyINTCN(0);
    RTC_ModifyAM1Mask(0x00);
    RTC_ModifyA1IE(0);
    RTC_ClearA1F();
    RTC_ModifyAM2Mask(0x07);
    RTC_ModifyA2IE(1);
    RTC_ClearA2F();
    RTC_ModifyINTCN(1);

    Power_Disable_I2C_SHT30();

    adc_val_stor = BKPR_ReadDWORD(BKPR_ADDR_DWORD_ADCVAL);
    battery_value = *(float *)&adc_val_stor;
    if (battery_value < 0.1 || battery_value > 5.0)
    {
        battery_value = ADC_GetChannel(ADC_CHANNEL_BATTERY);
    }

    if (battery_value <= DCDC_MIN_VOLTAGE) /* 电池已经低于DC-DC最低工作电压，不更新显示直接进入Standby */
    {
        SERIAL_DebugPrint("Battery low, Direct enter standby mode");
        LP_EnterStandby();
        return;
    }

    LUNAR_SolarToLunar(&Lunar, Time.Year + 2000, Time.Month, Time.Date);

    Lunar.Month = 11;

    if (Sensor.CEL < 0)
    {
        temp_value[0] = (int8_t)(Sensor.CEL - 0.05);
    }
    else
    {
        temp_value[0] = (int8_t)(Sensor.CEL + 0.05);
    }
    temp_value[1] = (uint8_t)((Sensor.CEL - temp_value[0]) * 10);

    if (Sensor.RH < 0)
    {
        rh_value[0] = (int8_t)(Sensor.RH - 0.05);
    }
    else
    {
        rh_value[0] = (int8_t)(Sensor.RH + 0.05);
    }
    rh_value[1] = (uint8_t)((Sensor.RH - rh_value[0]) * 10);

    Power_EnableGDEH029A1();
    EPD_Init(EPD_UPDATE_MODE_FAST);
    EPD_ClearRAM();

    EPD_DrawHLine(0, 28, 296, 2);
    EPD_DrawHLine(0, 104, 296, 2);
    EPD_DrawHLine(213, 67, 76, 2);
    EPD_DrawVLine(202, 39, 56, 2);
    EPD_DrawBattery(263, 0, BATT_MAX_VOLTAGE, Setting.voltage_min, battery_value);

    snprintf(String, sizeof(String), "2%03d/%d/%02d 星期%s", Time.Year, Time.Month, Time.Date, Lunar_DayString[Time.Day]);
    EPD_DrawUTF8(0, 0, 1, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);
    if (Time.Is_12hr != 0)
    {
        if (Time.PM != 0)
        {
            EPD_DrawUTF8(0, 9, 2, "PM", EPD_FontAscii_12x24, EPD_FontUTF8_24x24);
        }
        else
        {
            EPD_DrawUTF8(0, 5, 2, "AM", EPD_FontAscii_12x24, EPD_FontUTF8_24x24);
        }
    }
    snprintf(String, sizeof(String), "%02d:%02d", Time.Hours, Time.Minutes);
    if (Time.Is_12hr != 0)
    {
        EPD_DrawUTF8(34, 5, 6, String, EPD_FontAscii_27x56, EPD_FontUTF8_24x24);
    }
    else
    {
        EPD_DrawUTF8(22, 5, 6, String, EPD_FontAscii_27x56, EPD_FontUTF8_24x24);
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
    EPD_DrawUTF8(213, 5, 0, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);
    if (rh_value[0] < 100)
    {
        snprintf(String, sizeof(String), "%02d.%d％", rh_value[0], rh_value[1]);
    }
    else
    {
        snprintf(String, sizeof(String), "%03d ％", rh_value[0]);
    }
    EPD_DrawUTF8(213, 9, 0, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);
    snprintf(String, sizeof(String), "农历：%s%s%s", Lunar_MonthLeapString[Lunar.IsLeap], Lunar_MonthString[Lunar.Month], Lunar_DateString[Lunar.Date]);
    EPD_DrawUTF8(0, 14, 2, String, NULL, EPD_FontUTF8_16x16);

    EPD_Show(0);
    LP_EnterSleep();

    battery_value = ADC_GetChannel(ADC_CHANNEL_BATTERY);
    BKPR_WriteDWORD(BKPR_ADDR_DWORD_ADCVAL, *(uint32_t *)&battery_value);

    EPD_EnterDeepSleep();
    Power_DisableGDEH029A1();
}

/* ==================== 主循环 ==================== */

void Init(void) /* 系统复位后首先进入此函数并执行一次 */
{
#ifdef ENABLE_DEBUG_PRINT
    SERIAL_SendStringRN("\r\n\r\n***** SYSTEM RESET *****\r\n");
    LL_mDelay(19);
#endif
    LL_EXTI_DisableIT_0_31(EPD_BUSY_EXTI0_EXTI_IRQn);                /* 禁用电子纸唤醒中断 */
    LL_SYSCFG_VREFINT_SetConnection(LL_SYSCFG_VREFINT_CONNECT_NONE); /* 禁用VREFINT输出 */
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

    if (ResetInfo == LP_RESET_NORMALRESET && ((BTN_ReadUP() == 0 && BTN_ReadDOWN() == 0) || BKPR_ReadByte(BKPR_ADDR_BYTE_REQINIT) == 0xAA))
    {
        FullInit();
        FUNC_SaveSetting(&DefaultSetting);
        memcpy(&Setting, &DefaultSetting, sizeof(struct FUNC_Setting));
        Menu_Guide();
        Menu_SetTime();
    }

    FUNC_ReadSetting(&Setting); /* 读取保存的设置，如果没有则使用默认设置代替 */

    Setting.buzzer_enable = 1;

    if (ADC_GetChannel(ADC_CHANNEL_BATTERY) < Setting.voltage_boot) /* 电池电压过低，直接进入Standby模式，防止反复断电 */
    {
        SERIAL_DebugPrint("Battery low, Direct enter standby mode");
        RTC_ClearA2F();
        RTC_ClearA1F();
        LP_EnterStandby();
    }
}

void Loop(void) /* 在Init()执行完成后循环执行 */
{
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        if (RTC_GetOSF() != 0)
        {
            Menu_Guide();
            Menu_SetTime();
        }
        BKPR_ResetAll();
        break;
    case LP_RESET_NORMALRESET:
        BKPR_ResetAll();
        break;
    case LP_RESET_WKUPSTANDBY:
        if (RTC_GetA2F() != 0)
        {
            RTC_ClearA2F();
        }
        else
        {
            Menu_MainMenu();
        }
        break;
    }

    UpdateHomeDisplay();

    BTN_WaitSET(); /* 等待设置按钮释放 */

    SERIAL_DebugPrint("Ready to enter standby mode");
    LP_EnterStandby(); /* 程序停止，等待下一次唤醒复位 */

    /* 正常情况下进入Standby模式后，程序会停止在此处，直到下次复位或唤醒再重头开始执行 */

    SERIAL_DebugPrint("Enter standby mode fail");
    LL_mDelay(999); /* 等待1000ms */
    SERIAL_DebugPrint("Try to enter standby mode again");
    LP_EnterStandby(); /* 再次尝试进去Standby模式 */
    SERIAL_DebugPrint("Enter standby mode fail");
    SERIAL_DebugPrint("Try to reset the system");
    NVIC_SystemReset(); /* 两次未成功进入Standby模式，执行软复位 */
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

static void Power_EnableADC(void)
{
    ADC_Disable();
    ADC_StartCal();
    ADC_Enable();
}

static void Power_DisableADC(void)
{
    ADC_Disable();
}

static void Power_EnableBUZZER(void)
{
    BUZZER_Enable();
}

static void Power_DisableBUZZER(void)
{
    BUZZER_Disable();
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
    if (EEPROM_EraseRange(0, 511) != 0)
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

    BUZZER_Beep(199);
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

static void BTN_WaitUP(void)
{
    while (1)
    {
        if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) != 0)
        {
            LL_mDelay(BTN_DEBOUNCE_MS);
            if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) != 0)
            {
                return;
            }
        }
    }
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

static void BTN_WaitDOWN(void)
{
    while (1)
    {
        if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) != 0)
        {
            LL_mDelay(BTN_DEBOUNCE_MS);
            if (LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) != 0)
            {
                return;
            }
        }
    }
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

static void BTN_ModifySingleDigit(uint8_t *number, uint8_t modify_digit, uint8_t max_val, uint8_t min_val, uint8_t *wait_btn_release, uint8_t *update_display)
{
    uint8_t digit[4];

    digit[0] = *number / 1000;
    digit[1] = (*number - (digit[0] * 1000)) / 100;
    digit[2] = (*number - (digit[0] * 1000) - (digit[1] * 100)) / 10;
    digit[3] = *number - (digit[0] * 1000) - (digit[1] * 100) - (digit[2] * 10);

    modify_digit = (3 - modify_digit) % 4;
    if (BTN_ReadUP() == 0)
    {
        if (digit[modify_digit] < max_val)
            digit[modify_digit] += 1;
        else
            digit[modify_digit] = min_val;
        *wait_btn_release = 1;
        *update_display = 1;
        BEEP_Button();
    }
    else if (BTN_ReadDOWN() == 0)
    {
        if (digit[modify_digit] > min_val)
            digit[modify_digit] -= 1;
        else
            digit[modify_digit] = max_val;
        *wait_btn_release = 1;
        *update_display = 1;
        BEEP_Button();
    }

    *number = digit[0] * 1000 + digit[1] * 100 + digit[2] * 10 + digit[3];
}

/* ==================== 图形绘制 ==================== */

static void EPD_DrawBattery(uint16_t x, uint8_t y_x8, float bat_voltage, float min_voltage, float voltage)
{
    uint8_t dis_ram[98];
    uint8_t i, bar_size;

    if (voltage < min_voltage)
    {
        EPD_DrawImage(x, y_x8, EPD_Image_BattWarn);
        return;
    }
    voltage -= min_voltage;
    bar_size = (voltage / ((bat_voltage - min_voltage) / 20)) + 0.5;
    if (bar_size == 0)
    {
        bar_size = 1;
    }
    else if (bar_size > 20)
    {
        bar_size = 20;
    }
    memcpy(dis_ram, EPD_Image_BattFrame, sizeof(dis_ram));
    for (i = 0; i < bar_size; i++)
    {
        dis_ram[83 - (i * 3) + 0] &= 0xF8;
        dis_ram[83 - (i * 3) + 1] &= 0x00;
        dis_ram[83 - (i * 3) + 2] &= 0x1F;
    }
    EPD_DrawImage(x, y_x8, dis_ram);
}

/* ==================== 设置存储 ==================== */

void FUNC_SaveSetting(const struct FUNC_Setting *setting)
{
    uint8_t i;
    uint8_t *setting_ptr;
    setting_ptr = (uint8_t *)setting;
    for (i = 0; i < sizeof(struct FUNC_Setting); i++)
    {
        if (EEPROM_ReadByte(EEPROM_ADDR_BYTE_SETTING + i) != setting_ptr[i])
        {
            EEPROM_WriteByte(EEPROM_ADDR_BYTE_SETTING + i, setting_ptr[i]);
        }
    }
    DumpEEPROM();
}

void FUNC_ReadSetting(struct FUNC_Setting *setting)
{
    uint8_t i;
    uint8_t *setting_ptr;
    setting_ptr = (uint8_t *)setting;
    for (i = 0; i < sizeof(struct FUNC_Setting); i++)
    {
        setting_ptr[i] = EEPROM_ReadByte(EEPROM_ADDR_BYTE_SETTING + i);
    }
    if (setting->available != 0xAA)
    {
        SERIAL_DebugPrint("Read setting fail, use default setting");
        memcpy(setting, &DefaultSetting, sizeof(struct FUNC_Setting));
    }
}

/* ==================== 蜂鸣器 ==================== */

void BEEP_Button(void)
{
    if (Setting.buzzer_enable != 0)
    {
        BUZZER_SetFrqe(4000);
        BUZZER_SetVolume(Setting.buzzer_volume);
        BUZZER_Beep(19);
    }
}

void BEEP_OK(void)
{
    if (Setting.buzzer_enable != 0)
    {        
        BUZZER_SetFrqe(4000);
        BUZZER_SetVolume(Setting.buzzer_volume);
        BUZZER_SetFrqe(1000);
        BUZZER_Beep(29);
        BUZZER_SetFrqe(4000);
        BUZZER_Beep(29);
    }
}

/* ==================== 子菜单 ==================== */

static void Menu_SetTime(void) /* 时间设置页面 */
{
    struct RTC_Time new_time;
    uint8_t i, digit_select, save_time, update_display, wait_btn, arrow_y;
    uint16_t arrow_x;
    float battery_value;

    BEEP_OK();

    Power_EnableGDEH029A1();

    EPD_Init(EPD_UPDATE_MODE_FAST);
    EPD_ClearRAM();
    for (i = 0; i < 2; i++)
    {
        EPD_DrawUTF8(0, 0, 0, "时间设置：", NULL, EPD_FontUTF8_24x24);
        EPD_DrawImage(161, 0, EPD_Image_Arrow_8x8);
        EPD_DrawImage(209, 0, EPD_Image_Arrow_8x8);
        EPD_DrawImage(257, 0, EPD_Image_Arrow_8x8);
        EPD_DrawUTF8(149, 1, 0, "移动", NULL, EPD_FontUTF8_16x16);
        EPD_DrawUTF8(205, 1, 0, "加", NULL, EPD_FontUTF8_16x16);
        EPD_DrawUTF8(253, 1, 0, "减", NULL, EPD_FontUTF8_16x16);
        EPD_DrawHLine(0, 27, 296, 2);
        if (i == 0)
        {
            EPD_Show(0);
            LP_EnterStop();
            EPD_Init(EPD_UPDATE_MODE_PART);
            EPD_ClearRAM();
            EPD_EnterSleep();
        }
    }
    BTN_WaitAll();

    RTC_GetTime(&new_time);
    if (RTC_GetOSF() != 0 || new_time.Month == 0)
    {
        memcpy(&new_time, &DefaultTime, sizeof(struct RTC_Time));
    }

    digit_select = 0;
    update_display = 1;
    wait_btn = 0;
    save_time = 2;
    while (save_time == 2)
    {
        if (BTN_ReadSET() == 0)
        {
            if (digit_select < 17)
            {
                digit_select += 1;
            }
            else
            {
                digit_select = 0;
            }
            update_display = 1;
            wait_btn = 1;
            BEEP_Button();
        }
        switch (digit_select)
        {
        case 0:
            BTN_ModifySingleDigit(&new_time.Year, 2, 1, 0, &wait_btn, &update_display);
            break;
        case 1:
            BTN_ModifySingleDigit(&new_time.Year, 1, 9, 0, &wait_btn, &update_display);
            break;
        case 2:
            BTN_ModifySingleDigit(&new_time.Year, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 3:
            BTN_ModifySingleDigit(&new_time.Month, 1, 1, 0, &wait_btn, &update_display);
            break;
        case 4:
            BTN_ModifySingleDigit(&new_time.Month, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 5:
            BTN_ModifySingleDigit(&new_time.Date, 1, 3, 0, &wait_btn, &update_display);
            break;
        case 6:
            BTN_ModifySingleDigit(&new_time.Date, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 7:
            BTN_ModifySingleDigit(&new_time.Day, 0, 7, 1, &wait_btn, &update_display);
            break;
        case 8:
            BTN_ModifySingleDigit(&new_time.Is_12hr, 0, 1, 0, &wait_btn, &update_display);
            break;
        case 9:
            if (new_time.Is_12hr == 0)
            {
                digit_select += 1;
            }
            BTN_ModifySingleDigit(&new_time.PM, 0, 1, 0, &wait_btn, &update_display);
            break;
        case 10:
            if (new_time.Is_12hr == 0)
            {
                BTN_ModifySingleDigit(&new_time.Hours, 1, 2, 0, &wait_btn, &update_display);
            }
            else
            {
                BTN_ModifySingleDigit(&new_time.Hours, 1, 1, 0, &wait_btn, &update_display);
            }
            break;
        case 11:
            BTN_ModifySingleDigit(&new_time.Hours, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 12:
            BTN_ModifySingleDigit(&new_time.Minutes, 1, 5, 0, &wait_btn, &update_display);
            break;
        case 13:
            BTN_ModifySingleDigit(&new_time.Minutes, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 14:
            BTN_ModifySingleDigit(&new_time.Seconds, 1, 5, 0, &wait_btn, &update_display);
            break;
        case 15:
            BTN_ModifySingleDigit(&new_time.Seconds, 0, 9, 0, &wait_btn, &update_display);
            break;
        case 16:
            if (BTN_ReadUP() == 0)
            {
                save_time = 1;
                wait_btn = 0;
                update_display = 0;
            }
            break;
        case 17:
            if (BTN_ReadUP() == 0)
            {
                save_time = 0;
                wait_btn = 0;
                update_display = 0;
            }
            break;
        }
        if (update_display != 0)
        {
            if (EPD_CheckBusy() == 0)
            {
                update_display = 0;

                if (digit_select == 16 || digit_select == 17)
                {
                    EPD_DrawUTF8(197, 1, 0, "选择", NULL, EPD_FontUTF8_16x16);
                    EPD_DrawUTF8(253, 1, 0, "空", NULL, EPD_FontUTF8_16x16);
                }
                else
                {
                    EPD_DrawUTF8(197, 1, 0, "    ", NULL, EPD_FontUTF8_16x16);
                    EPD_DrawUTF8(205, 1, 0, "加  ", NULL, EPD_FontUTF8_16x16);
                    EPD_DrawUTF8(253, 1, 0, "减", NULL, EPD_FontUTF8_16x16);
                }

                snprintf(String, sizeof(String), "2%03d年%02d月%02d日 周%d", new_time.Year, new_time.Month, new_time.Date, new_time.Day);
                EPD_DrawUTF8(7, 4, 5, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);

                if (new_time.Is_12hr != 0)
                {
                    snprintf(String, sizeof(String), "时间格式：12小时制");
                }
                else
                {
                    snprintf(String, sizeof(String), "时间格式：24小时制");
                }
                EPD_DrawUTF8(5, 8, 0, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);

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
                EPD_DrawUTF8(5, 12, 0, String, NULL, EPD_FontUTF8_24x24);

                snprintf(String, sizeof(String), "%02d:%02d:%02d", new_time.Hours, new_time.Minutes, new_time.Seconds);
                EPD_DrawUTF8(58, 12, 5, String, EPD_FontAscii_12x24, EPD_FontUTF8_24x24);

                EPD_DrawUTF8(211, 13, 0, "保存", NULL, EPD_FontUTF8_16x16);
                EPD_DrawUTF8(258, 13, 0, "取消", NULL, EPD_FontUTF8_16x16);

                if (digit_select >= 0 && digit_select <= 2)
                {
                    arrow_x = 24 + (digit_select * 17);
                    arrow_y = 7;
                }
                else if (digit_select >= 3 && digit_select <= 4)
                {
                    arrow_x = 104 + ((digit_select - 3) * 17);
                    arrow_y = 7;
                }
                else if (digit_select >= 5 && digit_select <= 6)
                {
                    arrow_x = 167 + ((digit_select - 5) * 17);
                    arrow_y = 7;
                }
                else if (digit_select >= 7 && digit_select <= 7)
                {
                    arrow_x = 276 + ((digit_select - 7) * 17);
                    arrow_y = 7;
                }
                else if (digit_select >= 8 && digit_select <= 8)
                {
                    arrow_x = 132;
                    arrow_y = 11;
                }
                else if (digit_select >= 9 && digit_select <= 9)
                {
                    arrow_x = 23;
                    arrow_y = 15;
                }
                else if (digit_select >= 10 && digit_select <= 11)
                {
                    arrow_x = 58 + ((digit_select - 10) * 17);
                    arrow_y = 15;
                }
                else if (digit_select >= 12 && digit_select <= 13)
                {
                    arrow_x = 109 + ((digit_select - 12) * 17);
                    arrow_y = 15;
                }
                else if (digit_select >= 14 && digit_select <= 15)
                {
                    arrow_x = 160 + ((digit_select - 14) * 17);
                    arrow_y = 15;
                }
                else if (digit_select >= 16 && digit_select <= 17)
                {
                    arrow_x = 221 + ((digit_select - 16) * 47);
                    arrow_y = 15;
                }
                EPD_ClearArea(5, 7, 283, 1, 0xFF);
                EPD_ClearArea(5, 11, 283, 1, 0xFF);
                EPD_ClearArea(5, 15, 283, 1, 0xFF);
                EPD_DrawImage(arrow_x, arrow_y, EPD_Image_Arrow_12x8);

                EPD_Show(0);
            }
        }
        if (wait_btn != 0)
        {
            BTN_WaitAll();
            wait_btn = 0;
        }
        if (save_time == 1)
        {
            RTC_SetTime(&new_time);
            RTC_ClearOSF();
        }
    }
    Power_DisableGDEH029A1();
    BEEP_OK();
    BTN_WaitAll();
}

static void Menu_Guide(void) /* 首次使用时的引导 */
{
}

/* ==================== 主菜单 ==================== */

static void Menu_MainMenu(void)
{
    Menu_SetTime();
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

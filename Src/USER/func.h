#ifndef _FUNC_H_
#define _FUNC_H_

#include "main.h"

#include "lowpower.h"
#include "analog.h"
#include "bkpr.h"
#include "eeprom.h"
#include "serial.h"
#include "iic.h"
#include "ds3231.h"
#include "sht30.h"
#include "gdeh029A1.h"
#include "buzzer.h"
#include "lunar.h"

/* 可修改 */
#define SOFT_VERSION "L051_1.06_MELANTHA"
#define BTN_DEBOUNCE_MS 24
#define BAT_MIN_VOLTAGE 0.80
#define BAT_MAX_VOLTAGE 3.00
/* 结束 */

#define BKPR_ADDR_DWORD_ADCVAL 0x00
#define BKPR_ADDR_BYTE_REQINIT 0x04

#define EEPROM_ADDR_BYTE_SETTING 0x00
#define EEPROM_ADDR_DWORD_HWVERSION 0x01FF

#define REQUEST_RESET_ALL_FLAG 0x55
#define SETTING_AVALIABLE_FLAG 0xAA

struct Func_Setting
{
    uint8_t available;
    uint8_t buzzer_enable;
    uint8_t buzzer_volume;
    float battery_warn;
    float battery_stop;
    float sensor_temp_offset;
    float sensor_rh_offset;
    int16_t vrefint_offset;
    int8_t rtc_aging_offset;
};

struct Func_Alarm
{
    uint8_t flag; /* 闹钟启用 使用LED 使用蜂鸣器 自动停止 使用12小时制 12小时制下午 0 0 */
    uint8_t week; /* 周一 周二 周三 周四 周五 周六 周日 0 */
    uint8_t hour;
    uint8_t min;
};

void Init(void);
void Loop(void);

#endif

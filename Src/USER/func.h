#ifndef _FUNC_H_
#define _FUNC_H_

#include "main.h"

#define ENABLE_DEBUG_PRINT

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

#define BTN_DEBOUNCE_MS 19

#define BKPR_ADDR_DWORD_ADCVAL 0x00
#define BKPR_ADDR_BYTE_REQINIT 0x04

#define EEPROM_ADDR_BYTE_SETTING 0x00

#define DCDC_MIN_VOLTAGE 0.85
#define BATT_MAX_VOLTAGE 3.00

struct FUNC_Setting
{
    uint8_t available;
    float voltage_min;
    float voltage_boot;
    uint8_t buzzer_enable;
    uint8_t buzzer_volume;
    float soft_ver;
};

void Init(void);
void Loop(void);

#endif

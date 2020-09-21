#ifndef _RTC_H_
#define _RTC_H_

#include "main.h"

#include "iic.h"

#define RTC_I2C_ADDR 0xD0

#define RTC_REG_SEC 0x00
#define RTC_REG_MIN 0x01
#define RTC_REG_HOR 0x02
#define RTC_REG_DAY 0x03
#define RTC_REG_DAT 0x04
#define RTC_REG_MON 0x05
#define RTC_REG_YER 0x06

#define RTC_REG_AL1_SEC 0x07
#define RTC_REG_AL1_MIN 0x08
#define RTC_REG_AL1_HOR 0x09
#define RTC_REG_AL1_DDT 0x0A

#define RTC_REG_AL2_MIN 0x0B
#define RTC_REG_AL2_HOR 0x0C
#define RTC_REG_AL2_DDT 0x0D

#define RTC_REG_CTL 0x0E
#define RTC_REG_STA 0x0F
#define RTC_REG_AGI 0x10
#define RTC_REG_TPM 0x11
#define RTC_REG_TPL 0x12

struct RTC_Time
{
    uint8_t Seconds;
    uint8_t Minutes;
    uint8_t Hours;
    uint8_t Day;
    uint8_t Date;
    uint8_t Month;
    uint8_t Year;
    uint8_t Is_12hr;
    uint8_t PM;
};

struct RTC_Alarm
{
    uint8_t Seconds;
    uint8_t Minutes;
    uint8_t Hours;
    uint8_t Day;
    uint8_t Date;
    uint8_t DY;
    uint8_t Is_12hr;
    uint8_t PM;
};

uint8_t RTC_ReadREG(uint8_t reg);
uint8_t RTC_WriteREG(uint8_t reg, uint8_t data);
uint8_t RTC_ReadREG_Multi(uint8_t start_reg, uint8_t read_size, uint8_t *read_data);
uint8_t RTC_WriteREG_Multi(uint8_t start_reg, uint8_t write_size, const uint8_t *write_data);
uint8_t RTC_ModifyREG(uint8_t reg, uint8_t mask, uint8_t new_val);
uint8_t RTC_TestREG(uint8_t reg, uint8_t mask);

uint8_t RTC_GetTime(struct RTC_Time *time);
uint8_t RTC_SetTime(const struct RTC_Time *time);

uint8_t RTC_GetAlarm1(struct RTC_Alarm *alarm);
uint8_t RTC_SetAlarm1(const struct RTC_Alarm *alarm);
uint8_t RTC_GetAlarm2(struct RTC_Alarm *alarm);
uint8_t RTC_SetAlarm2(const struct RTC_Alarm *alarm);

uint8_t RTC_GetAM1Mask(void);
uint8_t RTC_ModifyAM1Mask(uint8_t alarm_mask);
uint8_t RTC_GetAM2Mask(void);
uint8_t RTC_ModifyAM2Mask(uint8_t alarm_mask);
uint8_t RTC_GetEOSC(void);
uint8_t RTC_ModifyEOSC(uint8_t eosc);
uint8_t RTC_GetBBSQW(void);
uint8_t RTC_ModifyBBSQW(uint8_t bbsqw);
uint8_t RTC_GetCONV(void);
uint8_t RTC_ModifyCONV(uint8_t conv);
uint8_t RTC_GetRS(void);
uint8_t RTC_ModifyRS(uint8_t rs);
uint8_t RTC_GetINTCN(void);
uint8_t RTC_ModifyINTCN(uint8_t intcn);
uint8_t RTC_GetA2IE(void);
uint8_t RTC_ModifyA2IE(uint8_t a2ie);
uint8_t RTC_GetA1IE(void);
uint8_t RTC_ModifyA1IE(uint8_t a1ie);
uint8_t RTC_GetOSF(void);
uint8_t RTC_ClearOSF(void);
uint8_t RTC_GetEN32KHZ(void);
uint8_t RTC_ModifyEN32KHZ(uint8_t en32khz);
uint8_t RTC_GetBUSY(void);
uint8_t RTC_GetA2F(void);
uint8_t RTC_ClearA2F(void);
uint8_t RTC_GetA1F(void);
uint8_t RTC_ClearA1F(void);
int8_t RTC_GetAging(void);
uint8_t RTC_ModifyAging(int8_t aging);
float RTC_GetTemp(void);
uint8_t RTC_ResetAllRegToDefault(void);

#endif

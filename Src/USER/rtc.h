#ifndef _RTC_H_
#define _RTC_H_

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

#include "main.h"

#include "iic.h"

uint8_t RTC_ReadREG(uint8_t reg);

#endif

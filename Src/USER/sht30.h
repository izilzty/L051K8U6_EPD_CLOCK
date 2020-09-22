#ifndef _SHT30_H_
#define _SHT30_H_

#include "main.h"

#include "iic.h"

#define TH_I2C_ADDR ((0x44 & 0xFE) << 1)

#define TH_ACC_HIGH 0
#define TH_ACC_MID 1
#define TH_ACC_LOW 2

struct TH_Value
{
    float Humidity;
    float Temperature;
    int8_t RH_Int;
    uint8_t RH_Poi;
    int8_t Temp_Int;
    uint8_t Temp_Poi;
};

uint8_t TH_WriteCmd(uint16_t command);
uint8_t TH_ReadData(uint8_t *data, uint8_t data_size);
uint8_t TH_ReadCmd(uint16_t command, uint8_t *data, uint8_t data_size);

uint8_t TH_GetTH_SingleShotWithCS(uint8_t acc, struct TH_Value *value);
uint8_t TH_GetTH_Periodic(struct TH_Value *value);

uint8_t TH_GetStatus(void);
uint8_t TH_ClearStatus(void);
#endif

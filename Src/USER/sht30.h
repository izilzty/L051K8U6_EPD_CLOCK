#ifndef _SHT30_H_
#define _SHT30_H_

#include "main.h"

#include "iic.h"

#define TH_I2C_ADDR ((0x44 & 0xFE) << 1)

#define TH_ACC_HIGH 0
#define TH_ACC_MID 1
#define TH_ACC_LOW 2

#define TH_MPS_0_5 0
#define TH_MPS_1 1
#define TH_MPS_2 2
#define TH_MPS_4 3
#define TH_MPS_10 4

struct TH_Value
{
    float RH;
    float CEL;
    int8_t RH_Int;
    uint8_t RH_Point;
    int8_t CEL_Int;
    uint8_t CEL_Point;
};

uint8_t TH_WriteCmd(uint16_t command);
uint8_t TH_ReadData(uint8_t *data, uint8_t data_size);
uint8_t TH_ReadCmd(uint16_t command, uint8_t *data, uint8_t data_size);

uint8_t TH_GetValue_SingleShotWithCS(uint8_t acc, struct TH_Value *value);
uint8_t TH_StartConv_SingleShotWithoutCS(uint8_t acc);
uint8_t TH_GetValue_SingleShotWithoutCS(struct TH_Value *value);

uint8_t TH_StartConv_Periodic(uint8_t acc, uint8_t mps);
uint8_t TH_StartConv_ART(void);
uint8_t TH_GetValue_Periodic_ART(struct TH_Value *value);

uint8_t TH_BreakCommand(void);
uint8_t TH_SoftReset(void);
uint8_t TH_ModifyHeater(uint8_t enable);

uint8_t TH_GetStatus(void);
uint8_t TH_GetAlertPending(void);
uint8_t TH_GetHeaterState(void);
uint8_t TH_GetAlertRHState(void);
uint8_t TH_GetAlertTempState(void);
uint8_t TH_GetResetState(void);
uint8_t TH_GetCmdExecuteState(void);
uint8_t TH_GetDataChecksumState(void);
uint8_t TH_ClearStatus(void);

void TH_SetTemperatureOffset(float offset);
void TH_SetHumidityOffset(float offset);
float TH_GetTemperatureOffset(void);
float TH_GetHumidityOffset(void);
#endif

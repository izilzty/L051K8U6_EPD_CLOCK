#include "sht30.h"

static float TempOffset;
static float RHOffset;

static uint8_t crc8(const uint8_t *data, uint8_t data_size)
{
    uint8_t i, j, crc, polynomial;

    polynomial = 0x31;
    crc = 0xFF;
    for (i = 0; i < data_size; i++)
    {
        crc ^= *data++;
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (crc << 1) ^ polynomial;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static void readout_data_conv(const uint8_t *raw_data, struct TH_Value *value)
{
    float conv_tmp;

    conv_tmp = (-45 + 175 * (((raw_data[0] << 8) | raw_data[1]) / 65535.0)) + TempOffset;
    value->CEL = conv_tmp;
    if (conv_tmp > 0)
    {
        conv_tmp += 0.005;
    }
    else
    {
        conv_tmp -= 0.005;
    }
    value->CEL_Int = (int8_t)conv_tmp;
    value->CEL_Poi = (conv_tmp - (int8_t)conv_tmp) * 100;

    conv_tmp = (100 * (((raw_data[3] << 8) | raw_data[4]) / 65535.0)) + RHOffset;
    value->RH = conv_tmp;
    if (conv_tmp > 0)
    {
        conv_tmp += 0.005;
    }
    else
    {
        conv_tmp -= 0.005;
    }
    value->RH_Int = (int8_t)conv_tmp;
    value->RH_Poi = (conv_tmp - (int8_t)conv_tmp) * 100;
}

uint8_t TH_WriteCmd(uint16_t command)
{
    if (I2C_Start(TH_I2C_ADDR, 0, 2) != 0 ||
        I2C_WriteByte(command >> 8) != 0 ||
        I2C_WriteByte(command & 0x00FF) != 0)
    {
        return 1;
    }
    if (I2C_Stop() != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_ReadData(uint8_t *data, uint8_t data_size)
{
    uint8_t i;

    if (I2C_Start(TH_I2C_ADDR, 1, data_size) != 0)
    {
        return 1;
    }
    for (i = 0; i < data_size; i++)
    {
        data[i] = I2C_ReadByte();
    }
    if (I2C_Stop() != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_ReadCmd(uint16_t command, uint8_t *data, uint8_t data_size)
{
    uint8_t i;

    if (I2C_Start(TH_I2C_ADDR, 0, 2) != 0 ||
        I2C_WriteByte(command >> 8) != 0 ||
        I2C_WriteByte(command & 0x00FF) != 0 ||
        I2C_Start(TH_I2C_ADDR, 1, data_size) != 0)
    {
        return 1;
    }
    for (i = 0; i < data_size; i++)
    {
        data[i] = I2C_ReadByte();
    }
    if (I2C_Stop() != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_GetStatus(void)
{
    uint8_t i, status_tmp[3], ret;

    if (TH_ReadCmd(0xF32D, status_tmp, sizeof(status_tmp)) != 0)
    {
        return 0xFE;
    }
    if (crc8(status_tmp, 2) != status_tmp[2])
    {
        return 0xFF;
    }
    ret = 0;
    if ((status_tmp[0] & 0x80) != 0)
    {
        ret |= 0x40;
    }
    if ((status_tmp[0] & 0x20) != 0)
    {
        ret |= 0x40 >> 1;
    }
    if ((status_tmp[0] & 0x08) != 0)
    {
        ret |= 0x40 >> 2;
    }
    if ((status_tmp[0] & 0x04) != 0)
    {
        ret |= 0x40 >> 3;
    }
    if ((status_tmp[1] & 0x10) != 0)
    {
        ret |= 0x40 >> 4;
    }
    if ((status_tmp[1] & 0x02) != 0)
    {
        ret |= 0x40 >> 5;
    }
    if ((status_tmp[1] & 0x01) != 0)
    {
        ret |= 0x40 >> 6;
    }

    return ret;
}

uint8_t TH_ClearStatus(void)
{
    return TH_WriteCmd(0x3041);
}

uint8_t TH_GetTH_SingleShotWithCS(uint8_t acc, struct TH_Value *value)
{
    uint8_t ht_tmp[6];
    uint16_t cmd;

    switch (acc)
    {
    case TH_ACC_HIGH:
        cmd = 0x2C06;
        break;
    case TH_ACC_MID:
        cmd = 0x2C0D;
        break;
    case TH_ACC_LOW:
        cmd = 0x2C10;
        break;
    default:
        cmd = 0x2C06;
        break;
    }
    if (TH_ReadCmd(cmd, ht_tmp, sizeof(ht_tmp)) != 0)
    {
        return 1;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 2;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

uint8_t TH_StartConvTH_SingleShotWithoutCS(uint8_t acc)
{
    uint8_t ht_tmp[6];
    uint16_t cmd;

    switch (acc)
    {
    case TH_ACC_HIGH:
        cmd = 0x2400;
        break;
    case TH_ACC_MID:
        cmd = 0x240B;
        break;
    case TH_ACC_LOW:
        cmd = 0x2416;
        break;
    default:
        cmd = 0x2400;
        break;
    }
    if (TH_WriteCmd(cmd) != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_GetTH_SingleShotWithoutCS(struct TH_Value *value)
{
    uint8_t ht_tmp[6];
    if (TH_ReadData(ht_tmp, sizeof(ht_tmp)) != 0)
    {
        return 1;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 2;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

uint8_t TH_StartConvTH_Periodic(uint8_t acc,uint8_t mps)
{
    uint8_t ht_tmp[6];
    uint16_t cmd;

    switch (acc)
    {
    case TH_ACC_HIGH:
        cmd = 0x2400;
        break;
    case TH_ACC_MID:
        cmd = 0x240B;
        break;
    case TH_ACC_LOW:
        cmd = 0x2416;
        break;
    default:
        cmd = 0x2400;
        break;
    }
    if (TH_WriteCmd(cmd) != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_GetTH_Periodic(struct TH_Value *value)
{
    uint8_t ht_tmp[6];
    if (TH_ReadCmd(0xE000, ht_tmp, sizeof(ht_tmp)) != 0)
    {
        return 1;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 2;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

void TH_SetTempOffset(float offset)
{
    TempOffset = offset;
}

void TH_SetRHOffset(float offset)
{
    RHOffset = offset;
}

float TH_GetTempOffset(void)
{
    return TempOffset;
}

void TH_GetRHOffset(void)
{
    return RHOffset;
}

/*
static void acc_div(uint32_t num1, uint32_t num2, uint16_t *result)
{
    uint8_t i;
    uint16_t num_tmp[2];
    uint16_t div =10000;
    
    num_tmp[0]=0;
    num_tmp[1]=0;
    
    num_tmp[0] = num1 / num2;
    num1 -= num2 * num_tmp[0];
    for (i = 1; i < 5; )
    {
        num1 *= 10;
        printf("%d\r\n",num1);
        div/=10;
        if ((num1 / num2) > 0)
        {
            i++;
            num_tmp[1] += (num1 / num2)*div;
            printf("--%d\r\n",num1 / num2);
            num1 -= num1/num2*num2;
            printf("%d\r\n",num1);
        }
        if(num1==0)
        {
            break;
        }
    }
    printf("\r\n%d.%04d\r\n",num_tmp[0],num_tmp[1]);
}



*/

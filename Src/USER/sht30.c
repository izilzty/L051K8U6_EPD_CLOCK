#include "sht30.h"

static float TemperatureOffset;
static float HumidityOffset;

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

    conv_tmp = (-45 + 175 * (((raw_data[0] << 8) | raw_data[1]) / 65535.0)) + TemperatureOffset;
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

    conv_tmp = (100 * (((raw_data[3] << 8) | raw_data[4]) / 65535.0)) + HumidityOffset;
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
    uint8_t i, i2c_ret;

    i2c_ret = I2C_Start(TH_I2C_ADDR, 1, data_size);
    if (i2c_ret != 0 && i2c_ret != 3)
    {
        return 1;
    }
    else if (i2c_ret == 3)
    {
        return 2;
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

uint8_t TH_GetValue_SingleShotWithCS(uint8_t acc, struct TH_Value *value)
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

uint8_t TH_StartConv_SingleShotWithoutCS(uint8_t acc)
{
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

uint8_t TH_GetValue_SingleShotWithoutCS(struct TH_Value *value)
{
    uint8_t ht_tmp[6], i2c_ret;

    i2c_ret = TH_ReadData(ht_tmp, sizeof(ht_tmp));
    if (i2c_ret != 0 && i2c_ret != 2)
    {
        return 1;
    }
    else if (i2c_ret == 2)
    {
        return 2;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 3;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

uint8_t TH_StartConv_Periodic(uint8_t acc, uint8_t mps)
{
    uint8_t ht_tmp[6];
    uint16_t cmd;
    const uint8_t cmd_map[15] = {
        0x32, 0x24, 0x2F, /* 0.5 mps */
        0x30, 0x26, 0x2D, /* 1 mps */
        0x36, 0x20, 0x2B, /* 2 mps */
        0x34, 0x22, 0x29, /* 4 mps */
        0x37, 0x21, 0x2A  /* 10 mps */
    };

    switch (mps)
    {
    case TH_MPS_0_5:
        cmd = 0x2000;
        break;
    case TH_MPS_1:
        cmd = 0x2100;
        break;
    case TH_MPS_2:
        cmd = 0x2200;
        break;
    case TH_MPS_4:
        cmd = 0x2300;
        break;
    case TH_MPS_10:
        cmd = 0x2700;
        break;
    default:
        cmd = 0x2000;
        break;
    }
    cmd |= cmd_map[mps * 3 + acc];
    if (TH_WriteCmd(cmd) != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_StartConv_ART(void)
{
    if (TH_WriteCmd(0x2B32) != 0)
    {
        return 1;
    }
    return 0;
}

uint8_t TH_GetValue_Periodic_ART(struct TH_Value *value)
{
    uint8_t ht_tmp[6], i2c_ret;

    i2c_ret = TH_ReadCmd(0xE000, ht_tmp, sizeof(ht_tmp));
    if (i2c_ret != 0 && i2c_ret != 2)
    {
        return 1;
    }
    else if (i2c_ret == 2)
    {
        return 2;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 3;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

uint8_t TH_BreakCommand(void)
{
    uint8_t th_ret;
    th_ret = TH_WriteCmd(0x3093);
    if (th_ret == 0)
    {
        LL_mDelay(1); /* 必要延时 */
    }
    return th_ret;
}

uint8_t TH_SoftReset(void)
{
    uint8_t th_ret;
    th_ret = TH_WriteCmd(0x3041);
    if (th_ret == 0)
    {
        LL_mDelay(1); /* 必要延时 */
    }
    return th_ret;
}

uint8_t TH_ModifyHeater(uint8_t is_enable)
{
    uint8_t th_ret;

    if (is_enable != 0)
    {
        th_ret = TH_WriteCmd(0x306D);
    }
    else
    {
        th_ret = TH_WriteCmd(0x3066);
    }
    if (th_ret == 0)
    {
        LL_mDelay(1); /* 必要延时 */
    }
    return th_ret;
}

uint8_t TH_GetStatus(void)
{
    uint8_t i, status_tmp[3], ret;
    uint16_t status_val;
    const uint8_t mask_map[7] = {
        0x80, 0x20, 0x08, 0x04, 0x10, 0x02, 0x01};

    if (TH_ReadCmd(0xF32D, status_tmp, sizeof(status_tmp)) != 0)
    {
        return 0xFE;
    }
    if (crc8(status_tmp, 2) != status_tmp[2])
    {
        return 0xFF;
    }
    ret = 0;
    for (i = 0; i < sizeof(mask_map); i++)
    {
        if (i < 4)
        {
            if ((status_tmp[0] & mask_map[i]) != 0)
            {
                ret |= 0x40 >> i;
            }
        }
        else
        {
            if ((status_tmp[1] & mask_map[i]) != 0)
            {
                ret |= 0x40 >> i;
            }
        }
    }
    return ret;
}

uint8_t TH_ClearStatus(void)
{
    uint8_t th_ret;
    th_ret = TH_WriteCmd(0x3041);
    if (th_ret == 0)
    {
        LL_mDelay(1); /* 必要延时 */
    }
    return th_ret;
}

void TH_SetTemperatureOffset(float offset)
{
    TemperatureOffset = offset;
}

void TH_SetHumidityOffset(float offset)
{
    HumidityOffset = offset;
}

float TH_GetTemperatureOffset(void)
{
    return TemperatureOffset;
}

float TH_GetHumidityOffset(void)
{
    return HumidityOffset;
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

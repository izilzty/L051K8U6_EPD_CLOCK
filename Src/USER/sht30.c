#include "sht30.h"

static float TemperatureOffset = 0;
static float HumidityOffset = 0;

/**
 * @brief  计算CRC-8校验值。
 * @param  data 要计算的数据。
 * @param  data_size 数据大小。
 * @return 计算结果。
 */
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

/**
 * @brief  传感器原始数据转为实际数据。
 * @param  raw_data 原始数据。
 * @param  value 实际数据存储结构体。
 */
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

/**
 * @brief  向传感器发送命令。
 * @param  cmd 要发送的命令。
 * @return 1：发送失败，0：发送成功。
 */
uint8_t TH_WriteCmd(uint16_t cmd)
{
    if (I2C_Start(TH_I2C_ADDR, 0, 2) != 0 ||
        I2C_WriteByte(cmd >> 8) != 0 ||
        I2C_WriteByte(cmd & 0x00FF) != 0)
    {
        return 1;
    }
    if (I2C_Stop() != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从传感器读取数据。
 * @param  data 数据指针。
 * @param  data_size 数据大小。
 * @return 1：读取失败，0：读取成功。
 */
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

/**
 * @brief  向传感器发送命令后读取数据。
 * @param  cmd 要发送的命令。
 * @param  data 数据指针。
 * @param  data_size 数据大小。
 * @return 1：读取或命令执行失败，0：读取或命令执行成功。
 */
uint8_t TH_ReadCmd(uint16_t cmd, uint8_t *data, uint8_t data_size)
{
    uint8_t i;

    if (I2C_Start(TH_I2C_ADDR, 0, 2) != 0 ||
        I2C_WriteByte(cmd >> 8) != 0 ||
        I2C_WriteByte(cmd & 0x00FF) != 0 ||
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

/**
 * @brief  以单次模式开始转换数据并在完成后读取（在转换完成前等待）。
 * @param  acc 数据精确等级。
 * @param  value 数据存储结构体。
 * @return 1：读取失败，0：读取成功。
 */
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

/**
 * @brief  以单次模式开始转换数据（发送开始转换命令后返回，后续使用其他命令读取返回值）。
 * @param  acc 数据精确等级。
 * @return 1：启动失败，0：启动成功。
 */
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

/**
 * @brief  读取单次模式转换完成的数据。
 * @param  value 数据存储结构体。
 * @return 1：读取失败，0：读取成功。
 */
uint8_t TH_GetValue_SingleShotWithoutCS(struct TH_Value *value)
{
    uint8_t ht_tmp[6];

    if (TH_ReadData(ht_tmp, sizeof(ht_tmp)) != 0)
    {
        return 1;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 1;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

/**
 * @brief  开始连续转换。
 * @param  acc 数据精确等级。
 * @param  mps 转换速度。
 * @return 1：启动失败，0：启动成功。
 */
uint8_t TH_StartConv_Periodic(uint8_t acc, uint8_t mps)
{
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

/**
 * @brief  开始加速响应速度的连续转换。
 * @return 1：启动失败，0：启动成功。
 */
uint8_t TH_StartConv_ART(void)
{
    if (TH_WriteCmd(0x2B32) != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  读取连续转换的数据。
 * @param  value 数据存储结构体。
 * @return 1：读取失败（出错或转换未完成），0：读取成功。
 */
uint8_t TH_GetValue_Periodic_ART(struct TH_Value *value)
{
    uint8_t ht_tmp[6];

    if (TH_ReadCmd(0xE000, ht_tmp, sizeof(ht_tmp)) != 0)
    {
        return 1;
    }
    if (crc8(ht_tmp, 2) != ht_tmp[2] || crc8(ht_tmp + 3, 2) != ht_tmp[5])
    {
        return 1;
    }
    readout_data_conv(ht_tmp, value);
    return 0;
}

/**
 * @brief  中断当前命令或停止连续转换。
 * @return 1：执行失败，0：执行成功。
 */
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

/**
 * @brief  软复位。
 * @return 1：执行失败，0：执行成功。
 */
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

/**
 * @brief  打开或关闭加热器。
 * @param  enable 开启或关闭加热器。
 * @return 1：执行失败，0：执行成功。
 */
uint8_t TH_ModifyHeater(uint8_t enable)
{
    uint8_t th_ret;

    if (enable != 0)
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

/**
 * @brief  获取传感器状态。
 * @return 传感器状态（从左往右分别是：0 警报待处理 加热器状态 湿度警报状态 温度警报状态 系统复位状态 命令执行状态 数据校验状态）。
 */
uint8_t TH_GetStatus(void)
{
    uint8_t i, status_tmp[3], ret;
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

/**
 * @brief  获取待处理警报状态。
 * @return 1：有待处理警报，0：无待处理警报。
 */
uint8_t TH_GetAlertPending(void)
{
    if ((TH_GetStatus() & 0x40) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取加热器状态。
 * @return 1：加热器已开启，0：加热器已关闭。
 */
uint8_t TH_GetHeater(void)
{
    if ((TH_GetStatus() & 0x20) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取湿度理警报状态。
 * @return 1：有警报，0：无警报。
 */
uint8_t TH_GetAlertRH(void)
{
    if ((TH_GetStatus() & 0x10) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取温度理警报状态。
 * @return 1：有警报，0：无警报。
 */
uint8_t TH_GetAlertTemp(void)
{
    if ((TH_GetStatus() & 0x08) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取传感器复位状态。
 * @return 1：有复位发生，0：无未发生。
 */
uint8_t TH_GetResetState(void)
{
    if ((TH_GetStatus() & 0x04) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取最后一条命令状态。
 * @return 1：最后一条命令执行失败，0：最后一条命令执行成功。
 */
uint8_t TH_GetCmdExecute(void)
{
    if ((TH_GetStatus() & 0x02) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  获取写入数据校验状态。
 * @return 1：校验错误，0：校验正确。
 */
uint8_t TH_GetDataChecksum(void)
{
    if ((TH_GetStatus() & 0x01) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  清除传感器状态。
 * @return 1：清除失败，0：清除成功。
 */
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

/**
 * @brief  设置温度偏移。
 * @param  offset 温度偏移。
 */
void TH_SetTemperatureOffset(float offset)
{
    TemperatureOffset = offset;
}

/**
 * @brief  设置湿度偏移。
 * @param  offset 湿度偏移。
 */
void TH_SetHumidityOffset(float offset)
{
    HumidityOffset = offset;
}

/**
 * @brief  读取温度偏移。
 * @return 温度偏移。
 */
float TH_GetTemperatureOffset(void)
{
    return TemperatureOffset;
}

/**
 * @brief  读取湿度偏移。
 * @return 湿度偏移。
 */
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

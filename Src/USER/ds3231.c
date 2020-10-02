#include "ds3231.h"

#include "serial.h"
#include <stdio.h>
/**
 * @brief  BIN转BCD。
 * @param  bin 要转换的数值。
 * @return 转换完成的数值。
 */
static uint8_t bin_to_bcd(uint8_t bin)
{
    uint8_t tmp;
    tmp = bin % 10;
    tmp += bin / 10 * 16;
    return tmp;
}

/**
 * @brief  BCD转BIN。
 * @param  bcd 要转换的数值。
 * @return 转换完成的数值。
 */
static uint8_t bcd_to_bin(uint8_t bcd)
{
    uint8_t tmp;
    tmp = bcd % 16;
    tmp += bcd / 16 * 10;
    return tmp;
}

/**
 * @brief  读取寄存器。
 * @param  reg 要读取的寄存器。
 * @return 读取到的数值。
 */
uint8_t RTC_ReadREG(uint8_t reg)
{
    uint8_t read_temp;

    if (I2C_Start(RTC_I2C_ADDR, 0, 1) != 0 ||
        I2C_WriteByte(reg) != 0 ||
        I2C_Start(RTC_I2C_ADDR, 1, 1) != 0)
    {
        return 0x00;
    }
    read_temp = I2C_ReadByte();
    I2C_Stop();
    return read_temp;
}

/**
 * @brief  写入寄存器。
 * @param  reg 要写入的寄存器。
 * @param  data 要写入的数值。
 * @return 1：写入失败，0：写入成功。
 */
uint8_t RTC_WriteREG(uint8_t reg, uint8_t data)
{
    if (I2C_Start(RTC_I2C_ADDR, 0, 2) != 0 ||
        I2C_WriteByte(reg) != 0 ||
        I2C_WriteByte(data) != 0)
    {
        return 1;
    }
    I2C_Stop();
    return 0;
}

/**
 * @brief  读取多个寄存器。
 * @param  start_reg 起始寄存器。
 * @param  read_size 要读取的大小。
 * @param  read_data 存放数值的指针。
 * @return 1：读取失败，0：读取成功。
 */
uint8_t RTC_ReadREG_Multi(uint8_t start_reg, uint8_t read_size, uint8_t *read_data)
{
    uint8_t i;
    if (I2C_Start(RTC_I2C_ADDR, 0, 1) != 0 ||
        I2C_WriteByte(start_reg) != 0 ||
        I2C_Start(RTC_I2C_ADDR, 1, read_size) != 0)
    {
        return 1;
    }
    for (i = 0; i < read_size; i++)
    {
        read_data[i] = I2C_ReadByte();
    }
    I2C_Stop();
    return 0;
}

/**
 * @brief  写入多个寄存器。
 * @param  start_reg 起始寄存器。
 * @param  write_size 要写入的大小。
 * @param  write_data 存放数值的指针。
 * @return 1：写入失败，0：写入成功。
 */
uint8_t RTC_WriteREG_Multi(uint8_t start_reg, uint8_t write_size, const uint8_t *write_data)
{
    uint8_t i, err;
    if (I2C_Start(RTC_I2C_ADDR, 0, write_size + 1) != 0 ||
        I2C_WriteByte(start_reg) != 0)
    {
        return 1;
    }
    err = 0;
    for (i = 0; i < write_size; i++)
    {
        if (I2C_WriteByte(write_data[i]) != 0)
        {
            err = 1;
            break;
        }
    }
    I2C_Stop();
    return err;
}

/**
 * @brief  修改寄存器中的某个或多个位。
 * @param  reg 要修改的寄存器。
 * @param  mask 掩码，要修改的位为1。
 * @param  new_val 要写入的新值。
 * @return 1：写入失败，0：写入成功。
 */
uint8_t RTC_ModifyREG(uint8_t reg, uint8_t mask, uint8_t new_val)
{
    uint8_t status_tmp;
    status_tmp = RTC_ReadREG(reg);
    status_tmp &= ~mask;
    status_tmp |= new_val & mask;
    if (RTC_WriteREG(reg, status_tmp) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  检查寄存器内的某个位是否为1。
 * @param  reg 要检查的寄存器。
 * @param  mask 掩码，要检查的位为1。
 * @return 1：为1，0：为0。
 */
uint8_t RTC_TestREG(uint8_t reg, uint8_t mask)
{
    if ((RTC_ReadREG(reg) & mask) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  读取实时时钟时间。
 * @param  time 时间存储结构体。
 * @return 1：读取失败，0：读取成功。
 */
uint8_t RTC_GetTime(struct RTC_Time *time)
{
    uint8_t i, *time_tmp;

    time_tmp = (uint8_t *)time;
    if (RTC_ReadREG_Multi(RTC_REG_SEC, sizeof(struct RTC_Time) - 2, time_tmp) != 0)
    {
        for (i = 0; i < sizeof(struct RTC_Time); i++)
        {
            time_tmp[i] = 0x00;
        }
        return 1;
    }
    for (i = 0; i < sizeof(struct RTC_Time) - 2; i++) /* BCD转BIN */
    {
        if (i == 2) /* 12小时制标志处理 */
        {
            if ((time_tmp[i] & 0x40) != 0) /* RTC当前使用的是12小时制 */
            {
                time->Is_12hr = 1;
                if ((time_tmp[i] & 0x20) != 0)
                {
                    time->PM = 1;
                }
                else
                {
                    time->PM = 0;
                }
                time_tmp[i] &= 0x1F;
            }
            else /* RTC当前使用的是24小时制 */
            {
                time->Is_12hr = 0;
                time->PM = 0;
                time_tmp[i] &= 0x3F;
            }
        }
        else if (i == 5 && (time_tmp[i] & 0x80) != 0) /* 世纪处理 */
        {
            time_tmp[i] = bcd_to_bin(time_tmp[i] & 0x1F); /* 转换月 */
            i += 1;
            time_tmp[i] = bcd_to_bin(time_tmp[i]) + 100; /* 转换年 */
            continue;
        }
        time_tmp[i] = bcd_to_bin(time_tmp[i]);
    }
    time->Day = ((time->Day + 12) % 7) + 1; /* 转换星期，以1为星期日 */
    return 0;
}

/**
 * @brief  设置实时时钟时间。
 * @param  time 时间存储结构体。
 * @return 1：设置失败，0：设置成功。
 */
uint8_t RTC_SetTime(const struct RTC_Time *time)
{
    uint8_t i, time_tmp[7];

    time_tmp[0] = time->Seconds;
    time_tmp[1] = time->Minutes;
    time_tmp[2] = time->Hours;
    time_tmp[3] = ((time->Day + 7) % 7) + 1; /* 转换星期，以1为星期日 */
    time_tmp[4] = time->Date;
    time_tmp[5] = time->Month;
    time_tmp[6] = time->Year;

    for (i = 0; i < sizeof(time_tmp); i++) /* BIN转BCD */
    {
        if (i == 6 && time_tmp[i] > 99) /* 世纪处理 */
        {
            time_tmp[i] -= 100;
            time_tmp[i - 1] |= 0x80;
        }
        time_tmp[i] = bin_to_bcd(time_tmp[i]);
    }

    if (time->Is_12hr != 0) /* 12小时制标志处理 */
    {
        time_tmp[2] &= 0x1F; /* 清除将要使用的标志 */
        time_tmp[2] |= 0x40; /* 设置12小时标志 */
        if (time->PM != 0)
        {
            time_tmp[2] |= 0x20; /* 设置PM标志 */
        }
    }

    /* 设置时间 -> 打开震荡器 -> 清除振荡器停止历史 */
    if (RTC_WriteREG_Multi(RTC_REG_SEC, sizeof(time_tmp), time_tmp) != 0 || RTC_ModifyEOSC(0) != 0 || RTC_ClearOSF() != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  读取闹钟1时间。
 * @param  alarm 闹钟存储结构体。
 * @return 1：读取失败，0：读取成功。
 */
uint8_t RTC_GetAlarm1(struct RTC_Alarm *alarm)
{
    uint8_t i, *alarm_tmp;

    alarm_tmp = (uint8_t *)alarm;
    if (RTC_ReadREG_Multi(RTC_REG_AL1_SEC, sizeof(struct RTC_Alarm) - 4, alarm_tmp) != 0)
    {
        for (i = 0; i < sizeof(struct RTC_Alarm); i++)
        {
            alarm_tmp[i] = 0x00;
        }
        return 1;
    }

    if ((alarm->Hours & 0x40) != 0) /* 12小时制 */
    {
        alarm->Is_12hr = 1;
        if ((alarm->Hours & 0x60) != 0) /* PM标志 */
        {
            alarm->PM = 1;
        }
        else
        {
            alarm->PM = 0;
        }
        alarm->Hours &= 0x1F;
    }
    else /* 24小时制 */
    {
        alarm->Is_12hr = 0;
        alarm->PM = 0;
        alarm->Hours &= 0x3F;
    }

    if ((alarm->Day & 0x40) != 0) /* 存储的是Day */
    {
        alarm->Day &= 0x0F;
        alarm->DY = 1;
    }
    else /* 存储的是Date */
    {
        alarm->Date = alarm->Day & 0x3F;
        alarm->Day = 0;
        alarm->DY = 0;
    }

    for (i = 0; i < sizeof(struct RTC_Alarm) - 3; i++) /* BCD转BIN */
    {
        alarm_tmp[i] = bcd_to_bin(alarm_tmp[i] & 0x7F);
    }

    if (alarm->DY != 0)
    {
        alarm->Day = ((alarm->Day + 12) % 7) + 1; /* 转换星期，以1为星期日 */
    }

    return 0;
}

/**
 * @brief  设置闹钟1时间。
 * @param  alarm 闹钟存储结构体。
 * @return 1：写入失败，0：写入成功。
 */
uint8_t RTC_SetAlarm1(const struct RTC_Alarm *alarm)
{
    uint8_t ret;

    ret = RTC_ModifyREG(RTC_REG_AL1_SEC, 0x7F, bin_to_bcd(alarm->Seconds));
    ret = RTC_ModifyREG(RTC_REG_AL1_MIN, 0x7F, bin_to_bcd(alarm->Minutes));
    if (alarm->Is_12hr != 0)
    {
        if (alarm->PM != 0)
        {
            ret = RTC_ModifyREG(RTC_REG_AL1_HOR, 0x7F, bin_to_bcd(alarm->Hours) | 0x60);
        }
        else
        {
            ret = RTC_ModifyREG(RTC_REG_AL1_HOR, 0x7F, bin_to_bcd(alarm->Hours) | 0x40);
        }
    }
    else
    {
        ret = RTC_ModifyREG(RTC_REG_AL1_HOR, 0x7F, bin_to_bcd(alarm->Hours));
    }
    if (alarm->DY != 0)
    {
        ret = RTC_ModifyREG(RTC_REG_AL1_DDT, 0x7F, bin_to_bcd(((alarm->Day + 7) % 7) + 1) | 0x40);
    }
    else
    {
        ret = RTC_ModifyREG(RTC_REG_AL1_DDT, 0x7F, bin_to_bcd(alarm->Date));
    }
    if (ret != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  读取闹钟2时间。
 * @param  alarm 闹钟存储结构体。
 * @return 1：读取失败，0：读取成功。
 */
uint8_t RTC_GetAlarm2(struct RTC_Alarm *alarm)
{
    uint8_t i, *alarm_tmp;

    alarm_tmp = (uint8_t *)alarm;
    if (RTC_ReadREG_Multi(RTC_REG_AL2_MIN, sizeof(struct RTC_Alarm) - 4, alarm_tmp + 1) != 0)
    {
        for (i = 0; i < sizeof(struct RTC_Alarm); i++)
        {
            alarm_tmp[i] = 0x00;
        }
        return 1;
    }

    if ((alarm->Hours & 0x40) != 0) /* 12小时制 */
    {
        alarm->Is_12hr = 1;
        if ((alarm->Hours & 0x60) != 0) /* PM标志 */
        {
            alarm->PM = 1;
        }
        else
        {
            alarm->PM = 0;
        }
        alarm->Hours &= 0x1F;
    }
    else /* 24小时制 */
    {
        alarm->Is_12hr = 0;
        alarm->PM = 0;
        alarm->Hours &= 0x3F;
    }

    if ((alarm->Day & 0x40) != 0) /* 存储的是Day */
    {
        alarm->Day &= 0x0F;
        alarm->DY = 1;
    }
    else /* 存储的是Date */
    {
        alarm->Date = alarm->Day & 0x3F;
        alarm->Day = 0;
        alarm->DY = 0;
    }

    for (i = 1; i < sizeof(struct RTC_Alarm) - 3; i++) /* BCD转BIN */
    {
        alarm_tmp[i] = bcd_to_bin(alarm_tmp[i] & 0x7F);
    }

    if (alarm->DY != 0)
    {
        alarm->Day = ((alarm->Day + 12) % 7) + 1; /* 转换星期，以1为星期日 */
    }

    return 0;
}

/**
 * @brief  设置闹钟2时间。
 * @param  alarm 闹钟存储结构体。
 * @return 1：写入失败，0：写入成功。
 */
uint8_t RTC_SetAlarm2(const struct RTC_Alarm *alarm)
{
    uint8_t ret;

    ret = RTC_ModifyREG(RTC_REG_AL2_MIN, 0x7F, bin_to_bcd(alarm->Minutes));
    if (alarm->Is_12hr != 0)
    {
        if (alarm->PM != 0)
        {
            ret = RTC_ModifyREG(RTC_REG_AL2_HOR, 0x7F, bin_to_bcd(alarm->Hours) | 0x60);
        }
        else
        {
            ret = RTC_ModifyREG(RTC_REG_AL2_HOR, 0x7F, bin_to_bcd(alarm->Hours) | 0x40);
        }
    }
    else
    {
        ret = RTC_ModifyREG(RTC_REG_AL2_HOR, 0x7F, bin_to_bcd(alarm->Hours));
    }
    if (alarm->DY != 0)
    {
        ret = RTC_ModifyREG(RTC_REG_AL2_DDT, 0x7F, bin_to_bcd(alarm->Day) | 0x40);
    }
    else
    {
        ret = RTC_ModifyREG(RTC_REG_AL2_DDT, 0x7F, bin_to_bcd(alarm->Date));
    }
    if (ret != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  获取闹钟1运行模式。
 * @return 运行模式，从左往右分别为：0 0 0 DY A1M4 A1M3 A1M2 A1M1 。
 */
uint8_t RTC_GetAM1Mask(void)
{
    uint8_t i, alarm_mask;
    alarm_mask = 0x00;
    if (RTC_TestREG(RTC_REG_AL1_DDT, 0x40) != 0)
    {
        alarm_mask |= 0x10;
    }
    for (i = 0; i < 4; i++)
    {
        alarm_mask |= RTC_TestREG(RTC_REG_AL1_SEC + i, 0x80) << i;
    }
    return alarm_mask;
}

/**
 * @brief  修改闹钟1运行模式。
 * @param  alarm_mask 运行模式，从左往右分别为：0 0 0 0 A1M4 A1M3 A1M2 A1M1 。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyAM1Mask(uint8_t alarm_mask)
{
    uint8_t i, ret;
    for (i = 0; i < 4; i++)
    {
        if ((alarm_mask & (0x01 << i)) != 0)
        {
            ret = RTC_ModifyREG(RTC_REG_AL1_SEC + i, 0x80, 0x80);
        }
        else
        {
            ret = RTC_ModifyREG(RTC_REG_AL1_SEC + i, 0x80, 0x00);
        }
    }
    if (ret != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  获取闹钟2运行模式。
 * @return 运行模式，从左往右分别为：0 0 0 0 DY A2M4 A2M3 A2M2 。
 */
uint8_t RTC_GetAM2Mask(void)
{
    uint8_t i, alarm_mask;

    alarm_mask = 0x00;
    if (RTC_TestREG(RTC_REG_AL2_DDT, 0x40) != 0)
    {
        alarm_mask |= 0x08;
    }
    for (i = 0; i < 3; i++)
    {
        alarm_mask |= RTC_TestREG(RTC_REG_AL2_MIN + i, 0x80) << i;
    }
    return alarm_mask;
}

/**
 * @brief  修改闹钟2运行模式。
 * @param  alarm_mask 运行模式，从左往右分别为：0 0 0 0 0 A2M4 A2M3 A2M2 。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyAM2Mask(uint8_t alarm_mask)
{
    uint8_t i, ret;
    for (i = 0; i < 3; i++)
    {
        if ((alarm_mask & (0x01 << i)) != 0)
        {
            ret = RTC_ModifyREG(RTC_REG_AL2_MIN + i, 0x80, 0x80);
        }
        else
        {
            ret = RTC_ModifyREG(RTC_REG_AL2_MIN + i, 0x80, 0x00);
        }
    }
    if (ret != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  获取振荡器是否开启。
 * @return 1：振荡器已关闭，0：振荡器已开启。
 */
uint8_t RTC_GetEOSC(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x80);
}

/**
 * @brief  修改振荡器开启状态。
 * @param  eosc 振荡器开启状态（1：关闭振荡器，0：开启振荡器）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyEOSC(uint8_t eosc)
{
    if (eosc != 0)
    {
        eosc = 0x80;
    }
    else
    {
        eosc = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x80, eosc);
}

/**
 * @brief  获取电池备份方波输出是否开启。
 * @return 1：电池备份方波输出已开启，0：电池备份方波输出已关闭。
 */
uint8_t RTC_GetBBSQW(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x40);
}

/**
 * @brief  修改电池备份方波输出开启状态。
 * @param  bbsqw 电池备份方波输出开启状态（1：开启电池备份方波输出，0：关闭电池备份方波输出）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyBBSQW(uint8_t bbsqw)
{
    if (bbsqw != 0)
    {
        bbsqw = 0x40;
    }
    else
    {
        bbsqw = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x40, bbsqw);
}

/**
 * @brief  获取手动温度转换是否正在进行。
 * @return 1：手动温度转换正在进行，0：手动温度转换已完成。
 */
uint8_t RTC_GetCONV(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x20);
}

/**
 * @brief  修改手动温度转换开启状态。
 * @param  conv 手动温度转换开启状态（1：开启手动温度转换，0：关闭手动温度转换）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyCONV(uint8_t conv)
{
    if (conv != 0)
    {
        conv = 0x20;
    }
    else
    {
        conv = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x20, conv);
}

/**
 * @brief  获取方波输出频率。
 * @return 3：8.192kHz，2：4.096kHz，1：1.024kHz，0：1.000kHz。
 */
uint8_t RTC_GetRS(void)
{
    return (RTC_ReadREG(RTC_REG_CTL) & 0x18) >> 3;
}

/**
 * @brief  修改方波输出频率。
 * @param  rs 方波输出频率（3：8.192kHz，2：4.096kHz，1：1.024kHz，0：1.000kHz）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyRS(uint8_t rs)
{
    return RTC_ModifyREG(RTC_REG_CTL, 0x18, (rs & 0x03) << 3);
}

/**
 * @brief  获取方波输出/中断输出复用状态。
 * @return 1：中断输出，0：方波输出。
 */
uint8_t RTC_GetINTCN(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x04);
}

/**
 * @brief  修改方波输出/中断输出状态。
 * @param  intcn 方波输出/中断输出状态（1：中断输出，0：方波输出）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyINTCN(uint8_t intcn)
{
    if (intcn != 0)
    {
        intcn = 0x04;
    }
    else
    {
        intcn = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x04, intcn);
}

/**
 * @brief  获取闹钟2中断开启状态。
 * @return 1：中断开启，0：中断关闭。
 */
uint8_t RTC_GetA2IE(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x02);
}

/**
 * @brief  修改闹钟2中断开启状态。
 * @param  a2ie 闹钟2中断开启状态（1：中断开启，0：中断关闭）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyA2IE(uint8_t a2ie)
{
    if (a2ie != 0)
    {
        a2ie = 0x02;
    }
    else
    {
        a2ie = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x02, a2ie);
}

/**
 * @brief  获取闹钟1中断开启状态。
 * @return 1：中断开启，0：中断关闭。
 */
uint8_t RTC_GetA1IE(void)
{
    return RTC_TestREG(RTC_REG_CTL, 0x02);
}

/**
 * @brief  修改闹钟1中断开启状态。
 * @param  a1ie 闹钟1中断开启状态（1：中断开启，0：中断关闭）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyA1IE(uint8_t a1ie)
{
    if (a1ie != 0)
    {
        a1ie = 0x01;
    }
    else
    {
        a1ie = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_CTL, 0x01, a1ie);
}

/**
 * @brief  获取振荡器停止历史状态。
 * @return 1：振荡器停止（时间停止），0：振荡器未停止。
 */
uint8_t RTC_GetOSF(void)
{
    return RTC_TestREG(RTC_REG_STA, 0x80);
}

/**
 * @brief  清除振荡器停止历史状态（置0）。
 * @return 1：清除失败，0：清除成功。
 */
uint8_t RTC_ClearOSF(void)
{
    return RTC_ModifyREG(RTC_REG_STA, 0x80, 0x00);
}

/**
 * @brief  获取32kHz输出开启状态。
 * @return 1：32kHz输出开启，0：32kHz输出关闭。
 */
uint8_t RTC_GetEN32KHZ(void)
{
    return RTC_TestREG(RTC_REG_STA, 0x08);
}

/**
 * @brief  修改32kHz输出开启状态。
 * @param  en32khz 32kHz输出开启状态（1：32kHz输出开启，0：32kHz输出关闭）。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyEN32KHZ(uint8_t en32khz)
{
    if (en32khz != 0)
    {
        en32khz = 0x08;
    }
    else
    {
        en32khz = 0x00;
    }
    return RTC_ModifyREG(RTC_REG_STA, 0x08, en32khz);
}

/**
 * @brief  获取温度转换运行状态。
 * @return 1：温度转换正在运行，0：温度转换已停止。
 */
uint8_t RTC_GetBUSY(void)
{
    return RTC_TestREG(RTC_REG_STA, 0x04);
}

/**
 * @brief  获取闹钟2触发状态。
 * @return 1：闹钟2已触发，0：闹钟2未触发。
 */
uint8_t RTC_GetA2F(void)
{
    return RTC_TestREG(RTC_REG_STA, 0x02);
}

/**
 * @brief  清除闹钟2触发状态。
 * @return 1：清除失败，0：清除成功。
 */
uint8_t RTC_ClearA2F(void)
{
    return RTC_ModifyREG(RTC_REG_STA, 0x02, 0x00);
}

/**
 * @brief  获取闹钟1触发状态。
 * @return 1：闹钟1已触发，0：闹钟1未触发。
 */
uint8_t RTC_GetA1F(void)
{
    return RTC_TestREG(RTC_REG_STA, 0x01);
}

/**
 * @brief  清除闹钟1触发状态。
 * @return 1：清除失败，0：清除成功。
 */
uint8_t RTC_ClearA1F(void)
{
    return RTC_ModifyREG(RTC_REG_STA, 0x01, 0x00);
}

/**
 * @brief  获取老化寄存器数值。
 * @return 老化寄存器数值。
 */
int8_t RTC_GetAging(void)
{
    return (int8_t)RTC_ReadREG(RTC_REG_AGI);
}

/**
 * @brief  修改老化寄存器数值。
 * @param  aging 老化寄存器数值。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ModifyAging(int8_t aging)
{
    return RTC_WriteREG(RTC_REG_AGI, (uint8_t)aging);
}

/**
 * @brief  获取温度数值。
 * @return 温度数值。
 */
float RTC_GetTemp(void)
{
    uint16_t temp_tmp;
    temp_tmp = ((uint16_t)RTC_ReadREG(RTC_REG_TPM)) << 2;
    temp_tmp |= RTC_ReadREG(RTC_REG_TPL) >> 6;
    if ((temp_tmp & 0x0200) != 0)
    {
        temp_tmp &= 0x01FF;        /* 去除负号标志 */
        temp_tmp ^= 0x01FF;        /* 转正数 */
        temp_tmp += 1;             /* 转正数 */
        return -(temp_tmp * 0.25); /* 计算温度并变为负数 */
    }
    else
    {
        return temp_tmp * 0.25; /* 计算温度 */
    }
}

/**
 * @brief  将所有寄存器修改到默认状态。
 * @return 1：修改失败，0：修改成功。
 */
uint8_t RTC_ResetAllRegToDefault(void)
{
    const uint8_t default_reg[17] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 时间寄存器 */
        0x00, 0x00, 0x00, 0x00,                   /* 闹钟1寄存器 */
        0x00, 0x00, 0x00,                         /* 闹钟2寄存器 */
        0x1C, 0x8B, 0x00                          /* 控制寄存器 */
    };
    return RTC_WriteREG_Multi(RTC_REG_SEC, sizeof(default_reg), default_reg);
}

#include "rtc.h"

uint8_t RTC_ReadREG(uint8_t reg)
{
    uint8_t read_temp;

    if (I2C_Start(RTC_I2C_ADDR, 1, 0) != 0 ||
        I2C_WriteByte(reg) != 0 ||
        I2C_Start(RTC_I2C_ADDR, 1, 1) != 0)
    {
        return 0x00;
    }
    read_temp = I2C_ReadByte();
    I2C_Stop();
    return read_temp;
}

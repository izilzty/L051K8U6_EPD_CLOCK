#include "iic.h"

/**
 * @brief  延时100ns的倍数（不准确，只是大概）。
 * @param  nsX100 延时时间。
 */
static void delay_100ns(volatile uint16_t nsX100)
{
    while (nsX100)
    {
        nsX100--;
    }
    ((void)nsX100);
}

/**
 * @brief  尝试清除I2C死锁状态。
 * @return 1：未成功清除死锁，0：已成功清除死锁。
 */
static uint8_t i2c_reset(void)
{
    uint8_t i;

    LL_I2C_Disable(I2C_PORT); /* 软复位 */
    for (i = 0; i < 10; i++)  /* 等待软复位 */
    {
        if (LL_I2C_IsEnabled(I2C_PORT) == 0)
        {
            break;
        }
    }
    LL_I2C_DeInit(I2C_PORT); /* 释放I2C占用的IO */

    /* 切换数据IO到开漏输出 */
    LL_GPIO_SetPinMode(I2C_SDA_PORT, I2C_SDA_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(I2C_SDA_PORT, I2C_SDA_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinPull(I2C_SDA_PORT, I2C_SDA_PIN, LL_GPIO_PULL_UP);

    /* 切换时钟IO到开漏输出 */
    LL_GPIO_SetPinMode(I2C_SCL_PORT, I2C_SCL_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(I2C_SCL_PORT, I2C_SCL_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinPull(I2C_SCL_PORT, I2C_SCL_PIN, LL_GPIO_PULL_UP);

    /* 设置IO默认状态 */
    LL_GPIO_SetOutputPin(I2C_SDA_PORT, I2C_SDA_PIN);
    LL_GPIO_SetOutputPin(I2C_SCL_PORT, I2C_SCL_PIN);
    delay_100ns(200);

    if (LL_GPIO_IsInputPinSet(I2C_SDA_PORT, I2C_SDA_PIN) == 0) /* 检测I2C是否已释放，如未释放则代表I2C未恢复，继续处理 */
    {
        for (i = 0; i < 255; i++) /* 在时钟线上发送脉冲，用来跳过现有数据 */
        {
            LL_GPIO_ResetOutputPin(I2C_SCL_PORT, I2C_SCL_PIN);
            delay_100ns(200);
            LL_GPIO_SetOutputPin(I2C_SCL_PORT, I2C_SCL_PIN);
            delay_100ns(200);
            if (LL_GPIO_IsInputPinSet(I2C_SDA_PORT, I2C_SDA_PIN) != 0) /* 数据线为高 */
            {
                i = 0; /* I2C数据线已被释放 */
                break;
            }
        }
        LL_GPIO_ResetOutputPin(I2C_SDA_PORT, I2C_SDA_PIN); /* 不管数据线是否为高，都发送一次停止和结束标志 */
        LL_mDelay(0);
        LL_GPIO_SetOutputPin(I2C_SDA_PORT, I2C_SDA_PIN);
        LL_mDelay(0);
    }
    else
    {
        i = 0; /* I2C数据线已被释放 */
    }

    I2C_INIT_FUNC(); /* 不管有没有成功恢复，都重新初始化I2C */

    if (i != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  产生启动或重新启动标志，开始数据传输。
 * @param  addr I2C设备地址。
 * @param  data_size 要传输的数据大小。
 * @param  is_read 本次传输是读取数据，不是写入数据。
 * @return 3：发送开始传输信号后收到了NACK，2：I2C从机未响应或发生死锁，未恢复，1：I2C从机未响应或发生死锁，需要重新执行，0：传输已启动。
 */
uint8_t I2C_Start(uint8_t addr, uint8_t data_size, uint8_t is_read)
{
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    if (LL_I2C_IsEnabled(I2C_PORT) == 0)
    {
        LL_I2C_Enable(I2C_PORT);
    }
    /* DocID025942 Rev 8 - Page 604 */
    LL_I2C_SetMasterAddressingMode(I2C_PORT, LL_I2C_ADDRESSING_MODE_7BIT);
    LL_I2C_SetSlaveAddr(I2C_PORT, addr);
    if (is_read == 0)
    {
        LL_I2C_SetTransferRequest(I2C_PORT, LL_I2C_REQUEST_WRITE);
    }
    else
    {
        LL_I2C_SetTransferRequest(I2C_PORT, LL_I2C_REQUEST_READ);
    }
    LL_I2C_DisableReloadMode(I2C_PORT);
    LL_I2C_SetTransferSize(I2C_PORT, data_size);
    /* DocID025942 Rev 8 - Page 606 */
    LL_I2C_DisableAutoEndMode(I2C_PORT);
    LL_I2C_GenerateStartCondition(I2C_PORT);
    if (is_read == 0)
    {
        while (LL_I2C_IsActiveFlag_TXE(I2C_PORT) == 0 && timeout != 0)
        {
            timeout -= 1;
            if (LL_I2C_IsActiveFlag_NACK(I2C_PORT) != 0)
            {
                LL_I2C_ClearFlag_NACK(I2C_PORT);
                return 3;
            }
        }
    }
    else
    {
        while (LL_I2C_IsActiveFlag_RXNE(I2C_PORT) == 0 && timeout != 0)
        {
            timeout -= 1;
            if (LL_I2C_IsActiveFlag_NACK(I2C_PORT) != 0)
            {
                LL_I2C_ClearFlag_NACK(I2C_PORT);
                return 3;
            }
        }
    }
    
    if (timeout == 0)
    {
        if (i2c_reset() == 0)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    return 0;
}

uint8_t I2C_Stop(void)
{
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    LL_I2C_GenerateStopCondition(I2C_PORT);
    while (LL_I2C_IsActiveFlag_BUSY(I2C_PORT) != 0 && timeout != 0)
    {
        timeout -= 1;
    }
    if (timeout == 0)
    {
        if (i2c_reset() == 0)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    return 0;
}

uint8_t I2C_WriteByte(uint8_t byte)
{
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    LL_I2C_TransmitData8(I2C_PORT, byte);
    while (LL_I2C_IsActiveFlag_TXE(I2C_PORT) == 0 && timeout != 0)
    {
        timeout -= 1;
    }
    if (timeout == 0)
    {
        if (i2c_reset() == 0)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    return 0;
}

uint8_t I2C_ReadByte(void)
{
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    while (LL_I2C_IsActiveFlag_RXNE(I2C_PORT) == 0 && timeout != 0)
    {
        timeout -= 1;
    }
    if (timeout == 0)
    {
        i2c_reset();
        return 0x00;
    }
    return LL_I2C_ReceiveData8(I2C_PORT);
}

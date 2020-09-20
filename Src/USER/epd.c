#include "epd.h"

/* 全屏刷新LUT */
static const uint8_t LUT_Full[30] = {
    0x00, 0x00, 0xA6, 0x65, 0x66,
    0x6A, 0x9A, 0x98, 0x66, 0x64,
    0x66, 0x00, 0x55, 0x99, 0x11,
    0x88, 0x11, 0x88, 0x11, 0x88,

    0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x2F, 0xFF, 0xFF, 0xFF, 0xFF};

/* 局部刷新LUT */
static const uint8_t LUT_Part[30] = {
    0x10, 0x18, 0x18, 0x08, 0x18,
    0x18, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    0x13, 0x14, 0x44, 0x12, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00};

/* 快速全屏刷新LUT */
static const uint8_t LUT_Fast[30] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x66, 0x64,
    0x66, 0x00, 0x55, 0x99, 0x11,
    0x88, 0x11, 0x88, 0x11, 0x88,

    0x00, 0x00, 0x00, 0x00, 0x77,
    0x17, 0x77, 0x77, 0x77, 0x77};

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
 * @brief  使用硬件SPI发送指定大小的数据。
 * @param  tx_data 要发送数据的指针。
 * @param  data_size 要发送数据的大小。
 * @return 1：传输超时，0：传输完成。
 */
static uint8_t spi_send_data(const uint8_t *tx_data, uint16_t data_size)
{
    uint32_t timeout;
    LL_SPI_ClearFlag_OVR(EPD_SPI);
    while (data_size--)
    {
        timeout = SPI_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
        while (LL_SPI_IsActiveFlag_TXE(EPD_SPI) == RESET && timeout != 0)
        {
            timeout -= 1;
        }
        if (timeout == 0)
        {
            return 1;
        }
        LL_SPI_TransmitData8(EPD_SPI, *tx_data);
        tx_data += 1;
    }
    timeout = SPI_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    while (LL_SPI_IsActiveFlag_BSY(EPD_SPI) == SET && timeout != 0)
    {
        timeout -= 1;
    }
    if (timeout == 0 || LL_SPI_IsActiveFlag_OVR(EPD_SPI) != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  向EPD控制器发送命令。
 * @param  cmd 要发送的命令。
 */
static void epd_send_cmd(uint8_t cmd)
{
    LL_GPIO_ResetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_ResetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    delay_100ns(1);
    spi_send_data((uint8_t *)&cmd, 1);
    delay_100ns(1);
    LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
}

/**
 * @brief  向EPD控制器发送数据。
 * @param  data 要发送的数据。
 */
static void epd_send_data(uint8_t data)
{
    LL_GPIO_SetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_ResetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    delay_100ns(1);
    spi_send_data((uint8_t *)&data, 1);
    delay_100ns(1);
    LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
}

/**
 * @brief  向EPD控制器发送指定大小的数据。
 * @param  data 要发送数据的指针。
 * @param  data_size 要发送数据的大小。
 */
static void epd_send_data_multi(const uint8_t *data, uint16_t data_size)
{
    LL_GPIO_SetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_ResetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    delay_100ns(1);
    spi_send_data(data, data_size);
    delay_100ns(1);
    LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
}

/**
 * @brief  等待EPD控制器空闲。
 * @return 1：等待超时，0：EPD控制器空闲。
 */
static uint8_t epd_wait_busy(void)
{
    uint16_t timeout;
    LL_GPIO_ResetOutputPin(EPD_LED_PORT, EPD_LED_PIN);
    delay_100ns(10); /* 1us 未要求时间，短暂延时 */
    timeout = EPD_TIMEOUT_MS;
    while (LL_GPIO_IsInputPinSet(EPD_BUSY_PORT, EPD_BUSY_PIN) && timeout != 0)
    {
        timeout -= 1;
        LL_mDelay(0); /* 1ms */
    }
    LL_GPIO_SetOutputPin(EPD_LED_PORT, EPD_LED_PIN);
    if (timeout == 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  设置EPD显示数据指针位置。
 * @param  x 显示数据指针起始X位置。
 * @param  y_x8 显示数据指针起始Y位置，设置1等于8像素。
 */
void EPD_SetCursor(uint16_t x, uint8_t y_x8)
{
    epd_send_cmd(0x4E); /* 设置X（短边）地址计数器 */
    epd_send_data(y_x8);
    epd_send_cmd(0x4F); /* 设置Y（长边）地址计数器 */
    epd_send_data(x & 0xFF);
    epd_send_data((x >> 8) & 0x01);
}

/**
 * @brief  设置EPD显示窗口位置和大小。
 * @param  x 显示窗口起始X位置。
 * @param  y_x8 显示窗口起始Y位置，设置1等于8像素。
 * @param  x_size 显示窗口X方向大小。
 * @param  y_size_x8 显示窗口Y方向大小，设置1等于8像素。
 */
void EPD_SetWindow(uint16_t x, uint8_t y_x8, uint16_t x_size, uint8_t y_size_x8)
{
    x = 296 - 1 - x;
    x_size = x - x_size + 1;          /* x_size已变为x结束地址 */
    y_size_x8 = y_size_x8 + y_x8 - 1; /* y_size已变为y结束地址 */

    epd_send_cmd(0x44); /* 设置X（短边）起始地址和结束地址，根据扫描方式不同，地址设置也不同 */
    epd_send_data(y_x8);
    epd_send_data(y_size_x8 & 0x1F);
    epd_send_cmd(0x45); /* 设置Y（长边）起始地址和结束地址，根据扫描方式不同，地址设置也不同 */
    epd_send_data(x & 0xFF);
    epd_send_data((x >> 8) & 0x01);
    epd_send_data(x_size & 0xFF);
    epd_send_data((x_size >> 8) & 0x01);

    EPD_SetCursor(x, y_x8);
}

/**
 * @brief  清除EPD控制器内全部显示RAM。
 * @note   执行完成后窗口会恢复至全屏幕。
 */
void EPD_ClearRAM(void)
{
    uint16_t i;

    EPD_SetWindow(0, 0, 296, 16);
    epd_send_cmd(0x24);
    for (i = 0; i < 4736; i++)
    {
        epd_send_data(0xFF);
    }
    EPD_SetWindow(0, 0, 296, 16);
}

/**
 * @brief  向EPD控制器发送指定大小的显示数据。
 * @param  data 要发送数据的指针。
 * @param  data_size 要发送数据的大小。
 */
void EPD_SendRAM(const uint8_t *data, uint16_t data_size)
{
    epd_send_cmd(0x24);
    epd_send_data_multi(data, data_size);
}

/**
 * @brief  更新EPD显示，并等待更新完成。
 * @param  wait 是否等待显示更新完成。
 * @return 1：等待超时，0：更新完成。
 */
uint8_t EPD_Show(uint8_t wait_busy)
{
    epd_send_cmd(0x22);
    epd_send_data(0xC4);
    epd_send_cmd(0x20);
    if (wait_busy != 0)
    {
        return epd_wait_busy();
    }
    return 0;
}

/**
 * @brief  进入普通睡眠模式。
 * @note   仅关闭DC-DC转换器，下次使用不需要重新初始化，显示RAM内数据会保持。
 * @note   下次调用EPD_Show()时会自动退出睡眠模式。
 */
void EPD_EnterSleep(void)
{
    epd_send_cmd(0x22);
    epd_send_data(0x02);
    epd_send_cmd(0x20);
    epd_wait_busy();
}

/**
 * @brief  进入深度睡眠模式。
 * @note   下次使用需要重新初始化，显示RAM内数据会丢失。
 */
void EPD_EnterDeepSleep(void)
{
    epd_send_cmd(0x22);
    epd_send_data(0x03);
    epd_send_cmd(0x20);
    epd_wait_busy();
    epd_send_cmd(0x10);
    epd_send_data(0x01);
}

/**
 * @brief  EPD初始化。
 * @param  update_mode 显示更新模式，可设置为：EPD_UPDATE_MODE_FULL、EPD_UPDATE_MODE_PART、EPD_UPDATE_MODE_FAST。
 */
void EPD_Init(uint8_t update_mode)
{
    if (LL_SPI_IsEnabled(EPD_SPI) == 0)
    {
        LL_SPI_Enable(EPD_SPI);
    }

    /* 为防止电流泄漏，系统复位后IO初始化时所有IO均为低电平，使用前需要先置高 */
    if (LL_GPIO_IsOutputPinSet(EPD_CS_PORT, EPD_CS_PIN) == 0 ||
        LL_GPIO_IsOutputPinSet(EPD_DC_PORT, EPD_DC_PIN) == 0 ||
        LL_GPIO_IsOutputPinSet(EPD_RST_PORT, EPD_RST_PIN) == 0)
    {
        LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
        LL_GPIO_SetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
        LL_GPIO_SetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
        delay_100ns(100); /* 10us，未要求，短暂延时 */
    }

    LL_GPIO_ResetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    LL_mDelay(0); /* 1ms，未要求，短暂延时 */
    LL_GPIO_SetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    LL_mDelay(0); /* 1ms，未要求，短暂延时 */

    if (update_mode != EPD_UPDATE_MODE_PART) /* 局部刷新需要上次的旧RAM数据（自动保存在控制器里），不能执行软复位和进入DeepSleep模式 */
    {
        epd_send_cmd(0x12);
        epd_wait_busy();
    }

    epd_send_cmd(0x01);
    epd_send_data(0x27);
    epd_send_data(0x01);
    epd_send_data(0x00);

    epd_send_cmd(0x0C);
    epd_send_data(0xD7);
    epd_send_data(0xD6);
    epd_send_data(0x9D);

    epd_send_cmd(0x11);
    epd_send_data(0x01);

    epd_send_cmd(0x2C); /* VCOM */
    epd_send_data(0x9A);

    epd_send_cmd(0x3A);
    epd_send_data(0x1A);

    epd_send_cmd(0x3B);
    epd_send_data(0x08);

    if (update_mode != EPD_UPDATE_MODE_PART)
    {
        epd_send_cmd(0x3C);
        epd_send_data(0x33);
    }
    else
    {
        epd_send_cmd(0x3C);
        epd_send_data(0x01);
    }

    switch (update_mode)
    {
    case EPD_UPDATE_MODE_FULL:
        epd_send_cmd(0x32);
        epd_send_data_multi(LUT_Full, sizeof(LUT_Full));
        break;
    case EPD_UPDATE_MODE_PART:
        epd_send_cmd(0x32);
        epd_send_data_multi(LUT_Part, sizeof(LUT_Part));
        break;
    case EPD_UPDATE_MODE_FAST:
        epd_send_cmd(0x32);
        epd_send_data_multi(LUT_Fast, sizeof(LUT_Fast));
        break;
    }

    EPD_ClearRAM();
}

/**
 * @brief  绘制UTF8字符串。
 * @param  x 绘制起始X位置。
 * @param  y_x8 绘制起始Y位置，设置1等于8像素。
 * @param  gap 字符间额外间距。
 * @param  str 要绘制的字符串指针。
 * @param  ascii_font ASCII字符字模指针。
 * @param  utf8_font UTF8字符字模指针。
 */
void EPD_DrawUTF8(uint16_t x, uint8_t y_x8, uint8_t gap, const char *str, const uint8_t *ascii_font, const uint8_t *utf8_font)
{
    uint8_t i, utf8_size;
    uint16_t x_count, font_size;
    uint32_t unicode, unicode_temp;
    const uint8_t *ascii_base_addr;

    x_count = 0;
    while (*str != '\0')
    {
        if (ascii_font != NULL && (*str & 0x80) == 0x00) /* 普通ASCII字符 */
        {
            if (*str != ' ')
            {
                font_size = ascii_font[1] * ascii_font[2] / 8;
                ascii_base_addr = ascii_font + (*str - ascii_font[0]) * font_size + 4;
                if (ascii_base_addr + font_size <= ascii_font + 4 + font_size * ascii_font[3]) /* 限制数组范围 */
                {
                    EPD_SetWindow(x + x_count, y_x8, ascii_font[1], ascii_font[2] / 8);
                    EPD_SendRAM(ascii_base_addr, font_size);
                }
            }
            x_count += ascii_font[1] + gap;
        }
        else if (utf8_font != NULL) /* UTF8字符 */
        {
            unicode = 0x000000;
            utf8_size = 0;
            for (i = 0; i < 5; i++)
            {
                if (*str & (0x80 >> i))
                {
                    utf8_size += 1;
                }
                else
                {
                    break;
                }
            }
            switch (utf8_size)
            {
            case 2:
                if (*(str + 1) != '\0')
                {
                    unicode = (*str & 0x1F) << 6;
                    str += 1;
                    unicode |= *str & 0x3F;
                }
                break;
            case 3:
                if (*(str + 1) != '\0' && *(str + 2) != '\0')
                {
                    unicode = (*str & 0x0F) << 12;
                    str += 1;
                    unicode |= (*str & 0x3F) << 6;
                    str += 1;
                    unicode |= *str & 0x3F;
                }
                break;
            case 4:
                if (*(str + 1) != '\0' && *(str + 2) != '\0' && *(str + 3) != '\0')
                {
                    unicode = (*str & 0x07) << 18;
                    str += 1;
                    unicode |= (*str & 0x3F) << 12;
                    str += 1;
                    unicode |= (*str & 0x3F) << 6;
                    str += 1;
                    unicode |= *str & 0x3F;
                }
                break;
            }
            if (unicode != 0)
            {
                font_size = utf8_font[1] * utf8_font[2] / 8;
                for (i = 0; i < utf8_font[3]; i++) /* 限制数组范围 */
                {
                    unicode_temp = utf8_font[4 + (font_size + 3) * i] << 16;
                    unicode_temp |= utf8_font[5 + (font_size + 3) * i] << 8;
                    unicode_temp |= utf8_font[6 + (font_size + 3) * i];
                    if (unicode_temp == unicode)
                    {
                        EPD_SetWindow(x + x_count, y_x8, utf8_font[1], utf8_font[2] / 8);
                        EPD_SendRAM(utf8_font + 7 + (font_size + 3) * i, font_size);
                        break;
                    }
                }
            }
            x_count += utf8_font[1] + gap;
        }
        str += 1;
    }
}

/**
 * @brief  绘制水平直线。
 * @param  x 绘制起始X位置。
 * @param  y 绘制起始Y位置。
 * @param  x_size 绘制长度。
 * @param  width 线宽度。
 * @note   Y方向始终会占用8的倍数的像素，例如在0,0位置绘制一条1像素宽高的线，会清除Y方向8像素内的显示数据。
 */
void EPD_DrawHLine(uint16_t x, uint8_t y, uint16_t x_size, uint8_t width)
{
    uint16_t i;
    uint8_t j, k, block, height, y_temp, width_temp;

    height = (y % 8 + width - 1) / 8 + 1;

    EPD_SetWindow(x, y / 8, x_size, height);

    for (i = 0; i < x_size; i++)
    {
        y_temp = y;
        width_temp = width;
        for (j = 0; j < height; j++)
        {
            block = 0x00;
            for (k = (y_temp % 8); k < 8; k++)
            {
                block |= 0x80 >> k;
                width_temp -= 1;
                if (width_temp <= 0)
                {
                    break;
                }
            }
            y_temp = 0;
            block = ~block;
            EPD_SendRAM((uint8_t *)&block, 1);
        }
    }
}

/**
 * @brief  绘制垂直直线。
 * @param  x 绘制起始X位置。
 * @param  y 绘制起始Y位置。
 * @param  y_size 绘制长度。
 * @param  width 线宽度。
 * @note   Y方向始终会占用8的倍数的像素，例如在0,0位置绘制一条1像素宽高的线，会清除Y方向8像素内的显示数据。
 */
void EPD_DrawVLine(uint16_t x, uint8_t y, uint8_t y_size, uint16_t width)
{
    uint16_t i;
    uint8_t j, k, block, height, y_temp, width_temp;

    height = (y % 8 + y_size - 1) / 8 + 1;

    EPD_SetWindow(x, y / 8, width, height);

    for (i = 0; i < width; i++)
    {
        y_temp = y;
        width_temp = y_size;
        for (j = 0; j < height; j++)
        {
            block = 0x00;
            for (k = (y_temp % 8); k < 8; k++)
            {
                block |= 0x80 >> k;
                width_temp -= 1;
                if (width_temp <= 0)
                {
                    break;
                }
            }
            y_temp = 0;
            block = ~block;
            EPD_SendRAM((uint8_t *)&block, 1);
        }
    }
}

#include "serial.h"
#include <stdio.h>

#define WAIT_TIMEOUT(val)                                       \
    timeout = SERIAL_TIMEOUT_MS;                                \
    systick_tmp = SysTick->CTRL;                                \
    ((void)systick_tmp);                                        \
    while (timeout != 0 && val)                                 \
    {                                                           \
        if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U) \
        {                                                       \
            timeout -= 1;                                       \
        }                                                       \
    }                                                           \
    if (timeout == 0)                                           \
    {                                                           \
        return;                                                 \
    }

/**
 * @brief  从串口发送指定大小的数据
 * @param  tx_data  要发送的数据指针
 * @param  data_size 要发送的数据大小
 */
void SERIAL_SendData(const uint8_t *tx_data, uint32_t data_size)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    while (data_size != 0)
    {
        WAIT_TIMEOUT(LL_USART_IsActiveFlag_TXE(SERIAL_NUM) == 0);
        LL_USART_ClearFlag_TC(SERIAL_NUM);
        LL_USART_TransmitData8(SERIAL_NUM, *tx_data);
        tx_data += 1;
        data_size -= 1;
    }
    WAIT_TIMEOUT(LL_USART_IsActiveFlag_TC(SERIAL_NUM) == 0);
}

/**
 * @brief  从串口发送字符串
 * @param  tx_char  要发送的字符串
 */
void SERIAL_SendString(const char *tx_char)
{
    uint8_t *char_ptr;
    uint8_t char_size;

    char_size = 0;
    char_ptr = (uint8_t *)tx_char;
    while (*char_ptr != '\0' && char_size < 255)
    {
        char_ptr += 1;
        char_size += 1;
    }
    SERIAL_SendData((uint8_t *)tx_char, char_size);
}

/**
 * @brief  从串口发送字符串，附加换行符
 * @param  tx_char  要发送的字符串
 */
void SERIAL_SendStringRN(const char *tx_char)
{
    SERIAL_SendString(tx_char);
    SERIAL_SendData((uint8_t *)"\r\n", 2);
}

/**
 * @brief  从串口发送调试打印信息
 * @param  file_name  当前程序文件路径字符串指针
 * @param  func_name  当前函数名称字符串指针
 * @param  func_line  当前函数所在行
 * @param  info_str  自定义信息字符串
 */
void _SERIAL_DebugPrint(const char *file_name, const char *func_name, uint32_t func_line, const char *info_str)
{
    char text[11];
    SERIAL_SendString("\r\n**DEBUG PRINT");
    SERIAL_SendString("\r\nFILE  : ");
    SERIAL_SendString(file_name);
    SERIAL_SendString("\r\nFUNC  : ");
    SERIAL_SendString(func_name);
    SERIAL_SendString("\r\nLINE  : ");
    snprintf(text, sizeof(text), "%d", func_line);
    SERIAL_SendString(text);
    SERIAL_SendString("\r\nINFO  : ");
    SERIAL_SendString(info_str);
    SERIAL_SendString("\r\n\r\n");
}

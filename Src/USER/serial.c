#include "serial.h"

/**
 * @brief  从串口发送指定大小的数据
 * @param  tx_data  要发送的数据指针
 * @param  data_size 要发送的数据大小
 */
void USART_SendData(const uint8_t *tx_data, uint32_t data_size)
{
    while (data_size != 0)
    {
        while (LL_USART_IsActiveFlag_TXE(SERIAL_USART) == RESET)
        {
        }
        LL_USART_ClearFlag_TC(SERIAL_USART);
        LL_USART_TransmitData8(SERIAL_USART, *tx_data);
        tx_data += 1;
        data_size -= 1;
    }
    while (LL_USART_IsActiveFlag_TC(SERIAL_USART) == RESET)
    {
    }
}

/**
 * @brief  从串口发送字符串
 * @param  tx_char  要发送的字符串
 */
void USART_SendString(const char *tx_char)
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
    USART_SendData((uint8_t *)tx_char, char_size);
}

/**
 * @brief  从串口发送字符串，附加换行符
 * @param  tx_char  要发送的字符串
 */
void USART_SendStringRN(const char *tx_char)
{
    USART_SendString(tx_char);
    USART_SendData((uint8_t *)"\r\n", 2);
}

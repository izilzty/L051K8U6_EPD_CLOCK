#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "main.h"

#define SERIAL_NUM USART1

#ifdef ENABLE_DEBUG_PRINT
#define USART_DebugPrint(info_str) _USART_DebugPrint(__FILE__, __FUNCTION__, __LINE__, info_str)
#else
#define USART_DebugPrint(info_str)
#endif

void USART_SendData(const uint8_t *tx_data, uint32_t data_size);
void USART_SendString(const char *tx_char);
void USART_SendStringRN(const char *tx_char);
void _USART_DebugPrint(const char *file_name, const char *func_name, uint32_t func_line, const char *info_str);

#endif

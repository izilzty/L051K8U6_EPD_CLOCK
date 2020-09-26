#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "main.h"

/* 可修改 */
#define SERIAL_NUM USART1
/* 结束 */

#define SERIAL_TIMEOUT_MS 1000

#ifdef ENABLE_DEBUG_PRINT
#define SERIAL_DebugPrint(info_str) _SERIAL_DebugPrint(__FILE__, __FUNCTION__, __LINE__, info_str)
#else
#define SERIAL_DebugPrint(info_str)
#endif

void SERIAL_SendData(const uint8_t *tx_data, uint32_t data_size);
void SERIAL_SendString(const char *tx_char);
void SERIAL_SendStringRN(const char *tx_char);
void _SERIAL_DebugPrint(const char *file_name, const char *func_name, uint32_t func_line, const char *info_str);

#endif

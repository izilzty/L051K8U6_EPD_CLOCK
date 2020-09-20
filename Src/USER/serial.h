#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "main.h"

#define SERIAL_USART USART1

void USART_SendData(const uint8_t *tx_data, uint32_t data_size);
void USART_SendString(const  char *tx_char);
void USART_SendStringRN(const char *tx_char);

#endif

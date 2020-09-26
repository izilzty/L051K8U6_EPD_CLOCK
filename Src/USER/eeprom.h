#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "main.h"

/* 可修改 */
#define EEPROM_UNLOCK_KEY1 0x89ABCDEF
#define EEPROM_UNLOCK_KEY2 0x02030405
#define EEPROM_BASE_ADDR 0x08080000
/* 结束 */

#define EEPROM_TIMEOUT_MS 100

uint8_t EEPROM_ReadByte(uint16_t addr);
uint16_t EEPROM_ReadWORD(uint16_t addr);
uint32_t EEPROM_ReadDWORD(uint16_t addr);

uint8_t EEPROM_WriteByte(uint16_t addr, uint8_t data);
uint8_t EEPROM_WriteWORD(uint16_t addr, uint16_t data);
uint8_t EEPROM_WriteDWORD(uint16_t addr, uint32_t data);

uint8_t EEPROM_EraseByte(uint16_t addr);
uint8_t EEPROM_EraseWORD(uint16_t addr);
uint8_t EEPROM_EraseDWORD(uint16_t addr);
uint16_t EEPROM_EraseRange(uint16_t start_addr_DWORD, uint16_t end_addr_DWORD);

#endif

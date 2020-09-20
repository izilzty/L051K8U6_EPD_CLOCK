#include "eeprom.h"

#define EEPROM_UNLOCK() \
    if (eeprom_unlock() != 0)  \
    {                   \
        return 1;       \
    }

#define EEPROM_LOCK() \
    if (eeprom_lock() != 0)  \
    {                 \
        return 1;     \
    }

/**
 * @brief  等待EEPROM空闲。
 * @return 1：等待超时，0：EEPROM空闲。
 */
static uint8_t eeprom_wait_busy(void)
{
    uint32_t timeout;

    timeout = EEPROM_TIMEOUT_MS / (1 / (0.00095 * HSE_VALUE));
    while ((FLASH->SR & FLASH_SR_BSY) != 0 && timeout != 0)
    {
        timeout -= 1;
    }
    if (timeout == 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  解锁EEPROM。
 * @return 1：解锁错误，0：解锁完成。
 */
static uint8_t eeprom_unlock(void)
{
    if (eeprom_wait_busy() != 0)
    {
        return 1;
    }
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0)
    {
        FLASH->PEKEYR = EEPROM_UNLOCK_KEY1;
        FLASH->PEKEYR = EEPROM_UNLOCK_KEY2;
    }
    return 0;
}

/**
 * @brief  锁定EEPROM。
 * @return 1：锁定错误，0：锁定完成。
 */
static uint8_t eeprom_lock(void)
{
    if (eeprom_wait_busy() != 0)
    {
        return 1;
    }
    FLASH->PECR |= FLASH_PECR_PELOCK;
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从指定地址读取一字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为2047。
 * @return 读取到的数据。
 */
uint8_t EEPROM_ReadByte(uint16_t addr)
{
    return *(__IO uint8_t *)(EEPROM_BASE_ADDR + addr);
}

/**
 * @brief  向指定地址写入一字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为2047。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t EEPROM_WriteByte(uint16_t addr, uint8_t data)
{
    EEPROM_UNLOCK();
    *(__IO uint8_t *)(EEPROM_BASE_ADDR + addr) = data;
    EEPROM_LOCK();
    if (*(__IO uint8_t *)(EEPROM_BASE_ADDR + addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从指定地址读取两字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为1023。
 * @return 读取到的数据。
 */
uint16_t EEPROM_ReadWORD(uint16_t addr)
{
    addr *= 2;
    return *(__IO uint16_t *)(EEPROM_BASE_ADDR + addr);
}

/**
 * @brief  向指定地址写入两字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为1023。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t EEPROM_WriteWORD(uint16_t addr, uint16_t data)
{
    addr *= 2;
    EEPROM_UNLOCK();
    *(__IO uint16_t *)(EEPROM_BASE_ADDR + addr) = data;
    EEPROM_LOCK();
    if (*(__IO uint16_t *)(EEPROM_BASE_ADDR + addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从指定地址读取四字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为511。
 * @return 读取到的数据。
 */
uint32_t EEPROM_ReadDWORD(uint16_t addr)
{
    addr *= 4;
    return *(__IO uint32_t *)(EEPROM_BASE_ADDR + addr);
}

/**
 * @brief  向指定地址写入四字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为511。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t EEPROM_WriteDWORD(uint16_t addr, uint32_t data)
{
    addr *= 4;
    EEPROM_UNLOCK();
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + addr) = data;
    EEPROM_LOCK();
    if (*(__IO uint32_t *)(EEPROM_BASE_ADDR + addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  擦除指定地址的一字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为2047。
 * @return 1：擦除错误，0：擦除完成。
 * @note   内置EEPROM不需要先擦除再写入，可以直接写入数据。
 * @note   在擦除时的内部执行顺序为：读取四字节->擦除四字节->将目标地址的数据清零->写入四字节，这样会造成写入放大，可能会有损EEPROM寿命。
 */
uint8_t EEPROM_EraseByte(uint16_t addr)
{
    uint16_t align_addr;
    uint32_t word_buffer;

    align_addr = (addr / 4) * 4;
    word_buffer = *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr);
    EEPROM_UNLOCK();
    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_DATA;
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) = 0;
    eeprom_wait_busy();
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_DATA);
    word_buffer &= ~(0x000000FF << ((addr % 4) * 8));
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) = word_buffer;
    EEPROM_LOCK();
    if (*(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) != word_buffer)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  擦除指定地址的两字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为1023。
 * @return 1：擦除错误，0：擦除完成。
 * @note   内置EEPROM不需要先擦除再写入，可以直接写入数据。
 * @note   在擦除时的内部执行顺序为：读取四字节->擦除四字节->将目标地址的数据清零->写入四字节，这样会造成写入放大，可能会有损EEPROM寿命。
 */
uint8_t EEPROM_EraseWORD(uint16_t addr)
{
    uint16_t align_addr;
    uint32_t word_buffer;

    addr *= 2;
    align_addr = (addr / 4) * 4;
    word_buffer = *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr);
    EEPROM_UNLOCK();
    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_DATA;
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) = 0;
    eeprom_wait_busy();
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_DATA);
    word_buffer &= ~(0x0000FFFF << (((addr / 2) % 2) * 16));
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) = word_buffer;
    EEPROM_LOCK();
    if (*(__IO uint32_t *)(EEPROM_BASE_ADDR + align_addr) != word_buffer)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  擦除指定地址的四字节。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为511。
 * @return 1：擦除错误，0：擦除完成。
 * @note   内置EEPROM不需要先擦除再写入，可以直接写入数据。
 */
uint8_t EEPROM_EraseDWORD(uint16_t addr)
{
    addr *= 4;
    EEPROM_UNLOCK();
    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_DATA;
    *(__IO uint32_t *)(EEPROM_BASE_ADDR + addr) = 0;
    eeprom_wait_busy();
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_DATA);
    EEPROM_LOCK();
    if (*(__IO uint32_t *)(EEPROM_BASE_ADDR + addr) != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  以四字节为单位，擦除指定地址范围内的数据。
 * @param  addr EEPROM地址，对于2K存储容量的EEPROM，起始地址为0，最大地址为511。
 * @return 大于0：擦除时出错的循环次数，减1为出错的地址，0：擦除完成。
 * @note   内置EEPROM不需要先擦除再写入，可以直接写入数据。
 */
uint16_t EEPROM_EraseRange(uint16_t start_addr_DWORD, uint16_t end_addr_DWORD)
{
    uint8_t err;
    uint16_t i;

    EEPROM_UNLOCK();
    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_DATA;
    err = 0;
    end_addr_DWORD += 1;
    for (i = start_addr_DWORD; i < end_addr_DWORD; i++)
    {
        *(__IO uint32_t *)(EEPROM_BASE_ADDR + (i * 4)) = 0;
        if (eeprom_wait_busy() != 0 || *(__IO uint32_t *)(EEPROM_BASE_ADDR + (i * 4)) != 0)
        {
            err = 1;
            break;
        }
    }
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_DATA);
    EEPROM_LOCK();
    if (err != 0)
    {
        return i + 1;
    }
    return 0;
}

#include "bkpr.h"

/**
 * @brief  从指定地址读取一字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为19。
 * @return 读取到的数据
 */
uint8_t BKPR_ReadByte(uint8_t addr)
{
    uint32_t dword_temp;

    dword_temp = *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + ((addr / 4) * 4));
    return (dword_temp >> ((addr % 4) * 8)) & 0x000000FF;
}

/**
 * @brief  向指定地址写入一字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为19。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t BKPR_WriteByte(uint8_t addr, uint8_t data)
{
    uint8_t offset;
    uint32_t align_addr, dword_temp;

    align_addr = (addr / 4) * 4;
    offset = (addr % 4) * 8;
    dword_temp = *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + align_addr);
    dword_temp &= ~(0x000000FF << offset);
    dword_temp |= (uint32_t)data << offset;
    if (LL_PWR_IsEnabledBkUpAccess() == 0)
    {
        LL_PWR_EnableBkUpAccess();
    }
    *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + align_addr) = dword_temp;
    LL_PWR_DisableBkUpAccess();
    if (BKPR_ReadByte(addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从指定地址读取两字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为9。
 * @return 读取到的数据
 */
uint16_t BKPR_ReadWORD(uint8_t addr)
{
    uint32_t dword_temp;

    dword_temp = *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + ((addr / 2) * 4));
    return (dword_temp >> ((addr % 2) * 16)) & 0x0000FFFF;
}

/**
 * @brief  向指定地址写入两字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为9。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t BKPR_WriteWORD(uint8_t addr, uint16_t data)
{
    uint8_t offset;
    uint32_t align_addr, dword_temp;

    align_addr = (addr / 2) * 4;
    offset = (addr % 2) * 16;
    dword_temp = *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + align_addr);
    dword_temp &= ~(0x0000FFFF << offset);
    dword_temp |= (uint32_t)data << offset;
    if (LL_PWR_IsEnabledBkUpAccess() == 0)
    {
        LL_PWR_EnableBkUpAccess();
    }
    *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + align_addr) = dword_temp;
    LL_PWR_DisableBkUpAccess();
    if (BKPR_ReadWORD(addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  从指定地址读取四字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为4。
 * @return 读取到的数据
 */
uint32_t BKPR_ReadDWORD(uint8_t addr)
{
    return *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + (addr * 4));
}

/**
 * @brief  向指定地址写入四字节。
 * @param  addr 备份寄存器地址，对于20Byte容量的寄存器，起始地址为0，最大地址为4。
 * @param  data 要写入的数据。
 * @return 1：写入错误，0：写入完成。
 */
uint8_t BKPR_WriteDWORD(uint8_t addr, uint32_t data)
{
    if (LL_PWR_IsEnabledBkUpAccess() == 0)
    {
        LL_PWR_EnableBkUpAccess();
    }
    *(__IO uint32_t *)(RTC_BACKUPREG_BASEADDR + (addr * 4)) = data;
    LL_PWR_DisableBkUpAccess();
    if (BKPR_ReadDWORD(addr) != data)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief  重置备份寄存器，清除所有数据。
 * @return 1：复位失败，0：复位完成。
 */
uint8_t BKPR_ResetALL(void)
{
    uint8_t i;

    if (LL_PWR_IsEnabledBkUpAccess() == 0)
    {
        LL_PWR_EnableBkUpAccess();
    }
    LL_RCC_ForceBackupDomainReset();
    LL_RCC_ReleaseBackupDomainReset();
    LL_PWR_DisableBkUpAccess();
    for (i = 0; i < 5; i++)
    {
        if (BKPR_ReadDWORD(i) != 0)
        {
            return 1;
        }
    }
    return 0;
}

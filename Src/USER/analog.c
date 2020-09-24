#include "analog.h"

#define WAIT_TIMEOUT(val)                                       \
    timeout = ANALOG_TIMEOUT_MS;                                \
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
        return 1;                                               \
    }

#define TS_CAL1_ADDR TEMPSENSOR_CAL1_ADDR
#define TS_CAL2_ADDR TEMPSENSOR_CAL2_ADDR
#define TS_VREF_MV TEMPSENSOR_CAL_VREFANALOG

#define VREFCAL_FACTORY_ADDR VREFINT_CAL_ADDR
#define VREFINT_FACTORY_ADDR VREFINT_CAL_VREF

static uint16_t VREFINT_CAL = 0;

/**
 * @brief  延时100ns的倍数（不准确，只是需大于即可）。
 * @param  nsX100 延时时间。
 */
void delay_100ns(volatile uint16_t nsX100)
{
    while (nsX100)
    {
        nsX100--;
    }
    ((void)nsX100);
}

float conv_vref_to_vdda(uint16_t vrefval)
{
    return (VREFINT_CAL_VREF * (*VREFCAL_FACTORY_ADDR) / (float)vrefval) / 1000;
}

float conv_tempv_to_temp(float vdda, uint16_t adcval)
{
    int32_t temp_tmp;
    temp_tmp = (adcval * vdda / (TS_VREF_MV / 1000)) - *TS_CAL1_ADDR;
    temp_tmp = temp_tmp * 100;
    temp_tmp = temp_tmp / (*TS_CAL2_ADDR - *TS_CAL1_ADDR);
    temp_tmp = temp_tmp + 30;
    return temp_tmp;
}

uint8_t ADC_Enable(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsEnabled(ANALOG_ADC) == 0)
    {
        /* DocID025942 Rev 8 - Page 277 */
        LL_ADC_ClearFlag_ADRDY(ANALOG_ADC);
        LL_ADC_EnableInternalRegulator(ANALOG_ADC);
        /* 
           修复CubeMX的生成BUG，修复后可去除，相同问题可在下面的链接看到：
           https://community.st.com/s/question/0D50X0000AU4CvOSQV/stm32cubemx-lladcsetcommonpathinternalch-generation-bug-only-one-internal-ch-is-enabled
           https://community.st.com/s/question/0D50X0000C20aDz/bug-cubemx-550-stm32g0-wrong-setcommonpathinternalch-initialization
        */
        LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ANALOG_ADC), LL_ADC_PATH_INTERNAL_TEMPSENSOR | LL_ADC_PATH_INTERNAL_VREFINT);
        LL_ADC_Enable(ANALOG_ADC);
        WAIT_TIMEOUT(LL_ADC_IsActiveFlag_ADRDY(ANALOG_ADC) == 0);
        delay_100ns(100); /* 10us 等待温度传感器稳定 */
    }
    return 0;
}

uint8_t ADC_Disable(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    /* DocID025942 Rev 8 - Page 276 */
    if (LL_ADC_IsEnabled(ANALOG_ADC) != 0)
    {
        if (LL_ADC_REG_IsConversionOngoing(ANALOG_ADC) != 0)
        {
            LL_ADC_REG_StopConversion(ANALOG_ADC);
            WAIT_TIMEOUT(LL_ADC_REG_IsStopConversionOngoing(ANALOG_ADC) != 0);
        }
        LL_ADC_Disable(ANALOG_ADC);
        WAIT_TIMEOUT(LL_ADC_IsEnabled(ANALOG_ADC) != 0);
        LL_ADC_ClearFlag_ADRDY(ANALOG_ADC);
        LL_ADC_DisableInternalRegulator(ANALOG_ADC);
    }
    return 0;
}

uint8_t ADC_StartCal(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsEnabled(ANALOG_ADC) != 0)
    {
        return 1;
    }
    /* DocID025942 Rev 8 - Page 277 */
    LL_ADC_ClearFlag_EOCAL(ANALOG_ADC);
    LL_ADC_StartCalibration(ANALOG_ADC);
    WAIT_TIMEOUT(LL_ADC_IsActiveFlag_EOCAL(ANALOG_ADC) == 0);
    LL_ADC_ClearFlag_EOCAL(ANALOG_ADC);
    return 0;
}

uint8_t ADC_GetCalFactor(void)
{
    return LL_ADC_GetCalibrationFactor(ANALOG_ADC) & 0x7F;
}

uint8_t ADC_StartSingleConversion(uint16_t *data, uint8_t data_size)
{
    uint8_t i;
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsActiveFlag_ADRDY(ANALOG_ADC) == 0)
    {
        return 1;
    }
    LL_ADC_ClearFlag_EOS(ANALOG_ADC);
    LL_ADC_ClearFlag_EOC(ANALOG_ADC);
    for (i = 0; i < data_size; i++)
    {
        if (LL_ADC_REG_IsConversionOngoing(ANALOG_ADC) == 0)
        {
            LL_ADC_REG_StartConversion(ANALOG_ADC);
        }
        WAIT_TIMEOUT(LL_ADC_IsActiveFlag_EOC(ANALOG_ADC) == 0);
        data[i] = LL_ADC_REG_ReadConversionData12(ANALOG_ADC) & 0x0FFF;
        LL_ADC_ClearFlag_EOC(ANALOG_ADC);
    }
    if (LL_ADC_REG_IsConversionOngoing(ANALOG_ADC) != 0)
    {
        LL_ADC_REG_StopConversion(ANALOG_ADC);
        WAIT_TIMEOUT(LL_ADC_REG_IsStopConversionOngoing(ANALOG_ADC) != 0);
    }
    LL_ADC_ClearFlag_EOS(ANALOG_ADC);
    return 0;
}

void ADC_SetRefenceValue(uint16_t vrefint)
{
    VREFINT_CAL = vrefint;
}

uint16_t ADC_GetRefenceValue(uint16_t vrefint)
{
    return vrefint;
}

float ADC_GetChannelVoltage(uint8_t channel)
{
    return 0;
}

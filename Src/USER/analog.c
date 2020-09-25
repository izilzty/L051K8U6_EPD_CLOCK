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

#define VREFCAL_FACTORY_ADDR VREFINT_CAL_ADDR
#define VREFINT_FACTORY_ADDR VREFINT_CAL_VREF

static uint16_t VREFINT_CAL = 0;

/**
 * @brief  延时100ns的倍数（不准确，只需大于即可）。
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

static float conv_vrefint_to_vdda(uint16_t vrefint_val)
{
    return (VREFINT_CAL_VREF * (*VREFCAL_FACTORY_ADDR) / (float)vrefint_val);
}

static float conv_adc_to_temp(float vdda_val, uint16_t adc_val)
{
    float temp_tmp;
    temp_tmp = (adc_val * vdda_val / (TEMPSENSOR_CAL_VREFANALOG)) - *TEMPSENSOR_CAL1_ADDR;
    temp_tmp = temp_tmp * TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP;
    temp_tmp = temp_tmp / (*TEMPSENSOR_CAL2_ADDR - *TEMPSENSOR_CAL1_ADDR);
    temp_tmp = temp_tmp + 30;
    return temp_tmp;
}

static float conv_adc_to_voltage(float vdda_val, uint16_t adc_val)
{
    return vdda_val / 0x0FFF * adc_val / 1000;
}

static float conv_float_avg(float *data, uint8_t data_size)
{
    uint8_t i, min_index, max_index;
    float avg;
    if (data_size < 3)
    {
        return 0;
    }
    min_index = 0;
    max_index = data_size - 1;
    for (i = 0; i < data_size; i++)
    {
        if (data[min_index] > data[i])
        {
            min_index = i;
        }
        else if (data[max_index] < data[i])
        {
            max_index = i;
        }
    }
    avg = 0;
    for (i = 0; i < data_size; i++)
    {
        if (i != min_index && i != max_index)
        {
            avg += data[i] / (data_size - 2);
        }
    }
    return avg;
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
        delay_100ns(LL_ADC_DELAY_TEMPSENSOR_STAB_US * 10); /* 等待温度传感器稳定 */
        /* 结束 */
        LL_ADC_Enable(ANALOG_ADC);
        WAIT_TIMEOUT(LL_ADC_IsActiveFlag_ADRDY(ANALOG_ADC) == 0);
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
    delay_100ns(LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES);
    return 0;
}

uint8_t ADC_GetCalFactor(void)
{
    return LL_ADC_GetCalibrationFactor(ANALOG_ADC) & 0x7F;
}

uint8_t ADC_ConversionSequence(uint32_t channels, uint16_t *data, uint8_t conv_count)
{
    uint8_t i;
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsActiveFlag_ADRDY(ANALOG_ADC) == 0)
    {
        return 1;
    }
    if (channels != 0)
    {
        LL_ADC_REG_SetSequencerChannels(ANALOG_ADC, channels);
    }
    LL_ADC_ClearFlag_EOS(ANALOG_ADC);
    LL_ADC_ClearFlag_EOC(ANALOG_ADC);
    for (i = 0; i < conv_count; i++)
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

float ADC_GetTemp(void)
{
    uint8_t i;
    uint16_t adc_val[2];
    float temp_tmp[10];
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_ConversionSequence(ADC_CHANNELS_TEMP_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_adc_to_temp(conv_vrefint_to_vdda(adc_val[0]), adc_val[1]);
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
}

float ADC_GetBattery(void)
{
    uint8_t i;
    uint16_t adc_val[2];
    float temp_tmp[10];
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_ConversionSequence(ADC_CHANNELS_CH1_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_adc_to_voltage(conv_vrefint_to_vdda(adc_val[1]), adc_val[0]);
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
}

float ADC_GetVDDA(void)
{
    uint8_t i;
    uint16_t adc_val[1];
    float temp_tmp[10];
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_ConversionSequence(ADC_CHANNELS_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_vrefint_to_vdda(adc_val[0]) / 1000;
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
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

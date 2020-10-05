#include "analog.h"

static int16_t VREFINT_offset = 0;

#define WAIT_TIMEOUT(val)                                       \
    timeout = ADC_TIMEOUT_MS;                                   \
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

/**
 * @brief  根据内部参考电压计算VDDA电压。
 * @param  vrefint 内部参考电压的ADC读数。
 * @return VDDA电压，单位为毫伏。
 */
static float conv_vrefint_to_vdda(uint16_t vrefint)
{
    return (VREFINT_CAL_VREF * ((*VREFINT_CAL_ADDR) + VREFINT_offset) / (float)vrefint);
}

/**
 * @brief  将内置温度传感器读出的数据转换为温度。
 * @param  vdda VDDA电压，单位为毫伏。
 * @param  adc 温度传感器的ADC读数。
 * @return 内置温度传感器的温度，单位为度。
 */
static float conv_adc_to_temp(float vdda, uint16_t adc)
{
    float temp;
    temp = (adc * vdda / (TEMPSENSOR_CAL_VREFANALOG)) - *TEMPSENSOR_CAL1_ADDR;
    temp = temp * TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP;
    temp = temp / (*TEMPSENSOR_CAL2_ADDR - *TEMPSENSOR_CAL1_ADDR);
    temp = temp + 30;
    return temp;
}

/**
 * @brief  将ADC读数转换为电压。
 * @param  vdda VDDA电压，单位为毫伏。
 * @param  adc 要转换通道的ADC读数。
 * @return 通道电压，单位为伏。
 */
static float conv_adc_to_voltage(float vdda, uint16_t adc)
{
    return vdda / 0x0FFF * adc / 1000;
}

/**
 * @brief  去掉float数组的最高值和最低值，并计算剩余数据的平均值。
 * @param  data 数组指针。
 * @param  data_size 数组大小，最小为3，低于3返回0。
 * @return 数组内数据的平均值。
 */
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

/**
 * @brief  打开ADC。
 * @return 1：打开失败，0：打开成功。
 */
uint8_t ADC_Enable(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsEnabled(ADC_NUM) == 0)
    {
        /* DocID025942 Rev 8 - Page 277 */
        LL_ADC_ClearFlag_ADRDY(ADC_NUM);
        LL_ADC_EnableInternalRegulator(ADC_NUM);
        /* 
           修复CubeMX的生成BUG，修复后可去除，相同问题可在下面的链接看到：
           https://community.st.com/s/question/0D50X0000AU4CvOSQV/stm32cubemx-lladcsetcommonpathinternalch-generation-bug-only-one-internal-ch-is-enabled
           https://community.st.com/s/question/0D50X0000C20aDz/bug-cubemx-550-stm32g0-wrong-setcommonpathinternalch-initialization
        */
        LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC_NUM), LL_ADC_PATH_INTERNAL_TEMPSENSOR | LL_ADC_PATH_INTERNAL_VREFINT);
        delay_100ns(LL_ADC_DELAY_TEMPSENSOR_STAB_US * 10); /* 等待温度传感器稳定 */
        /* 结束 */
        LL_ADC_Enable(ADC_NUM);
        WAIT_TIMEOUT((LL_ADC_IsActiveFlag_ADRDY(ADC_NUM) == 0 || LL_PWR_IsActiveFlag_VREFINTRDY() == 0));
    }
    return 0;
}

/**
 * @brief  关闭ADC。
 * @return 1：关闭失败，0：关闭成功。
 */
uint8_t ADC_Disable(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    /* DocID025942 Rev 8 - Page 276 */
    if (LL_ADC_IsEnabled(ADC_NUM) != 0)
    {
        if (LL_ADC_REG_IsConversionOngoing(ADC_NUM) != 0)
        {
            LL_ADC_REG_StopConversion(ADC_NUM);
            WAIT_TIMEOUT(LL_ADC_REG_IsStopConversionOngoing(ADC_NUM) != 0);
        }
        LL_ADC_Disable(ADC_NUM);
        WAIT_TIMEOUT(LL_ADC_IsEnabled(ADC_NUM) != 0);
        LL_ADC_ClearFlag_ADRDY(ADC_NUM);
        LL_ADC_DisableInternalRegulator(ADC_NUM);
    }
    return 0;
}

/**
 * @brief  开始ADC自动校准。
 * @return 1：校准失败，0：校准成功。
 * @note   每次ADC关闭并重新开启后需要重新校准。
 */
uint8_t ADC_StartCal(void)
{
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsEnabled(ADC_NUM) != 0)
    {
        return 1;
    }
    /* DocID025942 Rev 8 - Page 277 */
    LL_ADC_ClearFlag_EOCAL(ADC_NUM);
    LL_ADC_StartCalibration(ADC_NUM);
    WAIT_TIMEOUT(LL_ADC_IsActiveFlag_EOCAL(ADC_NUM) == 0);
    LL_ADC_ClearFlag_EOCAL(ADC_NUM);
    delay_100ns(LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES);
    return 0;
}

/**
 * @brief  获取ADC自动校准值。
 * @return ADC自动校准值。
 */
uint8_t ADC_GetCalFactor(void)
{
    return LL_ADC_GetCalibrationFactor(ADC_NUM) & 0x7F;
}

/**
 * @brief  转换一次指定通道的数值。
 * @param  channels 要转换的通道，留空则使用ADC寄存器内当前保存的通道。可以一次选择多个，例如：LL_ADC_CHANNEL_1 | LL_ADC_CHANNEL_VREFINT。
 * @param  data 转换结果存放指针。
 * @param  conv_count 转换次数，一般和要转换的通道数相同，也可以为要转换通道数的倍数，这样会转换多次。
 * @return 1：转换失败，0：转换成功。
 */
uint8_t ADC_StartConversionSequence(uint32_t channels, uint16_t *data, uint8_t conv_count)
{
    uint16_t i;
    uint32_t timeout;
    volatile uint32_t systick_tmp;

    if (LL_ADC_IsActiveFlag_ADRDY(ADC_NUM) == 0)
    {
        return 1;
    }
    if (channels != 0)
    {
        LL_ADC_REG_SetSequencerChannels(ADC_NUM, channels);
    }
    LL_ADC_ClearFlag_EOS(ADC_NUM);
    LL_ADC_ClearFlag_EOC(ADC_NUM);
    for (i = 0; i < 65535; i++)
    {
        if (LL_PWR_IsActiveFlag_VREFINTRDY() != 0) /* 等待VREFINT准备完成 */
        {
            break;
        }
    }
    if (i == 65534)
    {
        for (i = 0; i < conv_count; i++)
        {
            data[i] = 0x00;
        }
        return 1;
    }
    for (i = 0; i < conv_count; i++)
    {
        if (LL_ADC_REG_IsConversionOngoing(ADC_NUM) == 0)
        {
            LL_ADC_REG_StartConversion(ADC_NUM);
        }
        WAIT_TIMEOUT(LL_ADC_IsActiveFlag_EOC(ADC_NUM) == 0);
        data[i] = LL_ADC_REG_ReadConversionData12(ADC_NUM) & 0x0FFF;
        LL_ADC_ClearFlag_EOC(ADC_NUM);
    }
    if (LL_ADC_REG_IsConversionOngoing(ADC_NUM) != 0)
    {
        LL_ADC_REG_StopConversion(ADC_NUM);
        WAIT_TIMEOUT(LL_ADC_REG_IsStopConversionOngoing(ADC_NUM) != 0);
    }
    LL_ADC_ClearFlag_EOS(ADC_NUM);
    return 0;
}

/**
 * @brief  获取MCU内置温度传感器的温度。
 * @return 传感器温度，单位为度。
 */
float ADC_GetTemp(void)
{
    uint16_t i;
    uint16_t adc_val[2];
    float temp_tmp[5];

    for (i = 0; i < 65535; i++)
    {
        if (LL_PWR_IsActiveFlag_VREFINTRDY() != 0) /* 等待VREFINT准备完成 */
        {
            break;
        }
    }
    if (i == 65534)
    {
        return 0;
    }
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_StartConversionSequence(LL_ADC_CHANNEL_TEMPSENSOR | LL_ADC_CHANNEL_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_adc_to_temp(conv_vrefint_to_vdda(adc_val[0]), adc_val[1]);
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
}

/**
 * @brief  获取VDDA的电压。
 * @return VDDA电压，单位为伏。
 */
float ADC_GetVDDA(void)
{
    uint16_t i;
    uint16_t adc_val[1];
    float temp_tmp[5];

    for (i = 0; i < 65535; i++)
    {
        if (LL_PWR_IsActiveFlag_VREFINTRDY() != 0) /* 等待VREFINT准备完成 */
        {
            break;
        }
    }
    if (i == 65534)
    {
        return 0;
    }
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_StartConversionSequence(LL_ADC_CHANNEL_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_vrefint_to_vdda(adc_val[0]) / 1000;
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
}

/**
 * @brief  获取指定通道的电压。
 * @param  channel 要转换的通道，一次只可以选择一个，例如：LL_ADC_CHANNEL_1。
 * @return 通道电压，单位为伏。
 */
float ADC_GetChannel(uint32_t channel)
{
    uint16_t i;
    uint16_t adc_val[2];
    float temp_tmp[5];

    for (i = 0; i < 65535; i++)
    {
        if (LL_PWR_IsActiveFlag_VREFINTRDY() != 0) /* 等待VREFINT准备完成 */
        {
            break;
        }
    }
    if (i == 65534)
    {
        return 0;
    }
    for (i = 0; i < sizeof(temp_tmp) / sizeof(float); i++)
    {
        if (ADC_StartConversionSequence(channel | LL_ADC_CHANNEL_VREFINT, adc_val, sizeof(adc_val) / sizeof(uint16_t)) != 0)
        {
            return 0;
        }
        temp_tmp[i] = conv_adc_to_voltage(conv_vrefint_to_vdda(adc_val[1]), adc_val[0]);
    }
    return conv_float_avg(temp_tmp, sizeof(temp_tmp) / sizeof(float));
}

/**
 * @brief  开启内部参考电压输出。
 */
void ADC_EnableVrefintOutput(void)
{

    LL_SYSCFG_VREFINT_EnableADC();
    LL_SYSCFG_VREFINT_SetConnection(ADC_VREFINT_OUT_PIN);
}

/**
 * @brief  关闭内部参考电压输出。
 */
void ADC_DisableVrefintOutput(void)
{
    LL_SYSCFG_VREFINT_DisableADC();
    LL_SYSCFG_VREFINT_SetConnection(LL_SYSCFG_VREFINT_CONNECT_NONE);
}

/**
 * @brief  获取ADC内部参考电压的工厂校准值。
 * @return ADC参考电压工厂校准值，单位为毫伏。
 */
float ADC_GetVrefintFactory(void)
{
    return (VREFINT_CAL_VREF / (float)0x0FFF) * (*VREFINT_CAL_ADDR);
}

/**
 * @brief  获取ADC内部参考电压的步进值。
 * @return ADC参考电压步进值，单位为毫伏。
 */
float ADC_GetVrefintStep(void)
{
    return (VREFINT_CAL_VREF / (float)0x0FFF);
}

/**
 * @brief  设置ADC内部参考电压偏移。
 * @param  offset 偏移值，单位为ADC原始读数。
 */
void ADC_SetVrefintOffset(int16_t offset)
{
    VREFINT_offset = offset;
}

/**
 * @brief  获取ADC内部参考电压偏移。
 * @return 偏移值，单位为ADC原始读数。
 */
int16_t ADC_GetVrefintOffset(void)
{
    return VREFINT_offset;
}

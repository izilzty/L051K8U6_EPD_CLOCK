#ifndef _ANALOG_H_
#define _ANALOG_H_

#include "main.h"

/* 可修改 */
#define ADC_NUM ADC1
#define ADC_CHANNEL_BATTERY LL_ADC_CHANNEL_1
/* 结束 */

#define ADC_TIMEOUT_MS 1000

uint8_t ADC_Enable(void);
uint8_t ADC_Disable(void);
uint8_t ADC_StartCal(void);
uint8_t ADC_GetCalFactor(void);
uint8_t ADC_StartConversionSequence(uint32_t channels, uint16_t *data, uint8_t data_size);

float ADC_GetTemp(void);
float ADC_GetVDDA(void);
float ADC_GetChannel(uint32_t channel);

#endif

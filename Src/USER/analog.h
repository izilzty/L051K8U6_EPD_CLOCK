#ifndef _ANALOG_H_
#define _ANALOG_H_

#include "main.h"

#define ADC_NUM ADC1
#define ADC_TIMEOUT_MS 1000

#define ADC_CHANNEL_BATTERY LL_ADC_CHANNEL_1

uint8_t ADC_Enable(void);
uint8_t ADC_Disable(void);
uint8_t ADC_StartCal(void);
uint8_t ADC_GetCalFactor(void);
uint8_t ADC_StartConversionSequence(uint32_t channels, uint16_t *data, uint8_t data_size);

float ADC_GetTemp(void);
float ADC_GetVDDA(void);
float ADC_GetChannel(uint32_t channel);

#endif

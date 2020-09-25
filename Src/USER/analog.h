#ifndef _ANALOG_H_
#define _ANALOG_H_

#include "main.h"

#define ANALOG_ADC ADC1
#define ANALOG_TIMEOUT_MS 1000

#define ADC_CHANNELS_VREFINT LL_ADC_CHANNEL_VREFINT
#define ADC_CHANNELS_CH1_VREFINT (LL_ADC_CHANNEL_1 | LL_ADC_CHANNEL_VREFINT)
#define ADC_CHANNELS_TEMP_VREFINT (LL_ADC_CHANNEL_TEMPSENSOR | LL_ADC_CHANNEL_VREFINT)
#define ADC_CHANNELS_ALL_USED (LL_ADC_CHANNEL_1 | LL_ADC_CHANNEL_TEMPSENSOR | LL_ADC_CHANNEL_VREFINT)

uint8_t ADC_Enable(void);
uint8_t ADC_Disable(void);

uint8_t ADC_StartCal(void);
uint8_t ADC_GetCalFactor(void);

uint8_t ADC_ConversionSequence(uint32_t sequence, uint16_t *data, uint8_t data_size);

float ADC_GetTemp(void);
float ADC_GetBattery(void);
float ADC_GetVDDA(void);

#endif

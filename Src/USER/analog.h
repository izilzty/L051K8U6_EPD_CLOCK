#ifndef _ANALOG_H_
#define _ANALOG_H_

#define ANALOG_ADC ADC1
#define ANALOG_TIMEOUT_MS 3000

#include "main.h"

uint8_t ADC_Enable(void);
uint8_t ADC_Disable(void);

uint8_t ADC_StartCal(void);
uint8_t ADC_GetCalFactor(void);

uint8_t ADC_StartSingleConversion(uint16_t *data, uint8_t data_size);

float conv_vref_to_vdda(uint16_t vrefval);
float conv_tempv_to_temp(float vdda, uint16_t adcval);

#endif

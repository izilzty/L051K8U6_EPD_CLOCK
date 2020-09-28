#ifndef _BUZZER_H_
#define _BUZZER_H_

#include "main.h"

/* 可修改 */
#define BUZZER_TIMER TIM2
#define BUZZER_CHANNEL LL_TIM_CHANNEL_CH3
#define BUZZER_OC_SET_FUNC LL_TIM_OC_SetCompareCH3
#define BUZZER_CLOCK 1000000
/* 结束 */

void BUZZER_Enable(void);
void BUZZER_Disable(void);
void BUZZER_Start(void);
void BUZZER_Stop(void);
void BUZZER_Beep(uint16_t time_ms);
void BUZZER_SetVolume(uint8_t vol);
void BUZZER_SetFrqe(uint32_t freq);

#endif

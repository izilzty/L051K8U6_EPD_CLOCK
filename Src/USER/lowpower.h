#ifndef _LOWPOWER_H_
#define _LOWPOWER_H_

#include "main.h"

#define LP_STANDBY_WKUP_PIN LL_PWR_WAKEUP_PIN1
#define LP_STOP_WKUP_EXTI EPD_BUSY_EXTI0_EXTI_IRQn

#define LP_RESET_NONE 0
#define LP_RESET_NORMALRESET 1
#define LP_RESET_POWERON 2
#define LP_RESET_WKUPSTANDBY 3

uint8_t LP_GetResetInfo(void);
void LP_EnterSleep(void);
void LP_EnterStop(void);
void LP_EnterStandby(void);

#endif

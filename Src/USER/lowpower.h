#ifndef _LOWPOWER_H_
#define _LOWPOWER_H_

#include "main.h"

/* 可修改 */
#define LP_STANDBY_WKUP_PIN LL_PWR_WAKEUP_PIN1
#define LP_WKUP_EXTI LL_EXTI_LINE_0
#define LP_LPTIM_NUM LPTIM1
#define LP_LPTIM_WKUP_EXTI LL_EXTI_LINE_29
#define LP_LPTIM_FINAL_CLK 1156.25
#define LP_LPTIM_AUTORELOAD_MS 865
/* 结束 */

#define LP_RESET_NONE 0
#define LP_RESET_NORMALRESET 1
#define LP_RESET_POWERON 2
#define LP_RESET_WKUPSTANDBY 3

void LP_DisableDebug(void);
uint8_t LP_GetResetInfo(void);

void LP_EnterSleep(void);
void LP_EnterStop(void);
void LP_EnterStandby(void);

void LP_LPTIM_IRQCallback(void);
void LP_EnterSleepWithTimeout(uint16_t auto_reload);
void LP_EnterStopWithTimeout(uint16_t auto_reload);

#endif

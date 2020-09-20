#ifndef _FUNC_H_
#define _FUNC_H_

#include "main.h"

#include <stdio.h>

#define ENABLE_DEBUG_PRINT

#include "lowpower.h"
#include "serial.h"
#include "bkpr.h"
#include "eeprom.h"
#include "iic.h"
#include "rtc.h"
#include "epd.h"

void Init(void);
void Loop(void);

#endif

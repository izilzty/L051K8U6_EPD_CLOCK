#ifndef _FUNC_H_
#define _FUNC_H_

#include "main.h"

#define ENABLE_DEBUG_PRINT

#include "lowpower.h"
#include "bkpr.h"
#include "eeprom.h"
#include "serial.h"
#include "iic.h"
#include "ds3231.h"
#include "sht30.h"
#include "gdeh029A1.h"

void Init(void);
void Loop(void);

#endif

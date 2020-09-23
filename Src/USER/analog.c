#include "analog.h"

const static uint16_t *VREFCAL_FACTORY = (const uint16_t *)0x1FF80078;
static uint16_t VREFCAL_CAL = 0;

#ifndef __BH1750_H
#define __BH1750_H

#include "stm32f10x.h"

uint8_t BH1750_Init(void);
uint8_t BH1750_ReadLux_x10(uint16_t *lux_x10);

#endif

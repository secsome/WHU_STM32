#ifndef __BEEP_H
#define __BEEP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void BEEP_SwitchMode(uint8_t mode);

#define BEEP_MODE_OFF   GPIO_PIN_RESET
#define BEEP_MODE_ON    GPIO_PIN_SET

#ifdef __cplusplus
}
#endif

#endif
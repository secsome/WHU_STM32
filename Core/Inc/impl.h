#ifndef __IMPL_H
#define __IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Impl_OnLoopPrepare();
void Impl_OnLoopBody();

#ifdef __cplusplus
}
#endif

#endif
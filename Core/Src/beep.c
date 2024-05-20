#include "beep.h"

void BEEP_SwitchMode(uint8_t mode)
{
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, mode);
}
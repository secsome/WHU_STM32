// The following code would only be executed once,
// when the program starts.

extern void* _scritical;
extern void* _ecritical;
void Bootstrap_InitCriticalData()
{
    // Set all of the critical section data to 0
    for (void** p = &_scritical; p < &_ecritical; p++)
        *p = 0;
}

#include "stm32f4xx.h"

#define IF_MASK(t) if (RCC->CSR & (t))

void Boostrap()
{
    IF_MASK(RCC_CSR_LPWRRSTF_Msk) // Low power reset
    {

    }
    else IF_MASK(RCC_CSR_WWDGRSTF_Msk) // Window watchdog reset
    {

    }
    else IF_MASK(RCC_CSR_IWDGRSTF_Msk) // Independent watchdog reset
    {

    }
    else IF_MASK(RCC_CSR_SFTRSTF_Msk) // Software reset
    {

    }
    else IF_MASK(RCC_CSR_PORRSTF_Msk) // Power-on reset
    {
        Bootstrap_InitCriticalData();
    }
    else IF_MASK(RCC_CSR_PINRSTF_Msk) // Pin reset
    {

    }
    else IF_MASK(RCC_CSR_BORRSTF_Msk) // Brown-out reset
    {

    }
    else // Unknown reset, maybe done by the user
    {
        
    }
}
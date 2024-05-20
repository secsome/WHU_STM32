#include "main_loop.h"

#include "i2c.h"
#include "beep.h"
#include "zlg7290.h"
#include "lm75a.h"

void MainLoop_Body()
{
    {
        uint8_t test = 0x0d;
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, &test, 1);
    }

    const uint16_t temp = LM75A_GetTemp();
    if (temp != LM75A_RESULT_ERROR)
    {
        const double value = LM75A_ParseTemp(temp);
        if (value > 32.0)
        {
            for (int32_t i = 0; i < 250; ++i)
            {
                BEEP_SwitchMode(BEEP_MODE_ON);
                HAL_Delay(2);
                BEEP_SwitchMode(BEEP_MODE_OFF);
                HAL_Delay(2);
            }
            return;
        }
    }
    HAL_Delay(1000);
}
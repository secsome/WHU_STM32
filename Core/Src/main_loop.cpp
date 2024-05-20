#include "main_loop.h"

#include "i2c.h"
#include "beep.h"
#include "zlg7290.h"
#include "lm75a.h"

extern "C" uint8_t KeyPressed;

void MainLoop_Body()
{
    HAL_StatusTypeDef status;
    if (KeyPressed)
    {
        KeyPressed = 0;

        uint8_t buffer[3];
        status = ZLG7290_Read(&hi2c1, ZLG7290_ADDR_KEY, buffer, sizeof(buffer));
        if (status == HAL_OK)
        {
            const uint8_t key = buffer[0];
            const uint8_t repeat = buffer[1]; UNUSED(repeat);
            const uint8_t func = buffer[2]; UNUSED(func);
            // Just show a simple digit 1 on the screen for now
            if (key != 0)
            {
                uint8_t tmp = 0x0C;
                ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, &tmp, sizeof(tmp));
            }
        }
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
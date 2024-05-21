#include "impl.h"

#include "i2c.h"
#include "beep.h"
#include "zlg7290.h"
#include "lm75a.h"

#include "critical_data.hpp"

extern "C" uint8_t KeyPressed;
static critical_data<uint32_t> TemperatureLow; // current lowest temperature
static critical_data<uint32_t> TemperatureHigh; // current highest temperature
static critical_data<uint32_t> TemperatureCurrent; // current temperature
static critical_data<uint32_t> IsEditing; // 0: not editing, 1: editing
static critical_data<uint32_t> CursorPos; // ranges in [0, 2]
static critical_data<uint32_t> EditTarget; // 0: low temperature, 1: high temperature
static critical_data<uint32_t> EditDigitPart; // ranges in [0, 999]
static critical_data<uint32_t> EditDecimalPart; // ranges in [0, 999]
static critical_data<uint32_t> TemperatureHandleTick;

template<typename T, size_t N, typename Fn>
static constexpr T Impl_ReadAverData(T err, Fn&& func)
{
    T result = T{};
    for (size_t i = 0; i < N; ++i)
    {
        T tmp = func();
        if (tmp == err)
            return err;
        result += tmp;
    }
    return result / N;
}

static bool Impl_OnKeyDown(uint8_t key, uint8_t repeat, uint8_t func_key);
static void Impl_OnTemperatureOutOfRange();
static void Impl_OnErrorSound();

void Impl_OnLoopPrepare()
{
    do
    {
        TemperatureHandleTick = 0;
        TemperatureLow = 25 * 8 * 1000;
        TemperatureHigh = 35 * 8 * 1000;
        TemperatureCurrent = (TemperatureLow + TemperatureHigh) / 2;
        if (TemperatureLow > TemperatureHigh)
            continue;
    } while (
        !TemperatureHandleTick.is_valid() ||
        !TemperatureLow.is_valid() || 
        !TemperatureHigh.is_valid() || 
        !TemperatureCurrent.is_valid()
    );

    do
    {
        IsEditing = 0;
        CursorPos = 0;
        EditTarget = 0;
        EditDigitPart = 0;
        EditDecimalPart = 0;
    } while (
        !IsEditing.is_valid() || 
        !CursorPos.is_valid() || 
        !EditTarget.is_valid() ||
        !EditDigitPart.is_valid() ||
        !EditDecimalPart.is_valid()
    );

    // Stop flash
    uint8_t cmd[2];
    cmd[0] = 0b01110000;
    cmd[1] = 0b00000000;
    ZLG7290_Write(&hi2c1, ZLG7290_ADDR_CMDBUF0, cmd, sizeof(cmd));

    // Init display
    uint8_t display[8] = 
    {
        ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
        ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
        ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
        ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE
    };
    ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, display, sizeof(display));
}

void Impl_OnLoopBody()
{
    HAL_StatusTypeDef status;

    // This error can be handled easily by just setting the editing state to 0,
    // So we don't need to call Error_Handler here
    if (!IsEditing.is_valid())
    {
        IsEditing = 0;
        return;
    }

    if (IsEditing == 0)
    {
        constexpr uint32_t kTemperatureDelay = 5000;
        uint32_t current_tick = HAL_GetTick();
        if (!TemperatureHandleTick.is_valid() || current_tick - TemperatureHandleTick > kTemperatureDelay)
        {
            TemperatureHandleTick = current_tick;
            const uint16_t temp = Impl_ReadAverData<uint16_t, 5>(LM75A_RESULT_ERROR, LM75A_GetTemp);
            if (temp != LM75A_RESULT_ERROR)
                TemperatureCurrent = static_cast<uint32_t>(temp) * 1000;
            else
            {
                // If we cannot get temperature, just skip this routine raise a error
                Impl_OnErrorSound();
                return;
            }

            if (!TemperatureCurrent.is_valid() || !TemperatureLow.is_valid() || !TemperatureHigh.is_valid())
            {
                // If the data is invalid, we need to reinitialize the data
                Impl_OnErrorSound();
                Impl_OnLoopPrepare();
                return;
            }
            if (TemperatureCurrent < TemperatureLow || TemperatureCurrent > TemperatureHigh)
                Impl_OnTemperatureOutOfRange();
        }
    }

    if (KeyPressed)
    {
        KeyPressed = 0;
        uint8_t buffer[3];
        status = ZLG7290_Read(&hi2c1, ZLG7290_ADDR_KEY, buffer, sizeof(buffer));
        if (status == HAL_OK)
        {
            if (!Impl_OnKeyDown(buffer[0], buffer[1], buffer[2]))
                return;
        }

        constexpr uint32_t kEditingDelay = 20;
        HAL_Delay(kEditingDelay);
    }
}

static bool Impl_OnKeyDown(uint8_t key, uint8_t repeat, uint8_t func_key)
{
    if (key == 0)
        return true;

    if (!IsEditing.is_valid())
    {
        IsEditing = 0;
        Impl_OnErrorSound();
        return false;
    }

    bool is_editing = IsEditing != 0;
    UNUSED(repeat);
    UNUSED(func_key);

    uint32_t key_num = (uint32_t)-1;
    switch (key)
    {
    case ZLG7290_KEY_0: key_num = 0; break;
    case ZLG7290_KEY_1: key_num = 1; break;
    case ZLG7290_KEY_2: key_num = 2; break;
    case ZLG7290_KEY_3: key_num = 3; break;
    case ZLG7290_KEY_4: key_num = 4; break;
    case ZLG7290_KEY_5: key_num = 5; break;
    case ZLG7290_KEY_6: key_num = 6; break;
    case ZLG7290_KEY_7: key_num = 7; break;
    case ZLG7290_KEY_8: key_num = 8; break;
    case ZLG7290_KEY_9: key_num = 9; break;
    case ZLG7290_KEY_A: // switch to low temperate
    {
        if (!is_editing)
            break;

        EditTarget = 0;
        CursorPos = 0;
        EditDigitPart = TemperatureLow / 8 / 1000;
        EditDecimalPart = TemperatureLow / 8 % 1000;
        break;
    }
    case ZLG7290_KEY_B: // switch to high temperate
    {
        if (!is_editing)
            break;

        EditTarget = 1;
        CursorPos = 0;
        EditDigitPart = TemperatureHigh / 8 / 1000;
        EditDecimalPart = TemperatureHigh / 8 % 1000;
        break;
    }
    case ZLG7290_KEY_C: // move cursor left
    {
        if (!is_editing)
            break;

        if (!CursorPos.is_valid())
        {
            CursorPos = 0;
            Impl_OnErrorSound();
            break;
        }

        CursorPos = (CursorPos + 2) % 3;
        break;
    }
    case ZLG7290_KEY_D: // move cursor right
    {
        if (!is_editing)
            break;

        if (!CursorPos.is_valid())
        {
            CursorPos = 0;
            Impl_OnErrorSound();
            break;
        }

        CursorPos = (CursorPos + 1) % 3;
        break;
    }
    case ZLG7290_KEY_STAR: // enter edit mode, or exit without saving
    {
        if (is_editing)
        {
            is_editing = false;
            CursorPos = 0;
            EditTarget = 0;
        }
        else
        {
            is_editing = true;
            CursorPos = 0;
            EditTarget = 0;
            EditDigitPart = TemperatureLow / 8 / 1000;
            EditDecimalPart = TemperatureLow / 8 % 1000;
        }
        break;
    }
    case ZLG7290_KEY_POUND: // confirm, apply the changes
    {
        if (!is_editing) // not in edit mode, just ignore it
            break;

        if (!EditDigitPart.is_valid() || !EditDecimalPart.is_valid() || !EditTarget.is_valid())
        {
            // If the data is invalid, we need to reinitialize the data
            Impl_OnErrorSound();
            Impl_OnLoopPrepare();
            return false;
        }

        // Combine two parts of the temperature
        const uint32_t value = (EditDigitPart * 1000 + EditDecimalPart) * 8;
        if (EditTarget == 0)
            TemperatureLow = value;
        else if (EditTarget == 1)
            TemperatureHigh = value;
        is_editing = false; // Save the changes and exit editing mode
        break;
    }
    default:
        break;
    }
    do
    {
        if (key_num == (uint32_t)-1)
            break;
        if (!is_editing)
            break;
        
        if (!CursorPos.is_valid())
        {
            CursorPos = 0;
            Impl_OnErrorSound();
            break;
        }

        if (!EditTarget.is_valid())
        {
            EditTarget = 0;
            Impl_OnErrorSound();
            break;
        }

        if (EditTarget == 0)
        {
            uint32_t value = EditDigitPart;
            if (CursorPos == 0)
                value = value / 10 * 10 + key_num;
            else if (CursorPos == 1)
                value = value % 10 + value / 100 * 100 + key_num * 10;
            else if (CursorPos == 2)
                value = value % 100 + key_num * 100;
            else
            {
                CursorPos = 0;
                Impl_OnErrorSound();
                break;
            }
            EditDigitPart = value;
        }
        else if (EditTarget == 1)
        {
            uint32_t value = EditDecimalPart;
            if (CursorPos == 0)
                value = value / 10 * 10 + key_num;
            else if (CursorPos == 1)
                value = value % 10 + value / 100 * 100 + key_num * 10;
            else if (CursorPos == 2)
                value = value % 100 + key_num * 100;
            else
            {
                CursorPos = 0;
                Impl_OnErrorSound();
                break;
            }
            EditDecimalPart = value;
        }
        else
        {
            EditTarget = 0;
            Impl_OnErrorSound();
        }
    } while(false);

    // Update flashing state
    if (!is_editing)
    {
        // clear content
        uint8_t display[8] 
        { 
            ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
            ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
            ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE,
            ZLG7290_DISPLAY_MIDDLE, ZLG7290_DISPLAY_MIDDLE
        };
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, display, sizeof(display));
        
        // Stop flash
        uint8_t cmd[2];
        cmd[0] = 0b01110000;
        cmd[1] = 0b00000000;
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_CMDBUF0, cmd, sizeof(cmd));

        IsEditing = 0;
    }
    else
    {
        // Display current numbers
        constexpr uint8_t display_table[10] 
        {
            ZLG7290_DISPLAY_NUM0, ZLG7290_DISPLAY_NUM1, ZLG7290_DISPLAY_NUM2, ZLG7290_DISPLAY_NUM3,
            ZLG7290_DISPLAY_NUM4, ZLG7290_DISPLAY_NUM5, ZLG7290_DISPLAY_NUM6, ZLG7290_DISPLAY_NUM7,
            ZLG7290_DISPLAY_NUM8, ZLG7290_DISPLAY_NUM9
        };
        if (!EditDigitPart.is_valid())
        {
            EditDigitPart = 0;
            Impl_OnErrorSound();
        }
        if (!EditDecimalPart.is_valid())
        {
            EditDecimalPart = 0;
            Impl_OnErrorSound();
        }
        uint8_t display[8];
        display[0] = display[7] = 0;
        display[1] = display_table[EditDigitPart / 100 % 10];
        display[2] = display_table[(EditDigitPart % 100) / 10];
        display[3] = display_table[EditDigitPart % 10] | ZLG7290_DISPLAY_DOT;
        display[4] = display_table[EditDecimalPart / 100 % 10];
        display[5] = display_table[(EditDecimalPart % 100) / 10];
        display[6] = display_table[EditDecimalPart % 10];
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, display, sizeof(display));

        // Start flash
        uint8_t cmd[2];
        cmd[0] = 0b01110000;
        if (EditTarget == 0)
        {
            if (CursorPos == 0)
                cmd[1] = 0b00001000;
            else if (CursorPos == 1)
                cmd[1] = 0b00000100;
            else if (CursorPos == 2)
                cmd[1] = 0b00000010;
            else
            {
                CursorPos = 0;
                Impl_OnErrorSound();
                return false;
            }
        }
        else if (EditTarget == 1)
        {
            if (CursorPos == 0)
                cmd[1] = 0b01000000;
            else if (CursorPos == 1)
                cmd[1] = 0b00100000;
            else if (CursorPos == 2)
                cmd[1] = 0b00010000;
            else
            {
                CursorPos = 0;
                Impl_OnErrorSound();
                return false;
            }
        }
        else
        {
            CursorPos = 0;
            EditTarget = 0;
            Impl_OnErrorSound();
            return false;
        }
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_CMDBUF0, cmd, sizeof(cmd));
        
        IsEditing = 1;
    }

    return true;
}

void Impl_OnTemperatureOutOfRange()
{
    constexpr uint32_t kBeepDuration = 1000;
    constexpr uint32_t kBeepFrequency = 2;
    for (uint32_t i = 0; i < kBeepDuration; i += 2 * kBeepFrequency)
    {
        BEEP_SwitchMode(BEEP_MODE_ON);
        HAL_Delay(kBeepFrequency);
        BEEP_SwitchMode(BEEP_MODE_OFF);
        HAL_Delay(kBeepFrequency);
    }
}

static void Impl_OnErrorSound()
{
    // This function is used to indicate user that an error has occurred
    constexpr uint32_t kBeepDuration = 200;
    constexpr uint32_t kBeepFrequency = 10;
    for (uint32_t i = 0; i < kBeepDuration; i += 2 * kBeepFrequency)
    {
        BEEP_SwitchMode(BEEP_MODE_ON);
        HAL_Delay(kBeepFrequency);
        BEEP_SwitchMode(BEEP_MODE_OFF);
        HAL_Delay(kBeepFrequency);
    }
}

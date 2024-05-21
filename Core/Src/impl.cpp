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
        TemperatureLow = 25 * 8 * 1000;
        TemperatureHigh = 35 * 8 * 1000;
        TemperatureCurrent = (TemperatureLow + TemperatureHigh) / 2;
        if (TemperatureLow > TemperatureHigh)
            continue;
    } while (
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

        constexpr uint32_t kEditingDelay = 50;
        HAL_Delay(kEditingDelay);
    }

    // This error can be handled easily by just setting the editing state to 0,
    // So we don't need to call Error_Handler here
    if (!IsEditing.is_valid())
    {
        IsEditing = 0;
        return;
    }

    if (IsEditing == 0)
    {
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
        
        // System delay time in milliseconds
        constexpr uint32_t kSystemDelay = 5000;
        HAL_Delay(kSystemDelay);
    }
}

static bool Impl_OnKeyDown(uint8_t key, uint8_t repeat, uint8_t func_key)
{
    if (!IsEditing.is_valid())
    {
        IsEditing = 0;
        Impl_OnErrorSound();
        return false;
    }

    bool is_editing = IsEditing != 0;
    UNUSED(repeat);
    UNUSED(func_key);

    uint32_t key_num = 0;
    switch (key)
    {
    case ZLG7290_KEY_0: key_num = 0;
    case ZLG7290_KEY_1: key_num = 1;
    case ZLG7290_KEY_2: key_num = 2;
    case ZLG7290_KEY_3: key_num = 3;
    case ZLG7290_KEY_4: key_num = 4;
    case ZLG7290_KEY_5: key_num = 5;
    case ZLG7290_KEY_6: key_num = 6;
    case ZLG7290_KEY_7: key_num = 7;
    case ZLG7290_KEY_8: key_num = 8;
    case ZLG7290_KEY_9: key_num = 9;
    {
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
            break;
        }

        break;
    }
    case ZLG7290_KEY_A: // switch to low temperate
    {
        if (!is_editing)
            break;

        EditTarget = 1;
        CursorPos = 0;
        break;
    }
    case ZLG7290_KEY_B: // switch to high temperate
    {
        if (!is_editing)
            break;

        EditTarget = 1;
        CursorPos = 0;
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
        break;
    }
    default:
        break;
    }

    // Update flashing state
    if (!is_editing)
    {
        // Stop flash
        uint8_t cmd[2];
        cmd[0] = 0b01110000;
        cmd[1] = 0b00000000;
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_CMDBUF0, cmd, sizeof(cmd));
        IsEditing = 0;
    }
    else
    {
        // Start flash
        uint8_t cmd[2];
        cmd[0] = 0b01110000;
        if (EditTarget == 0)
        {
            if (CursorPos == 0)
                cmd[1] = 0b00000001;
            else if (CursorPos == 1)
                cmd[1] = 0b00000010;
            else if (CursorPos == 2)
                cmd[1] = 0b00000100;
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
                cmd[1] = 0b00001000;
            else if (CursorPos == 1)
                cmd[1] = 0b00010000;
            else if (CursorPos == 2)
                cmd[1] = 0b00100000;
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
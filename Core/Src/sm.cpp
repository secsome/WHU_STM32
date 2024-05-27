#include "sm.h"

#include "critical_data.hpp"

#include "main.h"
#include "i2c.h"
#include "lm75a.h"
#include "beep.h"
#include "zlg7290.h"

CRITICAL(uint32_t, SM_Inititalized);
CRITICAL(uint32_t, SM_Operation);
CRITICAL(uint32_t, KeyPressed);
CRITICAL(uint32_t, KeyData);
CRITICAL(uint32_t, KeyNum);
CRITICAL(uint32_t, TemperatureLow); // current lowest temperature
CRITICAL(uint32_t, TemperatureHigh); // current highest temperature
CRITICAL(uint32_t, TemperatureCurrent); // current temperature
CRITICAL(uint32_t, IsEditing); // 0: not editing, 1: editing
CRITICAL(uint32_t, CursorPos); // ranges in [0, 5]
CRITICAL(uint32_t, EditTarget); // 0: low temperature, 1: high temperature
CRITICAL(uint32_t, EditTemperate); // ranges in [0, 999999]
CRITICAL(uint32_t, TemperatureHandleTick);

constexpr uint32_t SM_TEMPERATURE_LOW_INIT = 25 * 8 * 1000;
constexpr uint32_t SM_TEMPERATURE_HIGH_INIT = 35 * 8 * 1000;

#define SM_CASE(x) case x: SM_Operation = _##x(); break
#define SM_STATE(x) static uint32_t _##x()

enum
{
    SM_OPT_IS_EDITING,
    SM_OPT_CHECK_TEMPTICK,
    SM_OPT_READTEMP,
    SM_OPT_IS_TEMP_IN_RANGE,
    SM_OPT_TEMP_OUT_OF_RANGE,
    SM_OPT_READ_KEY_INPUT,
    SM_OPT_READ_KEY_DELAY,
    SM_OPT_ON_KEY_PRESSED,
    SM_OPT_UPDATE_KEYNUM,
    SM_OPT_SWITCH_TARGET_LOW,
    SM_OPT_SWITCH_TARGET_HIGH,
    SM_OPT_MOVE_CURSOR_LEFT,
    SM_OPT_MOVE_CURSOR_RIGHT,
    SM_OPT_SWITCH_EDIT_MODE,
    SM_OPT_SAVE_AND_EXIT_EDIT,
    SM_OPT_UPDATE_DISPLAY,
    SM_OPT_RESETHANDLER,
};

void SM_Init()
{
    if (SM_Inititalized.is_valid() && SM_Inititalized)
        return;
        
    do
    {
        TemperatureHandleTick = 0;
        TemperatureLow = SM_TEMPERATURE_LOW_INIT;
        TemperatureHigh = SM_TEMPERATURE_HIGH_INIT;
        TemperatureCurrent = (TemperatureLow + TemperatureHigh) / 2;
        if (TemperatureLow > TemperatureHigh)
            continue;
    } while (!TemperatureHandleTick || !TemperatureLow || !TemperatureHigh || !TemperatureCurrent);

    do
    {
        IsEditing = 0;
        CursorPos = 0;
        EditTarget = 0;
        EditTemperate = 0;
    } while (!IsEditing || !CursorPos || !EditTarget || !EditTemperate);

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
    SM_Operation = SM_OPT_IS_EDITING;

    SM_Inititalized = 1;
}

SM_STATE(SM_OPT_IS_EDITING);
SM_STATE(SM_OPT_CHECK_TEMPTICK);
SM_STATE(SM_OPT_READTEMP);
SM_STATE(SM_OPT_IS_TEMP_IN_RANGE);
SM_STATE(SM_OPT_TEMP_OUT_OF_RANGE);
SM_STATE(SM_OPT_READ_KEY_INPUT);
SM_STATE(SM_OPT_READ_KEY_DELAY);
SM_STATE(SM_OPT_ON_KEY_PRESSED);
SM_STATE(SM_OPT_UPDATE_KEYNUM);
SM_STATE(SM_OPT_SWITCH_TARGET_LOW);
SM_STATE(SM_OPT_SWITCH_TARGET_HIGH);
SM_STATE(SM_OPT_MOVE_CURSOR_LEFT);
SM_STATE(SM_OPT_MOVE_CURSOR_RIGHT);
SM_STATE(SM_OPT_SWITCH_EDIT_MODE);
SM_STATE(SM_OPT_SAVE_AND_EXIT_EDIT);
SM_STATE(SM_OPT_UPDATE_DISPLAY);
SM_STATE(SM_OPT_RESETHANDLER);

void SM_Run()
{
    switch (SM_Operation)
    {
    SM_CASE(SM_OPT_IS_EDITING);
    SM_CASE(SM_OPT_CHECK_TEMPTICK);
    SM_CASE(SM_OPT_READTEMP);
    SM_CASE(SM_OPT_IS_TEMP_IN_RANGE);
    SM_CASE(SM_OPT_TEMP_OUT_OF_RANGE);
    SM_CASE(SM_OPT_READ_KEY_INPUT);
    SM_CASE(SM_OPT_READ_KEY_DELAY);
    SM_CASE(SM_OPT_ON_KEY_PRESSED);
    SM_CASE(SM_OPT_UPDATE_KEYNUM);
    SM_CASE(SM_OPT_SWITCH_TARGET_LOW);
    SM_CASE(SM_OPT_SWITCH_TARGET_HIGH);
    SM_CASE(SM_OPT_MOVE_CURSOR_LEFT);
    SM_CASE(SM_OPT_MOVE_CURSOR_RIGHT);
    SM_CASE(SM_OPT_SWITCH_EDIT_MODE);
    SM_CASE(SM_OPT_SAVE_AND_EXIT_EDIT);
    SM_CASE(SM_OPT_UPDATE_DISPLAY);
    default:
    SM_CASE(SM_OPT_RESETHANDLER);
    }
}

SM_STATE(SM_OPT_IS_EDITING)
{
    if (!IsEditing)
        IsEditing = 0;

    if (IsEditing == 0)
        return SM_OPT_CHECK_TEMPTICK;

    return SM_OPT_READ_KEY_INPUT;
}

SM_STATE(SM_OPT_CHECK_TEMPTICK)
{
    constexpr uint32_t kTemperatureDelay = 5000;
    uint32_t current_tick = HAL_GetTick();
    if (!TemperatureHandleTick || current_tick - TemperatureHandleTick > kTemperatureDelay)
    {
        TemperatureHandleTick = current_tick;
        return SM_OPT_READTEMP;
    }
    
    return SM_OPT_READ_KEY_INPUT;
}

template<typename T, size_t N, typename Fn>
static constexpr T ReadAverData(T err, Fn&& func)
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

SM_STATE(SM_OPT_READTEMP)
{
    LM75A_SetMode(LM75A_ADDR_CONF, LM75A_MODE_WORKING);
    
    const lm75a_temp_t temp = ReadAverData<lm75a_temp_t, 5>(LM75A_RESULT_ERROR, LM75A_GetTemp);
    LM75A_SetMode(LM75A_ADDR_CONF, LM75A_MODE_SHUTDOWN);
    if (temp != LM75A_RESULT_ERROR)
    {
        TemperatureCurrent = static_cast<uint32_t>(temp) * 1000;
        return SM_OPT_IS_TEMP_IN_RANGE;
    }
    
    // Deadlock might happen here, so we try to 
    // recover the state by calling RESET_HANDLER
    return SM_OPT_RESETHANDLER;
}

SM_STATE(SM_OPT_IS_TEMP_IN_RANGE)
{
    if (!TemperatureLow)
    {
        TemperatureLow = SM_TEMPERATURE_LOW_INIT;
        return SM_OPT_READ_KEY_INPUT;
    }
    if (!TemperatureHigh)
    {
        TemperatureHigh = SM_TEMPERATURE_HIGH_INIT;
        return SM_OPT_READ_KEY_INPUT;
    }
    if (!TemperatureCurrent)
    {
        TemperatureCurrent = (TemperatureLow + TemperatureHigh) / 2;
        return SM_OPT_READ_KEY_INPUT;
    }

    if (TemperatureCurrent < TemperatureLow || TemperatureCurrent > TemperatureHigh)
        return SM_OPT_TEMP_OUT_OF_RANGE;

    return SM_OPT_READ_KEY_INPUT;
}

SM_STATE(SM_OPT_TEMP_OUT_OF_RANGE)
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
    return SM_OPT_READ_KEY_INPUT;
}

SM_STATE(SM_OPT_READ_KEY_INPUT)
{
    if (!KeyPressed || KeyPressed == 0)
        return SM_OPT_IS_EDITING;
    
    KeyPressed = 0;
    uint8_t buffer[3];
    auto status = ZLG7290_Read(&hi2c1, ZLG7290_ADDR_KEY, buffer, sizeof(buffer));
    if (status != HAL_OK)
        return SM_OPT_READ_KEY_DELAY;

    KeyData = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
    return SM_OPT_ON_KEY_PRESSED;
}

SM_STATE(SM_OPT_READ_KEY_DELAY)
{
    constexpr uint32_t kEditingDelay = 20;
    HAL_Delay(kEditingDelay);
    return SM_OPT_IS_EDITING;
}

SM_STATE(SM_OPT_ON_KEY_PRESSED)
{
    if (!KeyData)
        KeyData = 0;

    uint32_t key = KeyData & 0xff;
    if (key == 0)
        return SM_OPT_READ_KEY_DELAY;
    
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_READ_KEY_DELAY;
    }

    switch (key)
    {
    case ZLG7290_KEY_0: KeyNum = 0; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_1: KeyNum = 1; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_2: KeyNum = 2; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_3: KeyNum = 3; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_4: KeyNum = 4; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_5: KeyNum = 5; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_6: KeyNum = 6; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_7: KeyNum = 7; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_8: KeyNum = 8; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_9: KeyNum = 9; return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_A: return SM_OPT_SWITCH_TARGET_LOW;
    case ZLG7290_KEY_B: return SM_OPT_SWITCH_TARGET_HIGH;
    case ZLG7290_KEY_C: return SM_OPT_MOVE_CURSOR_LEFT;
    case ZLG7290_KEY_D: return SM_OPT_MOVE_CURSOR_RIGHT;
    case ZLG7290_KEY_STAR: return SM_OPT_SWITCH_EDIT_MODE;
    case ZLG7290_KEY_POUND: return SM_OPT_SAVE_AND_EXIT_EDIT;
    }
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_UPDATE_KEYNUM)
{
    if (!KeyNum)
        return SM_OPT_UPDATE_DISPLAY;
    const auto keynum = KeyNum.get();

    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!EditTarget)
    {
        EditTarget = 0;
        CursorPos = 0;
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!EditTemperate)
    {
        EditTemperate = 0;
        CursorPos = 0;
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }
    uint32_t new_value = EditTemperate;

    if (!CursorPos)
    {
        CursorPos = 0;
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    switch (CursorPos)
    {
    case 0: new_value = new_value % 100000 + keynum * 100000; break;
    case 1: new_value = new_value % 10000 + keynum * 10000 + new_value / 100000 * 100000; break;
    case 2: new_value = new_value % 1000 + keynum * 1000 + new_value / 10000 * 10000; break;
    case 3: new_value = new_value % 100 + keynum * 100 + new_value / 1000 * 1000; break;
    case 4: new_value = new_value % 10 + keynum * 10 + new_value / 100 * 100; break;
    case 5: new_value = new_value / 10 * 10 + keynum; break;
    default: __builtin_unreachable();
    }
    
    EditTemperate = new_value;

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_TARGET_LOW)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    EditTarget = 0;
    CursorPos = 0;

    if (!TemperatureLow)
        TemperatureLow = SM_TEMPERATURE_LOW_INIT;
    EditTemperate = TemperatureLow / 8 % 1000000;
    
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_TARGET_HIGH)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    EditTarget = 1;
    CursorPos = 0;

    if (!TemperatureHigh)
        TemperatureHigh = SM_TEMPERATURE_HIGH_INIT;
    EditTemperate = TemperatureHigh / 8 % 1000000;

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_MOVE_CURSOR_LEFT)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!CursorPos)
        CursorPos = 0;

    if (CursorPos > 0)
        CursorPos = CursorPos - 1;

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_MOVE_CURSOR_RIGHT)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!CursorPos)
        CursorPos = 0;

    if (CursorPos < 5)
        CursorPos = CursorPos + 1;

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_EDIT_MODE)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (IsEditing)
    {
        IsEditing = 0;
        CursorPos = 0;
        EditTarget = 0;
    }
    else
    {
        IsEditing = 1;
        CursorPos = 0;
        EditTarget = 0;
        if (!TemperatureLow)
            TemperatureLow = SM_TEMPERATURE_LOW_INIT;
        EditTemperate = TemperatureLow / 8 % 1000000;
    }
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SAVE_AND_EXIT_EDIT)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!IsEditing)
        return SM_OPT_READ_KEY_DELAY;

    IsEditing = 0;
    CursorPos = 0;
    EditTarget = 0;
    if (!EditTarget)
    {
        EditTarget = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }
    if (!EditTemperate)
    {
        EditTemperate = 0;
        return SM_OPT_UPDATE_DISPLAY;
    }

    uint32_t current_temp = EditTemperate * 8;
    if (EditTarget == 0)
    {
        if (current_temp > TemperatureHigh)
            current_temp = TemperatureHigh;
        TemperatureLow = current_temp;
    }
    else
    {
        if (current_temp < TemperatureLow)
            current_temp = TemperatureLow;
        TemperatureHigh = current_temp;
    }
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_UPDATE_DISPLAY)
{
    if (!IsEditing)
    {
        IsEditing = 0;
        return SM_OPT_IS_EDITING;
    }

    if (IsEditing)
    {
        constexpr uint8_t display_table[10] 
        {
            ZLG7290_DISPLAY_NUM0, ZLG7290_DISPLAY_NUM1, ZLG7290_DISPLAY_NUM2, ZLG7290_DISPLAY_NUM3,
            ZLG7290_DISPLAY_NUM4, ZLG7290_DISPLAY_NUM5, ZLG7290_DISPLAY_NUM6, ZLG7290_DISPLAY_NUM7,
            ZLG7290_DISPLAY_NUM8, ZLG7290_DISPLAY_NUM9
        };
        if (!EditTemperate)
            EditTemperate = 0;
        if (!CursorPos)
            CursorPos = 0;
        const auto temp = EditTemperate.get();
        const auto cursor = CursorPos.get();

        uint8_t display[8];
        display[0] = display[7] = 0;
        display[1] = display_table[temp / 100000 % 10];
        display[2] = display_table[temp / 10000 % 10];
        display[3] = display_table[temp / 1000 % 10] | ZLG7290_DISPLAY_DOT;
        display[4] = display_table[temp / 100 % 10];
        display[5] = display_table[temp / 10 % 10];
        display[6] = display_table[temp % 10];
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_DPRAM0, display, sizeof(display));

        // Start flash
        uint8_t cmd[2];
        cmd[0] = 0b01110000;
        cmd[1] = 1 << (cursor + 1);
        ZLG7290_Write(&hi2c1, ZLG7290_ADDR_CMDBUF0, cmd, sizeof(cmd));
        
        IsEditing = 1;
    }
    else
    {
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
    }

    return SM_OPT_IS_EDITING;
}

extern "C" void Reset_Handler();
SM_STATE(SM_OPT_RESETHANDLER)
{
    Reset_Handler();
    __builtin_unreachable();
    return SM_OPT_IS_EDITING;
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
        KeyPressed = 1;
}

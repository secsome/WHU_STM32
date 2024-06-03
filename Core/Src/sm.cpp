#include "sm.h"

#include "backup_data.hpp"

#include "main.h"
#include "i2c.h"
#include "lm75a.h"
#include "beep.h"
#include "zlg7290.h"

BACKUP(uint32_t, SM_ResetJumpBack);
BACKUP(uint32_t, SM_Inititalized);
BACKUP(uint32_t, SM_Operation);
BACKUP(uint32_t, KeyPressed);
BACKUP(uint32_t, KeyData);
BACKUP(uint32_t, KeyNum);
BACKUP(uint32_t, TemperatureLow); // current lowest temperature
BACKUP(uint32_t, TemperatureHigh); // current highest temperature
BACKUP(uint32_t, TemperatureCurrent); // current temperature
BACKUP(uint32_t, IsEditing); // 0: not editing, 1: editing
BACKUP(uint32_t, CursorPos); // ranges in [0, 5]
BACKUP(uint32_t, EditTarget); // 0: low temperature, 1: high temperature
BACKUP(uint32_t, EditTemperate); // ranges in [0, 999999]
BACKUP(uint32_t, TemperatureHandleTick);
BACKUP(uint32_t, LastStep);
BACKUP(uint32_t, LastResetTick);

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
    if (BACKUP_IS_VALID(SM_Inititalized))
    {
        uint32_t sm_initialized;
        BACKUP_GET(SM_Inititalized, sm_initialized);
        if (sm_initialized)
        {
            if (BACKUP_IS_VALID(SM_ResetJumpBack))
            {
                uint32_t jmp_back;
                BACKUP_GET(SM_ResetJumpBack, jmp_back);
                SM_Operation = jmp_back;
            }
            return;
        }
    }
        
    do
    {
        BACKUP_SET(TemperatureHandleTick, 0);
        BACKUP_SET(TemperatureLow, SM_TEMPERATURE_LOW_INIT);
        BACKUP_SET(TemperatureHigh, SM_TEMPERATURE_HIGH_INIT);
        BACKUP_SET(TemperatureCurrent, (TemperatureLow + TemperatureHigh) / 2);
    } while (
        !BACKUP_IS_VALID(TemperatureHandleTick) || 
        !BACKUP_IS_VALID(TemperatureLow) || 
        !BACKUP_IS_VALID(TemperatureHigh) || 
        !BACKUP_IS_VALID(TemperatureCurrent)
    );

    do
    {
        IsEditing = 0;
        CursorPos = 0;
        EditTarget = 0;
        EditTemperate = 0;
    } while (
        !BACKUP_IS_VALID(IsEditing) || 
        !BACKUP_IS_VALID(CursorPos) || 
        !BACKUP_IS_VALID(EditTarget) || 
        !BACKUP_IS_VALID(EditTemperate)
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
    
    BACKUP_SET(SM_Operation, SM_OPT_IS_EDITING);
    BACKUP_SET(LastStep, SM_OPT_RESETHANDLER);
    BACKUP_SET(LastResetTick, HAL_GetTick());
    BACKUP_SET(SM_Inititalized, 1);
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
    // Reset after several time automatically
    constexpr uint32_t kGlobalResetTime = 600000;
    uint32_t current_tick = HAL_GetTick();
    uint32_t last_reset_tick;
    BACKUP_GET(LastResetTick, last_reset_tick);
    if (!BACKUP_IS_VALID(LastResetTick) || current_tick - last_reset_tick > kGlobalResetTime)
    {
        BACKUP_SET(LastResetTick, current_tick);
        const auto current_opt = SM_Operation.get();
        BACKUP_SET(SM_ResetJumpBack, current_opt);
        BACKUP_SET(SM_Operation, SM_OPT_RESETHANDLER);
    }

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
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_READ_KEY_INPUT:
    case SM_OPT_READ_KEY_DELAY:
    case SM_OPT_UPDATE_DISPLAY:
    case SM_OPT_RESETHANDLER:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_IS_EDITING);

    if (!BACKUP_IS_VALID(IsEditing))
        BACKUP_SET(IsEditing, 0);

    uint32_t is_editing;
    BACKUP_GET(IsEditing, is_editing);
    if (is_editing == 0)
        return SM_OPT_CHECK_TEMPTICK;

    return SM_OPT_READ_KEY_INPUT;
}

SM_STATE(SM_OPT_CHECK_TEMPTICK)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_IS_EDITING:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_CHECK_TEMPTICK);

    constexpr uint32_t kTemperatureDelay = 5000;
    uint32_t current_tick = HAL_GetTick();
    uint32_t temperature_tick;
    BACKUP_GET(TemperatureHandleTick, temperature_tick);
    if (!BACKUP_IS_VALID(TemperatureHandleTick) || current_tick - temperature_tick > kTemperatureDelay)
    {
        BACKUP_SET(TemperatureHandleTick, current_tick);
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
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_CHECK_TEMPTICK:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_READTEMP);

    LM75A_SetMode(LM75A_ADDR_CONF, LM75A_MODE_WORKING);   
    const lm75a_temp_t temp = ReadAverData<lm75a_temp_t, 5>(LM75A_RESULT_ERROR, LM75A_GetTemp);
    LM75A_SetMode(LM75A_ADDR_CONF, LM75A_MODE_SHUTDOWN);
    if (temp != LM75A_RESULT_ERROR)
    {
        BACKUP_SET(TemperatureCurrent, static_cast<uint32_t>(temp) * 1000);
        return SM_OPT_IS_TEMP_IN_RANGE;
    }
    
    // Deadlock might happen here, so we try to 
    // recover the state by calling RESET_HANDLER
    BACKUP_SET(SM_ResetJumpBack, SM_OPT_READTEMP);
    return SM_OPT_RESETHANDLER;
}

SM_STATE(SM_OPT_IS_TEMP_IN_RANGE)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_READTEMP:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_IS_TEMP_IN_RANGE);

    if (!BACKUP_IS_VALID(TemperatureLow))
    {
        BACKUP_SET(TemperatureLow, SM_TEMPERATURE_LOW_INIT);
        return SM_OPT_READ_KEY_INPUT;
    }
    if (!BACKUP_IS_VALID(TemperatureHigh))
    {
        BACKUP_SET(TemperatureHigh, SM_TEMPERATURE_HIGH_INIT);
        return SM_OPT_READ_KEY_INPUT;
    }
    if (!BACKUP_IS_VALID(TemperatureCurrent))
    {
        BACKUP_SET(TemperatureCurrent, (TemperatureLow + TemperatureHigh) / 2);
        return SM_OPT_READ_KEY_INPUT;
    }

    uint32_t temperature_low;
    BACKUP_GET(TemperatureLow, temperature_low);
    uint32_t temperature_high;
    BACKUP_GET(TemperatureHigh, temperature_high);
    uint32_t temperature_current;
    BACKUP_GET(TemperatureCurrent, temperature_current);

    if (temperature_current < temperature_low || temperature_current > temperature_high)
        return SM_OPT_TEMP_OUT_OF_RANGE;

    return SM_OPT_READ_KEY_INPUT;
}

SM_STATE(SM_OPT_TEMP_OUT_OF_RANGE)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_IS_TEMP_IN_RANGE:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_TEMP_OUT_OF_RANGE);

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
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_IS_EDITING:
    case SM_OPT_CHECK_TEMPTICK:
    case SM_OPT_IS_TEMP_IN_RANGE:
    case SM_OPT_TEMP_OUT_OF_RANGE:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_READ_KEY_INPUT);

    uint32_t key_pressed;
    BACKUP_GET(KeyPressed, key_pressed);
    if (!BACKUP_IS_VALID(KeyPressed) || key_pressed == 0)
        return SM_OPT_IS_EDITING;
    
    BACKUP_SET(KeyPressed, 0);
    uint8_t buffer[3];

    auto status = ZLG7290_Read(&hi2c1, ZLG7290_ADDR_KEY, buffer, sizeof(buffer));
    if (status != HAL_OK)
        return SM_OPT_READ_KEY_DELAY;
    for (size_t i = 1; i < 3; ++i)
    {
        uint8_t buffer2[3];
        auto status2 = ZLG7290_Read(&hi2c1, ZLG7290_ADDR_KEY, buffer2, sizeof(buffer));
        if (status2 != HAL_OK)
            return SM_OPT_READ_KEY_DELAY;
        if (buffer[0] == buffer2[0] && buffer[1] == buffer2[1] && buffer[2] == buffer2[2])
        {
            BACKUP_SET(KeyData, buffer[0] | (buffer[1] << 8) | (buffer[2] << 16));
            return SM_OPT_ON_KEY_PRESSED;
        }
    }

    return SM_OPT_READ_KEY_DELAY;
}

SM_STATE(SM_OPT_READ_KEY_DELAY)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_READ_KEY_INPUT:
    case SM_OPT_ON_KEY_PRESSED:
    case SM_OPT_SAVE_AND_EXIT_EDIT:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_READ_KEY_DELAY);

    constexpr uint32_t kEditingDelay = 20;
    HAL_Delay(kEditingDelay);
    return SM_OPT_IS_EDITING;
}

SM_STATE(SM_OPT_ON_KEY_PRESSED)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_READ_KEY_INPUT:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_ON_KEY_PRESSED);

    uint32_t key_data;
    BACKUP_GET(KeyData, key_data);
    if (!BACKUP_IS_VALID(KeyData))
    {
        BACKUP_SET(KeyData, 0);
        key_data = 0;
    }

    uint32_t key = key_data & 0xff;
    if (key == 0)
        return SM_OPT_READ_KEY_DELAY;
    
    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_READ_KEY_DELAY;
    }

    switch (key)
    {
    case ZLG7290_KEY_0: BACKUP_SET(KeyNum, 0); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_1: BACKUP_SET(KeyNum, 1); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_2: BACKUP_SET(KeyNum, 2); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_3: BACKUP_SET(KeyNum, 3); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_4: BACKUP_SET(KeyNum, 4); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_5: BACKUP_SET(KeyNum, 5); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_6: BACKUP_SET(KeyNum, 6); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_7: BACKUP_SET(KeyNum, 7); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_8: BACKUP_SET(KeyNum, 8); return SM_OPT_UPDATE_KEYNUM;
    case ZLG7290_KEY_9: BACKUP_SET(KeyNum, 9); return SM_OPT_UPDATE_KEYNUM;
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
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_UPDATE_KEYNUM);

    if (!BACKUP_IS_VALID(KeyNum))
        return SM_OPT_UPDATE_DISPLAY;
    uint32_t keynum;
    BACKUP_GET(KeyNum, keynum);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!BACKUP_IS_VALID(EditTarget))
    {
        BACKUP_SET(EditTarget, 0);
        BACKUP_SET(CursorPos, 0);
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!BACKUP_IS_VALID(EditTemperate))
    {
        BACKUP_SET(EditTemperate, 0);
        BACKUP_SET(CursorPos, 0);
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }
    uint32_t new_value;
    BACKUP_GET(EditTemperate, new_value);

    if (!BACKUP_IS_VALID(CursorPos))
    {
        BACKUP_SET(CursorPos, 0);
        BACKUP_SET(IsEditing, 0);
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
    
    BACKUP_SET(EditTemperate, new_value);

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_TARGET_LOW)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_SWITCH_TARGET_LOW);

    BACKUP_SET(IsEditing, 1);
    BACKUP_SET(EditTarget, 0);
    BACKUP_SET(CursorPos, 0);

    if (!BACKUP_IS_VALID(TemperatureLow))
        BACKUP_SET(TemperatureLow, SM_TEMPERATURE_LOW_INIT);

    uint32_t temperate_low;
    BACKUP_GET(TemperatureLow, temperate_low);
    BACKUP_SET(EditTemperate, temperate_low / 8 % 1000000);
    
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_TARGET_HIGH)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_SWITCH_TARGET_HIGH);

    BACKUP_SET(IsEditing, 1);
    BACKUP_SET(EditTarget, 1);
    BACKUP_SET(CursorPos, 0);

    if (!BACKUP_IS_VALID(TemperatureHigh))
        BACKUP_SET(TemperatureHigh, SM_TEMPERATURE_HIGH_INIT);
    
    uint32_t temperate_high;
    BACKUP_GET(TemperatureHigh, temperate_high);
    BACKUP_SET(EditTemperate, temperate_high / 8 % 1000000);

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_MOVE_CURSOR_LEFT)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_MOVE_CURSOR_LEFT);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!BACKUP_IS_VALID(CursorPos))
        BACKUP_SET(CursorPos, 0);

    uint32_t cur_pos;
    BACKUP_GET(CursorPos, cur_pos);
    if (cur_pos > 0)
        BACKUP_SET(CursorPos, cur_pos - 1);

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_MOVE_CURSOR_RIGHT)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_MOVE_CURSOR_RIGHT);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!BACKUP_IS_VALID(CursorPos))
        BACKUP_SET(CursorPos, 0);

    uint32_t cur_pos;
    BACKUP_GET(CursorPos, cur_pos);
    if (cur_pos < 5)
        BACKUP_SET(CursorPos, cur_pos + 1);

    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SWITCH_EDIT_MODE)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_SWITCH_EDIT_MODE);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }
    uint32_t is_editing;
    BACKUP_GET(IsEditing, is_editing);

    if (is_editing)
    {
        BACKUP_SET(IsEditing, 0);
        BACKUP_SET(CursorPos, 0);
        BACKUP_SET(EditTarget, 0);
    }
    else
    {
        BACKUP_SET(IsEditing, 1);
        BACKUP_SET(CursorPos, 0);
        BACKUP_SET(EditTarget, 0);
        if (!BACKUP_IS_VALID(TemperatureLow))
            BACKUP_SET(TemperatureLow, SM_TEMPERATURE_LOW_INIT);
        uint32_t temperature_low;
        BACKUP_GET(TemperatureLow, temperature_low);
        BACKUP_SET(EditTemperate, temperature_low / 8 % 1000000);
    }
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_SAVE_AND_EXIT_EDIT)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_SAVE_AND_EXIT_EDIT);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    if (!BACKUP_IS_VALID(IsEditing))
        return SM_OPT_READ_KEY_DELAY;

    BACKUP_SET(IsEditing, 0);
    BACKUP_SET(CursorPos, 0);
    if (!BACKUP_IS_VALID(EditTarget))
    {
        BACKUP_SET(EditTarget, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }
    if (!BACKUP_IS_VALID(EditTemperate))
    {
        BACKUP_SET(EditTemperate, 0);
        return SM_OPT_UPDATE_DISPLAY;
    }

    uint32_t current_temp;
    BACKUP_GET(EditTemperate, current_temp);
    current_temp *= 8;
    uint32_t edit_target;
    BACKUP_GET(EditTarget, edit_target);

    if (edit_target == 0)
    {
        uint32_t temperate_high;
        BACKUP_GET(TemperatureHigh, temperate_high);
        if (current_temp > temperate_high)
            current_temp = temperate_high;
        BACKUP_SET(TemperatureLow, current_temp);
    }
    else
    {
        uint32_t temperate_low;
        BACKUP_GET(TemperatureLow, temperate_low);
        if (current_temp < temperate_low)
            current_temp = temperate_low;
        BACKUP_SET(TemperatureHigh, current_temp);
    }
    return SM_OPT_UPDATE_DISPLAY;
}

SM_STATE(SM_OPT_UPDATE_DISPLAY)
{
    uint32_t last_step;
    BACKUP_GET(LastStep, last_step);
    switch (last_step)
    {
    case SM_OPT_ON_KEY_PRESSED:
    case SM_OPT_UPDATE_KEYNUM:
    case SM_OPT_SWITCH_TARGET_LOW:
    case SM_OPT_SWITCH_TARGET_HIGH:
    case SM_OPT_MOVE_CURSOR_LEFT:
    case SM_OPT_MOVE_CURSOR_RIGHT:
    case SM_OPT_SWITCH_EDIT_MODE:
    case SM_OPT_SAVE_AND_EXIT_EDIT:
        break;
    default:
        return SM_OPT_RESETHANDLER;
    }
    BACKUP_SET(LastStep, SM_OPT_UPDATE_DISPLAY);

    if (!BACKUP_IS_VALID(IsEditing))
    {
        BACKUP_SET(IsEditing, 0);
        return SM_OPT_IS_EDITING;
    }

    uint32_t is_editing;
    BACKUP_GET(IsEditing, is_editing);
    if (is_editing)
    {
        constexpr uint8_t display_table[10] 
        {
            ZLG7290_DISPLAY_NUM0, ZLG7290_DISPLAY_NUM1, ZLG7290_DISPLAY_NUM2, ZLG7290_DISPLAY_NUM3,
            ZLG7290_DISPLAY_NUM4, ZLG7290_DISPLAY_NUM5, ZLG7290_DISPLAY_NUM6, ZLG7290_DISPLAY_NUM7,
            ZLG7290_DISPLAY_NUM8, ZLG7290_DISPLAY_NUM9
        };
        if (!BACKUP_IS_VALID(EditTemperate))
            BACKUP_SET(EditTemperate, 0);
        if (!BACKUP_IS_VALID(CursorPos))
            BACKUP_SET(CursorPos, 0);
        uint32_t temp;
        BACKUP_GET(EditTemperate, temp);
        uint32_t cursor;
        BACKUP_GET(CursorPos, cursor);

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
        
        BACKUP_SET(IsEditing, 1);
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
    BACKUP_SET(LastStep, SM_OPT_RESETHANDLER);
    Reset_Handler();
    __builtin_unreachable();
    return SM_OPT_IS_EDITING;
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
        BACKUP_SET(KeyPressed, 1);
}

extern "C" void HAL_Delay(uint32_t delay)
{
    const uint32_t tick_start = HAL_GetTick();
    uint32_t delay_time = delay;

    // Add a freq to guarantee minimum wait
    if (delay_time < HAL_MAX_DELAY)
        delay_time += uwTickFreq;

    // Enter sleep mode for CPU to save power, but wait for delay ends at the same time
    while ((HAL_GetTick() - tick_start) < delay_time)
        ;
}
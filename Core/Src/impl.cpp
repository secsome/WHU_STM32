#include "impl.h"

#include "i2c.h"
#include "beep.h"
#include "zlg7290.h"
#include "lm75a.h"

#include "critical_data.hpp"

extern "C" uint8_t KeyPressed;
static critical_data<double> TemperatureLow;
static critical_data<double> TemperatureHigh;
static critical_data<double> TemperatureCurrent;
static critical_data<uint32_t> IsEditing;

static constexpr bool Impl_CloseEnough(double a, double b)
{
    constexpr double eps = 0.01;
    double diff = a - b;
    return diff < eps && diff > -eps;
}

template<typename T, size_t N, typename Fn>
static constexpr T Impl_ReadAverData(T err, Fn&& func)
{
    T result = T{};
    for (size_t i = 0; i < N; ++i)
    {
        T tmp = func();
        if (tmp == err)
            return err;
    }
    return result / N;
}

static void Impl_OnKeyDown(uint8_t key, uint8_t repeat, uint8_t func_key);
static void Impl_OnTemperatureChanged(double value);
static void Impl_OnTemperatureOutOfRange();

void Impl_OnLoopPrepare()
{
    do
    {
        TemperatureLow = 28.0;
        TemperatureHigh = 32.0;
        TemperatureCurrent = (TemperatureLow + TemperatureHigh) / 2;
        if (TemperatureLow > TemperatureHigh)
            continue;
    } while (!TemperatureLow.is_valid() || !TemperatureHigh.is_valid() || !TemperatureCurrent.is_valid());  
    do
    {
        IsEditing = 0;
    } while (!IsEditing.is_valid());
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
            Impl_OnKeyDown(buffer[0], buffer[1], buffer[2]);

        constexpr uint32_t kEditingDelay = 50;
        HAL_Delay(kEditingDelay);
    }

    if (IsEditing != 0)
    {
        const uint16_t temp = Impl_ReadAverData<uint16_t, 5>(LM75A_RESULT_ERROR, LM75A_GetTemp);
        if (temp != LM75A_RESULT_ERROR)
        {
            const double value = LM75A_ParseTemp(temp);
            if (Impl_CloseEnough(TemperatureCurrent, value))
                Impl_OnTemperatureChanged(value);
        }

        if (TemperatureCurrent < TemperatureLow || TemperatureCurrent > TemperatureHigh)
            Impl_OnTemperatureOutOfRange();
        
        // System delay time in milliseconds
        constexpr uint32_t kSystemDelay = 5000;
        HAL_Delay(kSystemDelay);
    }
}

static void Impl_OnKeyDown(uint8_t key, uint8_t repeat, uint8_t func_key)
{
    // TODO: Implement key handling
}

static void Impl_OnTemperatureChanged(double value)
{
    TemperatureCurrent = value;
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
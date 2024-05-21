#include "zlg7290.h"

__IO __WEAK uint32_t ZLG7290_I2C_Timeout = ZLG7290_TIMEOUT_LONG;
__IO __WEAK uint32_t ZLG7290_I2C_Retries = 5;

void ZLG7290_Set_Timeout(uint32_t timeout)
{
    ZLG7290_I2C_Timeout = timeout;
}

void ZLG7290_Set_Retries(uint32_t retries)
{
    ZLG7290_I2C_Retries = retries;
}

HAL_StatusTypeDef ZLG7290_Read(I2C_HandleTypeDef* hi2c, uint16_t addr, uint8_t* buf, uint16_t bufsz)
{
    HAL_StatusTypeDef status = HAL_OK;
    for (uint32_t i = 0; i < ZLG7290_I2C_Retries; ++i)
    {
        status = HAL_I2C_Mem_Read(hi2c, ZLG7290_SLVAEADDR, addr, I2C_MEMADD_SIZE_8BIT, buf, bufsz, ZLG7290_I2C_Timeout);
        if (status == HAL_OK)
            break;
    }
    return status;
}

static HAL_StatusTypeDef ZLG7290_WriteByte(I2C_HandleTypeDef* hi2c, uint16_t addr, uint8_t* buf)
{
    return HAL_I2C_Mem_Write(hi2c, ZLG7290_SLVAEADDR, addr, I2C_MEMADD_SIZE_8BIT, buf, 1, ZLG7290_I2C_Timeout);
}

HAL_StatusTypeDef ZLG7290_Write(I2C_HandleTypeDef* hi2c, uint16_t addr, uint8_t* buf, uint16_t bufsz)
{
    HAL_StatusTypeDef status = HAL_OK;
    for (uint32_t i = 0; i < ZLG7290_I2C_Retries; ++i)
    {
        for (uint32_t j = 1; j <= bufsz; ++j)
        {
            status |= ZLG7290_WriteByte(hi2c, addr + bufsz - j, buf + bufsz - j);
            HAL_Delay(5);
        }
        if (status == HAL_OK)
            break;
    }
    return status;
}

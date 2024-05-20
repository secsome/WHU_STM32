#ifndef __ZLG7290_H
#define __ZLG7290_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

enum
{
    ZLG7290_SLVAEADDR = 0x70
};

enum
{
    ZLG7290_ADDR_SYSREG     = 0x00,
    ZLG7290_ADDR_KEY        = 0x01,
    ZLG7920_ADDR_REPCNT     = 0x02,
    ZLG7290_ADDR_FUNCKEY    = 0x03,
    ZLG7290_ADDR_CMDBUF0    = 0x07,
    ZLG7290_ADDR_CMDBUF1    = 0x08,
    ZLG7290_ADDR_FLASH      = 0x0C,
    ZLG7290_ADDR_SCANNUM    = 0x0D,
    ZLG7290_ADDR_DPRAM0     = 0x10,
    ZLG7290_ADDR_DPRAM1     = 0x11,
    ZLG7290_ADDR_DPRAM2     = 0x12,
    ZLG7290_ADDR_DPRAM3     = 0x13,
    ZLG7290_ADDR_DPRAM4     = 0x14,
    ZLG7290_ADDR_DPRAM5     = 0x15,
    ZLG7290_ADDR_DPRAM6     = 0x16,
    ZLG7290_ADDR_DPRAM7     = 0x17,
};

#define ZLG7290_TIMEOUT_FLAG    ((uint32_t)0x1000)
#define ZLG7290_TIMEOUT_LONG    ((uint32_t)0xffff)

void ZLG7290_Set_Retries(uint32_t retries);
void ZLG7290_Set_Timeout(uint32_t timeout);

HAL_StatusTypeDef ZLG7290_Read(I2C_HandleTypeDef* hi2c, uint16_t addr, uint8_t* buf, uint16_t bufsz);
HAL_StatusTypeDef ZLG7290_Write(I2C_HandleTypeDef* hi2c, uint16_t addr, uint8_t* buf, uint16_t bufsz);

#ifdef __cplusplus
}
#endif

#endif
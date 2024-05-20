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

enum
{
    ZLG7290_DISPLAY_DOT         = 1 << 0,
    ZLG7290_DISPLAY_MIDDLE      = 1 << 1,
    ZLG7290_DISPLAY_LEFTTOP     = 1 << 2,
    ZLG7290_DISPLAY_LEFTBOTTOM  = 1 << 3,
    ZLG7290_DISPLAY_BOTTOM      = 1 << 4,
    ZLG7290_DISPLAY_RIGHTBOTTOM = 1 << 5,
    ZLG7290_DISPLAY_RIGHTTOP    = 1 << 6,
    ZLG7290_DISPLAY_TOP         = 1 << 7,
    ZLG7290_DISPLAY_ALL         = 0xFF,
    
    ZLG7290_DISPLAY_NUM0    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_MIDDLE,
    ZLG7290_DISPLAY_NUM1    = ZLG7290_DISPLAY_LEFTTOP | ZLG7290_DISPLAY_LEFTBOTTOM,
    ZLG7290_DISPLAY_NUM2    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_LEFTTOP & ~ZLG7290_DISPLAY_RIGHTBOTTOM,
    ZLG7290_DISPLAY_NUM3    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_LEFTTOP & ~ZLG7290_DISPLAY_LEFTBOTTOM,
    ZLG7290_DISPLAY_NUM4    = ZLG7290_DISPLAY_LEFTTOP | ZLG7290_DISPLAY_MIDDLE | ZLG7290_DISPLAY_RIGHTTOP | ZLG7290_DISPLAY_RIGHTBOTTOM,
    ZLG7290_DISPLAY_NUM5    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_LEFTBOTTOM & ~ZLG7290_DISPLAY_RIGHTTOP,
    ZLG7290_DISPLAY_NUM6    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_RIGHTTOP,
    ZLG7290_DISPLAY_NUM7    = ZLG7290_DISPLAY_TOP | ZLG7290_DISPLAY_RIGHTTOP | ZLG7290_DISPLAY_RIGHTBOTTOM,
    ZLG7290_DISPLAY_NUM8    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT,
    ZLG7290_DISPLAY_NUM9    = ZLG7290_DISPLAY_ALL & ~ZLG7290_DISPLAY_DOT & ~ZLG7290_DISPLAY_LEFTBOTTOM,
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
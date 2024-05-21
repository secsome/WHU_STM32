#ifndef __LM75A_H
#define __LM75A_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

// 寄存器指针地址
#define LM75A_ADDR_TEMP		0x00 // 温度寄存器指针地址
#define LM75A_ADDR_CONF		0x01 // 配置寄存器指针地址
#define LM75A_ADDR_THYST	0x02 // 滞后寄存器指针地址
#define LM75A_ADDR_TOS		0x03 // 过热关断寄存器指针地址

// 器件工作模式
#define LM75A_MODE_SHUTDOWN	0x01  //关断模式
#define LM75A_MODE_WORKING	0x00  //正常工作模式

// OS工作模式
#define LM75A_OSMODE_IRQ	0x02 // OS中断模式
#define LM75A_OSMODE_COMP	0x00 // OS比较器模式

// OS极性选择
#define LM75A_OS_HIGH	0x04 // OS高电平有效
#define LM75A_OS_LOW	0x00 // OS低电平有效

// OS故障队列
#define LM75A_EQ_DEFAULT 0x00

#define LM75A_RESULT_OK (uint16_t)0
#define LM75A_RESULT_ERROR (uint16_t)-1

// Functions
uint8_t LM75A_SetMode(uint8_t reg, uint8_t mode);

typedef uint16_t lm75a_temp_t;
lm75a_temp_t LM75A_GetTemp();

double LM75A_ParseTemp(lm75a_temp_t);

#endif

#ifdef __cplusplus
}
#endif
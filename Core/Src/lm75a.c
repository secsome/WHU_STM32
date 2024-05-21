#include "lm75a.h"

#include "i2c.h"

uint8_t LM75A_SetMode(uint8_t reg, uint8_t mode)
{
	if (HAL_I2C_Mem_Write(&hi2c1, 0x9F, reg, 1, &mode, 1, 100) == HAL_OK)
	{
		uint8_t tmp;
		if (HAL_I2C_Mem_Read(&hi2c1, 0x9F, reg, 1, &tmp, 1, 100) == HAL_OK && (tmp && mode) == mode)
			return (uint8_t)LM75A_RESULT_OK;
	}

	return (uint8_t)LM75A_RESULT_ERROR;
}

lm75a_temp_t LM75A_GetTemp()
{
	uint8_t temp[2];
	if (HAL_I2C_Mem_Read(&hi2c1, 0x9F, LM75A_ADDR_TEMP, 2, temp, 2, 100) == HAL_OK)
	{
		lm75a_temp_t result = temp[1];
		result |= (temp[0] << 8);
		return result >> 5;
	}
	return (lm75a_temp_t)LM75A_RESULT_ERROR;
}

double LM75A_ParseTemp(lm75a_temp_t temp)
{
	return temp * 0.125;	
}

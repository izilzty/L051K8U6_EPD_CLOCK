#ifndef _IIC_H_
#define _IIC_H_

#include "main.h"

#include "i2c.h"

#define I2C_TIMEOUT_MS 1000

#define I2C_NUM I2C1

#define I2C_PULLUP_PORT I2C1_PULLUP_GPIO_Port
#define I2C_PULLUP_PIN I2C1_PULLUP_Pin

#define I2C_SCL_PORT GPIOB
#define I2C_SCL_PIN LL_GPIO_PIN_6

#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN LL_GPIO_PIN_7

#define I2C_INIT_FUNC MX_I2C1_Init

uint8_t I2C_Start(uint8_t addr, uint8_t is_read, uint8_t data_size);
uint8_t I2C_Stop(void);
uint8_t I2C_WriteByte(uint8_t byte);
uint8_t I2C_ReadByte(void);

#endif

#ifndef __IIC_PERF_H__
#define __IIC_PERF_H__
#include <stdio.h>
#include "driver/gpio.h"

#define SDA_PIN 26
#define SCL_PIN 25
#define DEV_ADR  0x42 			 /*设备地址定义*/

#define SCL_H      GPIO.out_w1ts = (1 << SCL_PIN)
#define SCL_L      GPIO.out_w1tc = (1 << SCL_PIN)
   
#define SDA_H      GPIO.out_w1ts = (1 << SDA_PIN)
#define SDA_L      GPIO.out_w1tc = (1 << SDA_PIN)

#define SCL_read   (GPIO.in >> SCL_PIN) & 0x1
#define SDA_read   (GPIO.in >> SDA_PIN) & 0x1


#define DISABLE 0
#define ENABLE 1
void iic_init(void);
int SCCB_ReadByte(uint8_t* pBuffer, uint16_t length, uint8_t ReadAddress);
int SCCB_WriteByte( uint16_t WriteAddress , uint8_t SendByte );
#endif

/*
 *  FDC2214.h
 *
 *  Created on: May 22, 2026
 *      Author: Tor Kaufmann Gjerde
 */


#ifndef INC_FDC2214_H_
#define INC_FDC2214_H_

#include "main.h"
#include <stdbool.h>

#define ERROR_FDC2214_INIT	0x20U


uint8_t fdc2214_0_init(void);
uint8_t fdc2214_1_init(void);
uint8_t fdc2214_2_init(void);
uint8_t fdc2214_3_init(void);

void fdc2214_read_device_id(uint16_t *value);

HAL_StatusTypeDef fdc2214_read_channels(I2C_HandleTypeDef *hi2c,
										uint16_t addr,
										uint32_t *ch0,
										uint32_t *ch1,
										uint32_t *ch2,
										uint32_t *ch3 );

uint32_t fdc2214_read_ch(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t ch);

#endif


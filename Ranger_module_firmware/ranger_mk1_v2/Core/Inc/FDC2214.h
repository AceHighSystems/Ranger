/*
 *  FDC2214.h
 *
 *  Created on: May 22, 2026
 *      Author: Tor Kaufmann Gjerde
 */


#ifndef INC_FDC2214_H_
#define INC_FDC2214_H_

#include "main.h"

void fdc2214_write_register(uint8_t reg, uint16_t value);
uint16_t fdc2214_read_register(uint8_t reg);
void fdc2214_init(void);
uint32_t fdc2214_read_ch0_raw(void);
uint16_t fdc2214_read_device_id(void);

#endif

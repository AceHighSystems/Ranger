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


uint8_t fdc2214_init(void);

void fdc2214_read_device_id(uint16_t *value);

#endif


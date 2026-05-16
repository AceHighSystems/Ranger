/*
 *  DRV8465.h
 *
 *  Created on: May 11, 2026
 *      Author: Tor Kaufmann Gjerde
 */


#ifndef INC_DRV8462_H_
#define INC_DRV8462_H_

#include "main.h"

void drv8462_init_fullstep_spi_mode(void);
void drv8462_set_direction(uint8_t direction_is_forward);
void drv8462_step_once(uint8_t direction_is_forward);
void drv8462_step_once_HW(uint8_t direction);

#endif

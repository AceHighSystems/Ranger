/*
 *  INA229.h
 *
 *  Created on: May 19, 2026
 *      Author: Tor Kaufmann Gjerde
 */


#ifndef INC_INA229_H_
#define INC_INA229_H_


void ina229_init(void);
float ina229_read_bus_voltage_V(void);
float ina229_read_current_A(void);


#endif

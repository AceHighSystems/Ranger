
/*
 *  FDC2214.c
 *
 *  Created on: May 22, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#include "main.h"

#define FDC2214_ADDR_LOW   (0x2A << 1)
#define FDC2214_ADDR_HIGH  (0x2B << 1)

#define FDC2214_ADDR       FDC2214_ADDR_LOW

void fdc2214_write_register(uint8_t reg, uint16_t value);
uint16_t fdc2214_read_register(uint8_t reg);
void fdc2214_init(void);
uint16_t fdc2214_read_device_id(void);
uint32_t fdc2214_read_ch0_raw(void);


extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

void fdc2214_init(void)
{
    fdc2214_write_register(0x1A, 0x1C01); // CONFIG
    fdc2214_write_register(0x1B, 0x020D); // MUX_CONFIG

    fdc2214_write_register(0x08, 0xFFFF); // RCOUNT_CH0
    fdc2214_write_register(0x10, 0x0400); // SETTLECOUNT_CH0
    fdc2214_write_register(0x14, 0x1001); // CLOCK_DIVIDERS_CH0
    fdc2214_write_register(0x1E, 0x7800); // DRIVE_CURRENT_CH0
}


void fdc2214_write_register(uint8_t reg, uint16_t value)
{
    uint8_t tx[3];

    tx[0] = reg;
    tx[1] = (value >> 8) & 0xFF;
    tx[2] = value & 0xFF;

    HAL_I2C_Master_Transmit(&hi2c1, FDC2214_ADDR, tx, 3, HAL_MAX_DELAY);
}


uint16_t fdc2214_read_register(uint8_t reg)
{
    uint8_t tx = reg;
    uint8_t rx[2];

    HAL_I2C_Master_Transmit(&hi2c1, FDC2214_ADDR, &tx, 1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(&hi2c1, FDC2214_ADDR, rx, 2, HAL_MAX_DELAY);

    return ((uint16_t)rx[0] << 8) | rx[1];
}


uint32_t fdc2214_read_ch0_raw(void)
{
    uint16_t msb = fdc2214_read_register(0x00);
    uint16_t lsb = fdc2214_read_register(0x01);

    uint32_t value = 0;

    value |= ((uint32_t)(msb & 0x0FFF) << 16);
    value |= lsb;

    return value;
}


uint16_t fdc2214_read_device_id(void)
{
    return fdc2214_read_register(0x7F);
}


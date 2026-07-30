
/*
 *  FDC2214.c
 *
 *  Created on: May 22, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#include "main.h"
#include "FDC2214.h"
#include <stdbool.h>

#define FDC2214_ADDR_LOW   (0x2A << 1)
#define FDC2214_ADDR_HIGH  (0x2B << 1)

#define FDC2214_ADDR       FDC2214_ADDR_LOW

static HAL_StatusTypeDef fdc2214_write_register(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t reg, uint16_t value);
static HAL_StatusTypeDef fdc2214_read_register(I2C_HandleTypeDef *hi2c, uint16_t addr , uint8_t reg, uint16_t *value);


void fdc2214_read_device_id(uint16_t *value);
uint32_t fdc2214_read_ch0_raw(void);


extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;


uint8_t fdc2214_0_init(void)
{
	uint16_t id = 0;

	if (fdc2214_read_register(&hi2c1, FDC2214_ADDR_LOW, 0x7F, &id) != HAL_OK)
	{
	    return 2;
	}

	if (id != 0x3055)   // expected FDC2214 device ID
	{
	    return 3;
	}

    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x1A, 0x1E01 | 0x2000) != HAL_OK) return 4; // Configure while in sleep first, external clock + sleep mode

    // ch0
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x08, 0x1000) != HAL_OK) return 4; // RCOUNT
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x10, 0x1000) != HAL_OK) return 4; // SETTLECOUNT
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x14, 0x1002) != HAL_OK) return 4; // CLOCK_DIVIDERS
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x1E, 0xA000) != HAL_OK) return 4; // DRIVE_CURRENT

    // ch1
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x09, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x11, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x15, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x1F, 0xA000) != HAL_OK) return 4;

    // ch2
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x0A, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x12, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x16, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x20, 0xA000) != HAL_OK) return 4;

    // ch3
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x0B, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x13, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x17, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x21, 0xA000) != HAL_OK) return 4;

    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x1B, 0xC20D) != HAL_OK) return 4; // Autoscan CH0, CH1, CH2, CH3, deglitch 10 MHz
    if (fdc2214_write_register(&hi2c1, FDC2214_ADDR_LOW, 0x1A, 0x1E01) != HAL_OK) return 4; // Exit sleep, external CLKIN

    return 1;
}

uint8_t fdc2214_1_init(void)
{
	uint16_t id = 0;

	if (fdc2214_read_register(&hi2c2, FDC2214_ADDR_HIGH, 0x7F, &id) != HAL_OK)
	{
	    return 2;
	}

	if (id != 0x3055)   // expected FDC2214 device ID
	{
	    return 3;
	}

    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x1A, 0x1E01 | 0x2000) != HAL_OK) return 4; // Configure while in sleep first, external clock + sleep mode

    // ch0
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x08, 0x1000) != HAL_OK) return 4; // RCOUNT
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x10, 0x1000) != HAL_OK) return 4; // SETTLECOUNT
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x14, 0x1002) != HAL_OK) return 4; // CLOCK_DIVIDERS
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x1E, 0xA000) != HAL_OK) return 4; // DRIVE_CURRENT

    // ch1
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x09, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x11, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x15, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x1F, 0xA000) != HAL_OK) return 4;

    // ch2
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x0A, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x12, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x16, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x20, 0xA000) != HAL_OK) return 4;

    // ch3
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x0B, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x13, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x17, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x21, 0xA000) != HAL_OK) return 4;

    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x1B, 0xC20D) != HAL_OK) return 4; // Autoscan CH0, CH1, CH2, CH3, deglitch 10 MHz
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_HIGH, 0x1A, 0x1E01) != HAL_OK) return 4; // Exit sleep, external CLKIN

    return 1;
}


uint8_t fdc2214_2_init(void)
{
	uint16_t id = 0;

	if (fdc2214_read_register(&hi2c2, FDC2214_ADDR_LOW, 0x7F, &id) != HAL_OK)
	{
	    return 2;
	}

	if (id != 0x3055)   // expected FDC2214 device ID
	{
	    return 3;
	}

    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x1A, 0x1E01 | 0x2000) != HAL_OK) return 4; // Configure while in sleep first, external clock + sleep mode

    // ch0
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x08, 0x1000) != HAL_OK) return 4; // RCOUNT
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x10, 0x1000) != HAL_OK) return 4; // SETTLECOUNT
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x14, 0x1002) != HAL_OK) return 4; // CLOCK_DIVIDERS
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x1E, 0xA000) != HAL_OK) return 4; // DRIVE_CURRENT

    // ch1
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x09, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x11, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x15, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x1F, 0xA000) != HAL_OK) return 4;

    // ch2
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x0A, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x12, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x16, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x20, 0xA000) != HAL_OK) return 4;

    // ch3
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x0B, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x13, 0x1000) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x17, 0x1002) != HAL_OK) return 4;
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x21, 0xA000) != HAL_OK) return 4;

    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x1B, 0xC20D) != HAL_OK) return 4; // Autoscan CH0, CH1, CH2, CH3, deglitch 10 MHz
    if (fdc2214_write_register(&hi2c2, FDC2214_ADDR_LOW, 0x1A, 0x1E01) != HAL_OK) return 4; // Exit sleep, external CLKIN

    return 1;
}



static HAL_StatusTypeDef fdc2214_write_register(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t reg, uint16_t value)
{
    uint8_t tx[3];

    tx[0] = reg;
    tx[1] = (value >> 8) & 0xFF;
    tx[2] = value & 0xFF;

    return HAL_I2C_Master_Transmit(hi2c, addr, tx, 3, 100);
}


static HAL_StatusTypeDef fdc2214_read_register(I2C_HandleTypeDef *hi2c, uint16_t addr , uint8_t reg, uint16_t *value)
{
    uint8_t rx[2];

    if (HAL_I2C_Master_Transmit(hi2c, addr, &reg, 1, 100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_I2C_Master_Receive(hi2c, addr, rx, 2, 100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    *value = ((uint16_t)rx[0] << 8) | rx[1];

    return HAL_OK;
}


uint32_t fdc2214_read_ch(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t ch)
{
	uint8_t reg_msb = ch * 2;
	uint8_t reg_lsb = reg_msb + 1;

	uint16_t msb;
	uint16_t lsb;

    fdc2214_read_register(hi2c, addr, reg_msb, &msb);
    fdc2214_read_register(hi2c, addr, reg_lsb, &lsb);

    uint32_t value = 0;

    value |= ((uint32_t)(msb & 0x0FFF) << 16);
    value |= lsb;

    return value;
}

/*
void fdc2214_read_device_id(uint16_t *value)
{
    fdc2214_read_register(0x7F, value);
}
*/

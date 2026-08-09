/*
 *  DRV8465.c
 *
 *  Created on: May 11, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#include "INA229.h"
#include "main.h"
#include "stm32g4xx_hal.h"
#include <stdint.h>

/*
 * Assumes CubeMX has already created and initialized hspi1 or hspi2.
 * IMPORTANT: INA229 SPI should be MSB first.
 * Datasheet timing corresponds to SPI mode 1: CPOL = 0, CPHA = 1.
 */
extern SPI_HandleTypeDef hspi2;

/* ============================================================
 * User hardware configuration
 * ============================================================
 */

#define INA229_CS_GPIO_PORT       GPIOB
#define INA229_CS_PIN             GPIO_PIN_9

/*
 * Change these to match your hardware.
 *
 * Example:
 * 10 mOhm shunt, max expected current 10 A.
 */
#define INA229_SHUNT_RESISTANCE_OHM      0.016f
#define INA229_MAX_EXPECTED_CURRENT_A    10.0f

/* ============================================================
 * INA229 register addresses
 * ============================================================
 */

#define INA229_REG_CONFIG         0x00
#define INA229_REG_ADC_CONFIG     0x01
#define INA229_REG_SHUNT_CAL      0x02
#define INA229_REG_VBUS           0x05
#define INA229_REG_CURRENT        0x07
#define INA229_REG_DEVICE_ID      0x3F
#define INA229_REG_DIETEMP        0x06

/* ============================================================
 * Scaling
 * ============================================================
 */

/*
 * CURRENT_LSB = Max expected current / 2^19
 */
#define INA229_CURRENT_LSB_A      (INA229_MAX_EXPECTED_CURRENT_A / 524288.0f)

/*
 * VBUS LSB = 195.3125 uV
 */
#define INA229_VBUS_LSB_V         0.0001953125f


#define INA229_DIETEMP_LSB_mC     7.8125f

/* ============================================================
 * Low-level chip select
 * ============================================================
 */

static void ina229_select(void)
{
    HAL_GPIO_WritePin(INA229_CS_GPIO_PORT,
                      INA229_CS_PIN,
                      GPIO_PIN_RESET);
}

static void ina229_deselect(void)
{
    HAL_GPIO_WritePin(INA229_CS_GPIO_PORT,
                      INA229_CS_PIN,
                      GPIO_PIN_SET);
}

/* ============================================================
 * Helper: sign extend 24-bit two's complement to int32_t
 * ============================================================
 */

static int32_t ina229_sign_extend_24(uint32_t raw)
{
    if (raw & 0x800000UL)
    {
        raw |= 0xFF000000UL;
    }

    return (int32_t)raw;
}

/* ============================================================
 * Low-level SPI access
 * ============================================================
 */

/*
 * INA229 SPI command byte:
 *
 * Bits [7:2] = register address
 * Bit  [1]   = 0
 * Bit  [0]   = R/W
 *
 * R/W = 0: write
 * R/W = 1: read
 */

static HAL_StatusTypeDef ina229_write_register_16(uint8_t reg, uint16_t value)
{
    uint8_t tx[3];

    tx[0] = (uint8_t)((reg & 0x3FU) << 2);   // write command
    tx[1] = (uint8_t)(value >> 8);           // MSB
    tx[2] = (uint8_t)(value & 0xFFU);        // LSB

    ina229_select();

    HAL_StatusTypeDef status =
        HAL_SPI_Transmit(&hspi2, tx, 3, 10);

    ina229_deselect();

    return status;
}

static uint16_t ina229_read_register_16(uint8_t reg)
{
    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    tx[0] = (uint8_t)(((reg & 0x3FU) << 2) | 0x01U);  // read command

    ina229_select();

    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 10);

    ina229_deselect();

    if (status != HAL_OK)
    {
        return 0xFFFFU;
    }

    return ((uint16_t)rx[1] << 8) | rx[2];
}

static uint32_t ina229_read_register_24(uint8_t reg)
{
    uint8_t tx[4] = {0};
    uint8_t rx[4] = {0};

    tx[0] = (uint8_t)(((reg & 0x3FU) << 2) | 0x01U);  // read command

    ina229_select();

    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, 10);

    ina229_deselect();

    if (status != HAL_OK)
    {
        return 0xFFFFFFFFUL;
    }

    return ((uint32_t)rx[1] << 16) |
           ((uint32_t)rx[2] << 8)  |
           ((uint32_t)rx[3]);
}

/* ============================================================
 * INA229 initialization
 * ============================================================
 */

void ina229_init(void)
{
    /*
     * Give the INA229 time after power-up.
     * Datasheet startup time is roughly 300 us, so 2 ms is safe.
     */
    HAL_Delay(2);

    /*
     * Optional software reset:
     * CONFIG bit 15 = RST.
     */
    ina229_write_register_16(INA229_REG_CONFIG, 0x8000);
    HAL_Delay(2);

    /*
     * ADC_CONFIG = 0xFB68
     *
     * MODE    = 0xF  continuous shunt + bus + temperature
     * VBUSCT  = 0x5  1052 us conversion time
     * VSHCT   = 0x5  1052 us conversion time
     * VTCT    = 0x5  1052 us conversion time
     * AVG     = 0x0  no averaging
     */
    ina229_write_register_16(INA229_REG_ADC_CONFIG, 0xFB68);

    /*
     * SHUNT_CAL = 13107.2e6 * CURRENT_LSB * RSHUNT
     *
     * For ADCRANGE = 0, this is used directly.
     * For ADCRANGE = 1, multiply SHUNT_CAL by 4.
     */
    float shunt_cal_f =
        13107.2e6f *
        INA229_CURRENT_LSB_A *
        INA229_SHUNT_RESISTANCE_OHM;

    uint16_t shunt_cal = (uint16_t)(shunt_cal_f + 0.5f);

    ina229_write_register_16(INA229_REG_SHUNT_CAL, shunt_cal);

    /*
     * Wait for first conversion cycle.
     * With shunt + bus + temp at 1052 us each, wait > 3.2 ms.
     */
    HAL_Delay(5);
}

/* ============================================================
 * Read bus voltage
 * ============================================================
 */

float ina229_read_volt(void)
{
    uint32_t raw = ina229_read_register_24(INA229_REG_VBUS);

    /*
     * VBUS is a 20-bit value inside a 24-bit register.
     * Drop the unused lower 4 bits.
     */
    raw = raw >> 4;

    return (float)raw * INA229_VBUS_LSB_V;
}

/* ============================================================
 * Read current
 * ============================================================
 */

float ina229_read_current(void)
{
    uint32_t raw = ina229_read_register_24(INA229_REG_CURRENT);

    raw = raw >> 4;   // CURRENT is bits 23:4

    int32_t raw_s24 = ina229_sign_extend_24(raw);

    return (float)raw_s24 * INA229_CURRENT_LSB_A;
}

/* ============================================================
 * Read Temperature
 * ============================================================
 */

float ina229_read_temp(void)
{
    uint16_t raw_u16 = ina229_read_register_16(INA229_REG_DIETEMP);

    int16_t raw_s16 = (int16_t)raw_u16;

    return (float)raw_s16 * INA229_DIETEMP_LSB_mC;;
}

/* ============================================================
 * Optional: read device ID for SPI bring-up test
 * ============================================================
 */

uint16_t ina229_read_device_id(void)
{
    return ina229_read_register_16(INA229_REG_DEVICE_ID);
}

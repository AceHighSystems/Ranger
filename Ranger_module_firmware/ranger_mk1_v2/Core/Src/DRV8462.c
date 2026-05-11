


#include "DRV8462.h"
#include "main.h"
#include "stm32g4xx_hal.h"

/*
 * This file assumes that CubeMX has already created and initialized hspi1.
 */
extern SPI_HandleTypeDef hspi2;


/* ============================================================
 * User hardware configuration
 * ============================================================
 */

/*
 * Chip Select pin for the DRV8462 SPI interface.
 * Change these to match your schematic / CubeMX setup.
 */
#define DRV8462_CS_GPIO_PORT      GPIOB
#define DRV8462_CS_PIN            GPIO_PIN_12


/* ============================================================
 * DRV8462 register addresses
 * ============================================================
 */

#define DRV8462_REG_CTRL1         0x04
#define DRV8462_REG_CTRL2         0x05
#define DRV8462_REG_CTRL3         0x06


/* ============================================================
 * CTRL1 register bits
 * ============================================================
 */

/*
 * EN_OUT enables the H-bridge outputs.
 *
 * Important:
 * The motor driver will not drive the motor unless:
 * - nSLEEP pin is high
 * - ENABLE pin is high
 * - VM motor supply is present
 * - EN_OUT bit is set to 1
 */
#define DRV8462_CTRL1_EN_OUT      (1U << 7)


/* ============================================================
 * CTRL2 register bits
 * ============================================================
 */

/*
 * DIR:
 * Sets the motor direction when SPI_DIR is enabled.
 */
#define DRV8462_CTRL2_DIR         (1U << 7)

/*
 * STEP:
 * Creates one motor step when written as 1.
 * The DRV8462 automatically clears this bit afterwards.
 */
#define DRV8462_CTRL2_STEP        (1U << 6)

/*
 * SPI_DIR:
 * 0 = direction controlled by external DIR pin
 * 1 = direction controlled by CTRL2 DIR bit
 */
#define DRV8462_CTRL2_SPI_DIR     (1U << 5)

/*
 * SPI_STEP:
 * 0 = stepping controlled by external STEP pin
 * 1 = stepping controlled by CTRL2 STEP bit
 */
#define DRV8462_CTRL2_SPI_STEP    (1U << 4)

/*
 * Micro-step mode bits.
 *
 * 0x00 = full-step, 100% current
 * 0x01 = full-step, 71% current
 *
 * For first bring-up, 100% full-step is simple and obvious.
 */
#define DRV8462_FULLSTEP_100_PERCENT_CURRENT   0x00
#define DRV8462_FULLSTEP_71_PERCENT_CURRENT    0x01


/* ============================================================
 * Low-level chip select helper functions
 * ============================================================
 */

static void drv8462_select(void)
{
    HAL_GPIO_WritePin(DRV8462_CS_GPIO_PORT,
                      DRV8462_CS_PIN,
                      GPIO_PIN_RESET);
}


static void drv8462_deselect(void)
{
    HAL_GPIO_WritePin(DRV8462_CS_GPIO_PORT,
                      DRV8462_CS_PIN,
                      GPIO_PIN_SET);
}


/* ============================================================
 * Low-level SPI access
 * ============================================================
 */

static uint8_t drv8462_write_register(uint8_t register_address, uint8_t register_data)
{
    uint8_t tx_buffer[2];
    uint8_t rx_buffer[2];

    /*
     * DRV8462 SPI write frame:
     *
     * Byte 0:
     * bit 7 = 0  normal frame
     * bit 6 = 0  write operation
     * bit 5..0 = register address
     *
     * Byte 1:
     * register data
     */
    tx_buffer[0] = register_address & 0x3F;
    tx_buffer[1] = register_data;

    drv8462_select();

    HAL_SPI_TransmitReceive(&hspi2,
                            tx_buffer,
                            rx_buffer,
                            2,
                            HAL_MAX_DELAY);

    drv8462_deselect();

    /*
     * During every SPI transaction, the DRV8462 returns a status byte.
     * This can later be decoded for fault checking.
     */
    return rx_buffer[0];
}


static uint8_t drv8462_read_register(uint8_t register_address)
{
    uint8_t tx_buffer[2];
    uint8_t rx_buffer[2];

    /*
     * DRV8462 SPI read frame:
     *
     * Byte 0:
     * bit 7 = 0  normal frame
     * bit 6 = 1  read operation
     * bit 5..0 = register address
     *
     * Byte 1:
     * dummy byte
     */
    tx_buffer[0] = 0x40 | (register_address & 0x3F);
    tx_buffer[1] = 0x00;

    drv8462_select();

    HAL_SPI_TransmitReceive(&hspi2,
                            tx_buffer,
                            rx_buffer,
                            2,
                            HAL_MAX_DELAY);

    drv8462_deselect();

    /*
     * For a register read, the returned register value is in byte 1.
     */
    return rx_buffer[1];
}


/* ============================================================
 * DRV8462 initialization
 * ============================================================
 */

void drv8462_init_fullstep_spi_mode(void)
{
    /*
     * Wait after waking the device.
     *
     * Before calling this function, make sure:
     * - nSLEEP GPIO is set high
     * - ENABLE GPIO is set high
     * - motor supply VM is present
     */
    HAL_Delay(2);

    /*
     * Optional fault clear.
     *
     * CTRL3 default is approximately 0x38.
     * Setting bit 7 clears latched faults.
     */
    drv8462_write_register(DRV8462_REG_CTRL3, 0xB8);

    /*
     * Configure CTRL2:
     *
     * SPI_DIR  = 1, direction controlled over SPI
     * SPI_STEP = 1, step command controlled over SPI
     * microstep mode = full-step, 100% current
     */
    uint8_t ctrl2_value = 0;

    ctrl2_value |= DRV8462_CTRL2_SPI_DIR;
    ctrl2_value |= DRV8462_CTRL2_SPI_STEP;
    ctrl2_value |= DRV8462_FULLSTEP_100_PERCENT_CURRENT;

    drv8462_write_register(DRV8462_REG_CTRL2, ctrl2_value);

    /*
     * Configure CTRL1:
     *
     * 0x0F is close to the default value.
     * Adding EN_OUT enables the motor outputs.
     */
    uint8_t ctrl1_value = 0x0F;

    ctrl1_value |= DRV8462_CTRL1_EN_OUT;

    drv8462_write_register(DRV8462_REG_CTRL1, ctrl1_value);
}


/* ============================================================
 * Direction control
 * ============================================================
 */

void drv8462_set_direction(uint8_t direction_is_forward)
{
    uint8_t ctrl2_value = 0;

    ctrl2_value |= DRV8462_CTRL2_SPI_DIR;
    ctrl2_value |= DRV8462_CTRL2_SPI_STEP;
    ctrl2_value |= DRV8462_FULLSTEP_100_PERCENT_CURRENT;

    if (direction_is_forward != 0)
    {
        ctrl2_value |= DRV8462_CTRL2_DIR;
    }

    drv8462_write_register(DRV8462_REG_CTRL2, ctrl2_value);
}


/* ============================================================
 * Single-step command over SPI
 * ============================================================
 */

void drv8462_step_once(uint8_t direction_is_forward)
{
    uint8_t ctrl2_value = 0;

    ctrl2_value |= DRV8462_CTRL2_SPI_DIR;
    ctrl2_value |= DRV8462_CTRL2_SPI_STEP;
    ctrl2_value |= DRV8462_FULLSTEP_100_PERCENT_CURRENT;

    if (direction_is_forward != 0)
    {
        ctrl2_value |= DRV8462_CTRL2_DIR;
    }

    /*
     * Writing STEP = 1 creates one step.
     * The DRV8462 clears this bit internally afterwards.
     */
    ctrl2_value |= DRV8462_CTRL2_STEP;

    drv8462_write_register(DRV8462_REG_CTRL2, ctrl2_value);
}

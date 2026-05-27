


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

static uint8_t drv8462_write_register(uint8_t reg, uint8_t data)
{
	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = (uint8_t)(reg & 0x3F);   // address / command byte
	tx[1] = data;                    // register data

	rx[0] = 0;
	rx[1] = 0;

    drv8462_select();

    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(&hspi2,
                                tx,
                                rx,
                                2,
                                10);

    drv8462_deselect();

    if (status != HAL_OK)
    {
        return 0xFF;
    }

    return rx[0];
}


static uint8_t drv8462_read_register(uint8_t reg)
{
	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = (uint8_t)(0x80U | (reg & 0x3F));  // adjust read bit if needed
	tx[1] = 0x00;

	rx[0] = 0;
	rx[1] = 0;

    drv8462_select();

    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(&hspi2,
                                tx,
                                rx,
                                2,
                                10);

    drv8462_deselect();

    if (status != HAL_OK)
    {
        return 0xFF;
    }

    return rx[1];
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
     * Internal VREF.
     *
     */
    drv8462_write_register(0x10, 0x12);
    drv8462_write_register(0x0E, 0x0A); // current limit - low torque/current for bring-up

    /*
     * Configure CTRL2:
     *
     * SPI_DIR  = 1, direction controlled over SPI
     * SPI_STEP = 1, step command controlled over SPI
     * microstep mode = full-step, 100% current
     */
    uint8_t ctrl2_value = 0;

    //ctrl2_value |= DRV8462_CTRL2_SPI_DIR;
    //ctrl2_value |= DRV8462_CTRL2_SPI_STEP;
    ctrl2_value |= 0x08; // micro-step

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
    ctrl2_value |= 0x08; // micro-step;

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
    ctrl2_value |= 0x08; // micro-step;

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

/* ============================================================
 * Single-step command over HW interface
 * ============================================================
 */
/* DRV8462:
 *  PB12 = CS (M0), PB13 = SLCK (M1), PB14 = MISO (Decay1), PB15 = MOSI (Decay2), PB2 = MODE
 *  PB2 = MODE, PB3 = STEP, PB4 = DIR */

void drv8462_step_once_HW(uint8_t direction)
{
	if(direction == 1){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET); // direction 1
	}
	if(direction == 0){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET); // direction 0
	}

	// Toggle the STEP pin with PWM output
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_Delay(5);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
	HAL_Delay(5);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

}

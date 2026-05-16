/*
 * ranger_app.h
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *  Description:
 *  Public interface for the Ranger application layer.
 */

#ifndef INC_RANGER_APP_H_
#define INC_RANGER_APP_H_

#include "ace_protocol.h"
#include "main.h"

/*
   *  PB2 = MODE
   *  PB3 = STEP
   *  PB4 = DIR
   *  PB10 = ENABLE
   *  PB11 = nSLEEP
   *  PB12 = CS (M0)
   *  PB13 = SLCK (M1), PB14 = MISO (Decay1), PB15 = MOSI (Decay2) Not used as std. GPIOs used by SPI driver
 */

#define DRV_ENABLE 		GPIO_PIN_10
#define DRV_SLEEP		GPIO_PIN_11
#define DRV_CS			GPIO_PIN_12
#define DRV_MODE		GPIO_PIN_2
#define DRV_STEP		GPIO_PIN_3
#define DRV_DIR			GPIO_PIN_4


/**
 * @brief Initialize Ranger application state.
 */
void ranger_app_init(void);

/**
 * @brief Run periodic Ranger application tasks.
 *
 * Called continuously from main loop.
 */
void ranger_app_tick(void);

/**
 * @brief Handle decoded Ace command frame.
 *
 * Called by ranger_can.c after a CAN command frame has been received
 * and decoded by ace_protocol.c.
 */
void ranger_app_handle_command(const ace_command_frame_t *frame);

#endif /* INC_RANGER_APP_H_ */

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

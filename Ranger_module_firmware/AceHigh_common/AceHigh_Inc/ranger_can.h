/*
 * ranger_can.h
 *
 *  Created on: Apr 27, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *  Provides:
 *  - Initialization, reception and sending of CAN frames
 *  - Ace protocol command decoding via ace_protocol
 *
 *  Does NOT handle:
 *  - Application logic (ranger_app)
 *  - Bootloader logic
*/

#ifndef INC_RANGER_CAN_H_
#define INC_RANGER_CAN_H_

#include <stdint.h>
#include "main.h"
#include "ace_protocol.h"

/* =========================
   Public API
   ========================= */

/**
 * @brief Retrieve latest received ACE command frame (non-blocking)
 *
 * Copies the latest received raw CAN frame from the internal RX mailbox,
 * decodes it into an Ace command frame, and returns it to the caller.
 *
 * Function is safe to call from the main loop. It uses a short
 * critical section to protect against concurrent updates from the
 * RX interrupt.
 *
 * @param[out] frame  Pointer to destination Ace command frame
 *
 * @return 1 if a new command frame was available and decoded
 * @return 0 if no new command frame is available
 *
 * @note
 * - Single-frame mailbox: if a new frame arrives before the previous
 *   one is consumed, the new frame is dropped.
 * - Intended for simple command/response flows.
 * 	 Use a ring buffer for high-throughput use cases,
 * 	 e.g. bootloader firmware upload.
 */
uint8_t ranger_can_receive_command(ace_command_frame_t *frame);

/**
 * @brief Initialize CAN interface
 *
 * Responsibilities:
 * - Start FDCAN peripheral
 * - Enable RX interrupts
 * - Prepare internal TX headers
 */
void ranger_can_init(void);

/**
 * @brief Send response frame
 *
 * Used for replying to READ/WRITE commands.
 *
 * @param command_id   Command being responded to
 * @param parameter_id Parameter being accessed
 * @param status_code  Result status (OK / ERROR / etc.)
 * @param payload      Pointer to payload data (optional)
 * @param payload_len  Length of payload (max 5 bytes)
 */
void ranger_can_send_response(uint8_t command_id,
                              uint8_t parameter_id,
                              uint8_t status_code,
                              const uint8_t *payload,
                              uint8_t payload_len);


/**
 * @brief Send heartbeat frame
 *
 * Periodic status broadcast of module state.
 *
 * @param system_state        Current system state (READY, FAULT, etc.)
 * @param module_temperature  Temperature (placeholder for now)
 * @param error_flags         Bitfield of active errors
 * @param uptime_s            Uptime in seconds
 */
void ranger_can_send_heartbeat(uint8_t system_state,
                               uint8_t module_temperature,
                               uint16_t error_flags,
                               uint32_t uptime_s);


/* =========================
   Node ID interface
   ========================= */

/**
 * @brief Get current node ID
 *
 * @return Current runtime node ID
 */
uint8_t ranger_can_get_node_id(void);


/**
 * @brief Apply node ID immediately
 *
 * NOTE:
 * - Prefer ranger_can_request_node_id_change() from command handlers
 * - Must be in range 1–127
 * - Stops CAN
 * - Reconfigures CAN RX filter
 * - Restarts CAN
 */
void ranger_can_set_node_id(uint8_t node_id);

/**
 * @brief Take in the requested node ID value
 *
 * NOTE:
 * - Must be in range 1–127
 * - Does not immediately reconfigure CAN filter
 * - Stores request so it can be applied later outside CAN RX interrupt
 * - Sets the node_id_change_pending variable
 * - Stores the new node ID temporarily
 *
 * @param node_id New node ID
 */
void ranger_can_request_node_id_change(uint8_t node_id);

/**
 * @brief Process the node ID change request
 *
 * NOTE:
 * - Checks the node ID request pending variable
 * - If change request is pending
 * - Calls the ranger_can_set_node_id function setting the new filter
 * - Clears the pending request if new filter was implemented
 *
 */
void ranger_can_process_pending_node_id_change(void);



#endif /* INC_RANGER_CAN_H_ */



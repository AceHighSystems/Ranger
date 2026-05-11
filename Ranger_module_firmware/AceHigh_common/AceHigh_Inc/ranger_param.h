/*
 * ranger_params.h
 *
 *  Created on: Apr 27, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_RANGER_PARAM_H_
#define INC_RANGER_PARAM_H_

#include "ace_protocol.h"

/* =========================
   System (0x10–0x1F)
   ========================= */
#define RANGER_PARAM_NODE_ID          0x10U
#define RANGER_PARAM_STATE            0x12U
#define RANGER_PARAM_UPTIME           0x13U
#define RANGER_PARAM_RESET			  0x14U

/* =========================
   Motion (0x40–0x5F)
   ========================= */
#define RANGER_PARAM_STEP_FREQUENCY   0x40U
#define RANGER_PARAM_STEP_DIRECTION   0x41U

/* =========================
   Encoder (0x60–0x7F)
   ========================= */
#define RANGER_PARAM_ENCODER_POSITION 0x60U

/* =========================
   Telemetry (0x80–0x9F)
   ========================= */
#define RANGER_PARAM_VOLTAGE          0x80U

/* =========================
   Diagnostics (0xC0–0xDF)
   ========================= */
#define RANGER_PARAM_LED_PA1          0xC0U



void ranger_param_init(void);
void ranger_param_handle_command(const ace_command_frame_t *frame);


#endif /* INC_RANGER_PARAM_H_ */



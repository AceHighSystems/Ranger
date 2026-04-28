/*
 * AceLight_protocol.h
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_ACE_PROTOCOL_H_
#define INC_ACE_PROTOCOL_H_

#include <stdint.h>

#define ACE_CANID_COMMAND_BASE      0x600U
#define ACE_CANID_RESPONSE_BASE     0x580U
#define ACE_CANID_HEARTBEAT_BASE    0x700U

#define ACE_PROTOCOL_VERSION        0x01U

#define ACE_CMD_READ                0x01U
#define ACE_CMD_WRITE               0x02U

#define ACE_CMD_BOOT_PING           0x40U
#define ACE_CMD_BOOT_START          0x41U
#define ACE_CMD_BOOT_DATA           0x42U
#define ACE_CMD_BOOT_END            0x43U
#define ACE_CMD_BOOT_RUN_APP        0x44U

/* Following STATUS codes used for response message frame */
#define ACE_STATUS_EXECUTING        0x00U
#define ACE_STATUS_QUEUED           0x01U
#define ACE_STATUS_DATA_FOLLOWS     0x02U
#define ACE_STATUS_UNKNOWN_COMMAND  0x10U
#define ACE_STATUS_INVALID_PARAM    0x11U
/* STATUS codes END */

/* Following STATE codes used for heartbeat message frame */
#define ACE_STATE_READY             0x01U
#define ACE_STATE_FAULT             0x02U
#define ACE_STATE_EXECUTING         0x03U
#define ACE_STATE_BOOTLOADER        0x04U
#define ACE_STATE_DISABLED          0x05U
/* STATE codes END */

/* Following structure is defined as a command frame */
typedef struct
{
  uint8_t command_id;
  uint8_t parameter_id;
  uint8_t payload[6];
} ace_command_frame_t;

void ace_decode_command(const uint8_t data[8], ace_command_frame_t *frame);

#endif

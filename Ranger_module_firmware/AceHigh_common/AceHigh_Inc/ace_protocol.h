/*
 *  ace_protocol.h
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_ACE_PROTOCOL_H_
#define INC_ACE_PROTOCOL_H_

#include <stdint.h>
#define ACE_NODE_ID_DEFAULT 0x02U

#define ACE_CANID_COMMAND_BASE      0x600U
#define ACE_CANID_RESPONSE_BASE     0x580U
#define ACE_CANID_HEARTBEAT_BASE    0x700U

#define ACE_PROTOCOL_VERSION        0x01U

/* Command IDs Ace protocol */
#define ACE_CMD_READ                0x01U
#define ACE_CMD_WRITE               0x02U

/* Following Parameter ID is specific for the bootloader of ace modules */
#define ACE_PARAM_BOOT				0x30U

/* Following Command ID are specific for the bootloader of ace modules */
#define ACE_CMD_BOOT_PING           0x40U
#define ACE_CMD_BOOT_START          0x41U
#define ACE_CMD_BOOT_DATA           0x42U
#define ACE_CMD_BOOT_END            0x43U
#define ACE_CMD_RESET               0x44U // resets the MCU jumps to the bootloader


/* Following STATUS codes are used for the module response message frames */
#define ACE_STATUS_OK        		0x10U  //   Last command was handled without errors
#define ACE_STATUS_QUEUED           0x11U  // 	if no specific USE-CASE please depricate
#define ACE_STATUS_DATA_FOLLOWS     0x12U
#define ACE_STATUS_UNKNOWN_COMMAND  0x13U
#define ACE_STATUS_UNKNOWN_PARAM    0x14U
/* STATUS codes END */

/* Following STATE codes used for the module heartbeat and bootloader response messages
 * Response is present in payload NOT Status field */
#define ACE_STATE_STANDBY           		  0x21U
#define ACE_STATE_FAULT             		  0x22U
#define ACE_STATE_EXECUTING         		  0x23U
#define ACE_STATE_APP_VALID					      0x24U
#define ACE_STATE_BOOTLOADER_ACTIVE     	0x25U
#define ACE_STATE_BOOTLOADER_INACTIVE   	0x26U
#define ACE_STATE_FIRMWARE_SIZE_FAULT   	0x27U
#define ACE_STATE_FIRMWARE_ERASE_FAULT  	0x28U
#define ACE_STATE_FIRMWARE_SEQUENCE_FAULT 0x29U
#define ACE_STATE_FIRMWARE_WRITE_FAULT  	0x2AU

/* STATE codes END */

/**
 * @brief Ace protocol command structure for received CAN frames / commands
 *
 * NOTE:
 * - Structure for holding specific data from the received standard CAN frame
 * - operates on top of the low level MCU CAN controller
 * - Includes the CAN node ID, Command, Parameter ID and payload data if applicable.
 * - Frame is decoded by the ace protocol layer
 * - May be updated to handle CAN-FD support
 *
 *  @param command_id : 	The specific command ID + CAN node ID
 *  @param parameter_id: 	The module parameter ID to operate on
 *  @param payload:			The data payload
 */

typedef struct
{
  uint8_t command_id;
  uint8_t parameter_id;
  uint8_t payload[6];
} ace_command_frame_t;

/**
 * @brief Ace protocol response structure for sending CAN frames / commands
 *
 * NOTE:
 * - Structure for ace protocol response frame
 * - Includes the echo of CAN node ID and Command.
 * - Status code encodes standard module status codes as defined in ace_protocol.h
 * - Frame is decoded by the ace protocol layer
 * - May be updated to handle CAN-FD support
 *
 *  @param command_id : 	Echo of the specific command ID + CAN node ID
 *  @param parameter_id: 	The module parameter ID that is being operated on
 *  @param payload:			Payload data if applicable
 */

typedef struct
{
  uint8_t command_id;
  uint8_t parameter_id;
  uint8_t status_code;
  uint8_t payload[5];
} ace_response_frame_t;

void ace_decode_command(const uint8_t data[8], ace_command_frame_t *frame);

#endif

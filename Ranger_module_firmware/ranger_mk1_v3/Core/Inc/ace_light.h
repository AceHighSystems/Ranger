/*
 * ace_light.h
 *
 *  Created on: Aug 8, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_ACE_LIGHT_H_
#define INC_ACE_LIGHT_H_

#include <stdint.h>
#include <stdbool.h>


/* ============================================================
 * AceLight reserved parameter IDs 0x00 to 0x0F
 * ============================================================ */

#define ACE_PARAM_SYNC                  0x01U
#define ACE_PARAM_NODE_ID               0x02U
#define ACE_PARAM_DEVICE_TYPE           0x03U
#define ACE_PARAM_SERIAL_NUMBER         0x04U
#define ACE_PARAM_FIRMWARE_VERSION      0x05U
#define ACE_PARAM_HARDWARE_VERSION      0x06U
#define ACE_PARAM_PROTOCOL_VERSION      0x07U


/* ============================================================
 * AceLight message types
 * ============================================================ */

typedef enum
{
    ACE_MSG_BROADCAST       = 0x01U,
    ACE_MSG_WRITE_REQUEST   = 0x02U,
    ACE_MSG_WRITE_RESPONSE  = 0x03U,
    ACE_MSG_READ_REQUEST    = 0x04U,
    ACE_MSG_READ_RESPONSE   = 0x05U

} ace_message_type_t;


/* ============================================================
 * AceLight response status codes
 * ============================================================ */

typedef enum
{
    ACE_STATUS_OK                = 0x01U,
    ACE_STATUS_DATA_FOLLOWS      = 0x02U,
    ACE_STATUS_INVALID_PARAMETER = 0x03U,
    ACE_STATUS_INVALID_VALUE     = 0x04U,
    ACE_STATUS_READ_ONLY         = 0x05U,
    ACE_STATUS_WRITE_ONLY        = 0x06U,
    ACE_STATUS_NODE_BUSY         = 0x07U

} ace_status_t;


/* ============================================================
 * AceLight parameter data types
 * ============================================================ */

typedef enum
{
    ACE_PARAM_U8,
    ACE_PARAM_U16,
    ACE_PARAM_U32,
    ACE_PARAM_I16,
    ACE_PARAM_I32

} ace_param_type_t;


/* ============================================================
 * AceLight parameter access permissions
 * ============================================================ */

typedef enum
{
    ACE_PARAM_RO,
    ACE_PARAM_WO,
    ACE_PARAM_RW

} ace_param_access_t;


/* ============================================================
 * AceLight parameter dictionary entry
 *
 * Describes one parameter exposed through the AceLight protocol.
 *
 * id:
 *     Unique parameter identifier.
 *
 * data:
 *     Pointer to the actual parameter data value.
 *
 * type:
 *     Data type of the parameter.
 *
 * access:
 *     Access permission of the parameter, such as read/write,
 *     read-only or write-only.
 * ============================================================ */

typedef struct
{
    uint16_t id;				// Parameter identifier
    void *data;                 // Pointer to the actual parameter data value
    ace_param_type_t type;      // Parameter data type
    ace_param_access_t access;  // Read and write, or read-only, write-only

} ace_parameter_t;


/* ============================================================
 * AceLight CAN transmit callback
 * ============================================================ */

/*
 * The hardware-specific application supplies this function.
 *
 * For Ranger this will point to ranger_can_transmit().
 */
typedef bool (*ace_tx_callback_t)(
    uint32_t identifier,
    const uint8_t *data,
    uint8_t length);


/* ============================================================
 * AceLight protocol instance
 *
 * Holds the configuration required by one AceLight instance.
 *
 * parameters:
 *     Pointer to the device parameter table.
 *
 * parameter_count:
 *     Number of entries in the parameter table.
 *
 * transmit:
 *     Hardware-specific callback used by AceLight to transmit
 *     CAN messages.
 * ============================================================ */

typedef struct
{
    const ace_parameter_t *parameters;
    uint16_t parameter_count;
    ace_tx_callback_t transmit;

} ace_light_t;


/* ============================================================
 * Public AceLight API
 *
 * ace_init:
 *     Initializes an AceLight instance with the device parameter
 *     table and hardware-specific transmit callback.
 *
 * ace_receive:
 *     Processes a received AceLight CAN message. The identifier,
 *     payload data and payload length are supplied by the
 *     hardware-specific CAN receive implementation.
 * ============================================================ */

void ace_init(
    ace_light_t *ace,
    const ace_parameter_t *parameters,
    uint16_t parameter_count,
    ace_tx_callback_t transmit);


void ace_receive(
    ace_light_t *ace,
    uint32_t identifier,
    const uint8_t *data,
    uint8_t length);


#endif /* INC_ACE_LIGHT_H_ */

/*
 * ace_light.c
 *
 * Generic AceLight protocol implementation.
 *
 * This file is hardware and device independent.
 *
 * It must NOT contain references to:
 *
 *   - Ranger
 *   - STM32 HAL
 *   - FDCAN
 *   - g_param
 *   - ranger_param_table
 *
 * Device-specific parameter storage and the parameter dictionary are
 * supplied to AceLight through ace_init().
 */

#include "ace_light.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/* ============================================================
 * AceLight identifier layout
 * ============================================================
 *
 * Extended 29-bit CAN identifier:
 *
 *  28       26  25  24  18  17  16       10  9  8             0
 * +-----------+---+-----------+---+-----------+---+-------------+
 * | Msg Type  | R | Dest Node | R | Src Node  | R | Parameter ID|
 * +-----------+---+-----------+---+-----------+---+-------------+
 *
 * Message Type : 3 bits
 * Destination  : 7 bits
 * Source       : 7 bits
 * Parameter ID : 9 bits
 *
 * Bits 25, 17 and 9 are reserved and must remain zero.
 */

#define ACE_MESSAGE_TYPE_SHIFT       26U
#define ACE_DESTINATION_SHIFT        18U
#define ACE_SOURCE_SHIFT             10U
#define ACE_PARAMETER_SHIFT           0U

#define ACE_MESSAGE_TYPE_MASK        0x07U
#define ACE_NODE_ID_MASK             0x7FU
#define ACE_PARAMETER_ID_MASK        0x1FFU

#define ACE_RESERVED_BIT_25          (1UL << 25U)
#define ACE_RESERVED_BIT_17          (1UL << 17U)
#define ACE_RESERVED_BIT_9           (1UL << 9U)

#define ACE_RESERVED_BITS_MASK       \
    (ACE_RESERVED_BIT_25 |           \
     ACE_RESERVED_BIT_17 |           \
     ACE_RESERVED_BIT_9)

#define ACE_CAN_IDENTIFIER_MASK      0x1FFFFFFFUL


/* ============================================================
 * Internal decoded identifier representation
 * ============================================================ */

typedef struct
{
    ace_message_type_t message_type;
    uint8_t destination;
    uint8_t source;
    uint16_t parameter_id;

} ace_identifier_t;


/* ============================================================
 * Local function prototypes
 * ============================================================ */

static bool ace_decode_identifier(uint32_t identifier, ace_identifier_t *decoded);


static uint32_t ace_build_identifier(
	ace_message_type_t message_type,
    uint8_t destination,
    uint8_t source,
    uint16_t parameter_id);


static const ace_parameter_t *ace_param_find(
    const ace_light_t *ace,
    uint16_t id);


static uint8_t ace_param_size(
    ace_param_type_t type);


static ace_status_t ace_param_read(
    const ace_light_t *ace,
    uint16_t id,
    uint8_t *data,
    uint8_t *length);


static ace_status_t ace_param_write(
    const ace_light_t *ace,
    uint16_t id,
    const uint8_t *data,
    uint8_t length);


static void ace_handle_read_request(
    ace_light_t *ace,
    const ace_identifier_t *request);


static void ace_handle_write_request(
    ace_light_t *ace,
    const ace_identifier_t *request,
    const uint8_t *data,
    uint8_t length);


static void ace_handle_broadcast(
    ace_light_t *ace,
    const ace_identifier_t *message,
    const uint8_t *data,
    uint8_t length);


static void ace_send_response(
    ace_light_t *ace,
    ace_message_type_t message_type,
    const ace_identifier_t *request,
    ace_status_t status,
    const uint8_t *data,
    uint8_t length);


/* ============================================================
 * Public functions
 * ============================================================ */

/**
 * @brief Initialize an AceLight protocol instance.
 *
 * This function connects the generic AceLight protocol engine to a
 * specific device.
 *
 * The device supplies:
 *
 *   - its parameter dictionary
 *   - the number of parameters in that dictionary
 *   - a hardware-specific CAN transmit callback
 *
 * No hardware initialization is performed here.
 *
 * @param ace             Pointer to AceLight instance.
 * @param parameters      Device parameter dictionary.
 * @param parameter_count Number of entries in the dictionary.
 * @param transmit        CAN transmit callback.
 */
void ace_init(
    ace_light_t *ace,
    const ace_parameter_t *parameters,
	uint16_t parameter_count,
    ace_tx_callback_t transmit)
{
    /*
     * Protect against an invalid AceLight instance pointer.
     */
    if (ace == NULL)
    {
        return;
    }
    /* Pass in the references to the ace instance ie. populate the members. */
    ace->parameters = parameters;
    ace->parameter_count = parameter_count;
    ace->transmit = transmit;
}
/**
 * @brief Process a received CAN frame as an AceLight message.
 *
 * This is the primary input function for the AceLight protocol engine.
 *
 * The hardware-specific CAN driver should forward accepted CAN frames
 * directly to this function.
 *
 * The function:
 *
 *   1. Decodes the 29-bit AceLight identifier
 *   2. Validates the identifier
 *   3. Checks whether the frame was intended for this node
 *   4. Determines the message type
 *   5. Processes READ, WRITE or BROADCAST messages
 *   6. Generates responses when required
 *
 * @param ace        Pointer to AceLight instance.
 * @param identifier 29-bit extended CAN identifier.
 * @param data       CAN payload.
 * @param length     Payload length in bytes.
 */
void ace_receive(
    ace_light_t *ace,
    uint32_t identifier,
    const uint8_t *data,
    uint8_t length)
{
    ace_identifier_t identifier_decoded;

    /*
     * AceLight cannot operate without a valid instance.
     */
    if (ace == NULL)
    {
        return;
    }

    /*
     * Current AceLight implementation uses classical CAN payload sizes.
     */
    if (length > 8U)
    {
        return;
    }


    /*
     * Decode and validate the AceLight identifier.
     *
     * Invalid identifiers are silently ignored.
     */
    if (!ace_decode_identifier(identifier, &identifier_decoded))
    {
        return;
    }


    /*
     * Normal messages must be addressed to this node.
     *
     * Broadcast messages are accepted independently of the destination
     * field.
     *
     * CAN hardware filtering may already have performed this check,
     * but AceLight repeats it so protocol correctness does not depend
     * on hardware configuration.
     */
    if (identifier_decoded.message_type != ACE_MSG_BROADCAST)
    {

    	/* Get the node ID value from the parameter table for this node. */
    	const ace_parameter_t *node_id_entry;

    	node_id_entry = ace_param_find(ace, ACE_PARAM_NODE_ID);

    	if ((node_id_entry == NULL) || (node_id_entry->data == NULL))
    	        {
    	            return;
    	        }

        uint8_t node_id = (uint8_t)(*((const uint8_t *)node_id_entry->data));


        if (identifier_decoded.destination != node_id)
        {
            return;
        }
    }

    /*
     * Route the received message according to message type.
     */
    switch (identifier_decoded.message_type)
    {
        case ACE_MSG_WRITE_REQUEST:

            ace_handle_write_request(
                ace,
                &identifier_decoded,
                data,
                length);

            break;


        case ACE_MSG_READ_REQUEST:

            /*
             * A READ_REQUEST contains no parameter data.
             */
            if (length != 0U)
            {
                return;
            }

            ace_handle_read_request(
                ace,
                &identifier_decoded);

            break;


        case ACE_MSG_BROADCAST:

            ace_handle_broadcast(
                ace,
                &identifier_decoded,
                data,
                length);

            break;


        /*
         * Response messages are currently ignored by this device-side
         * implementation.
         *
         * Support for AceLight nodes which actively initiate requests
         * can be added later.
         */
        case ACE_MSG_WRITE_RESPONSE:
        case ACE_MSG_READ_RESPONSE:
        default:

            break;
    }
}


/* ============================================================
 * Identifier handling
 * ============================================================ */

/**
 * @brief Decode and validate an incomming received AceLight CAN identifier.
 *
 * @param identifier Raw extended CAN identifier.
 * @param decoded    Destination structure for decoded fields.
 *
 * @return true if the identifier is valid.
 */
static bool ace_decode_identifier(
    uint32_t identifier,
    ace_identifier_t *decoded)
{
    uint32_t message_type;

    if (decoded == NULL)
    {
        return false;
    }

    /*
     * AceLight uses only the 29-bit extended CAN identifier range.
     */
    if ((identifier & ~ACE_CAN_IDENTIFIER_MASK) != 0U)
    {
        return false;
    }

    /*
     * Reserved identifier bits must be zero.
     */
    if ((identifier & ACE_RESERVED_BITS_MASK) != 0U)
    {
        return false;
    }

    /*
     * Extract the message type.
     */
    message_type =
        (identifier >> ACE_MESSAGE_TYPE_SHIFT) &
        ACE_MESSAGE_TYPE_MASK;

    /*
     * Reject undefined message types.
     */
    if ((message_type < (uint32_t)ACE_MSG_BROADCAST) ||
        (message_type > (uint32_t)ACE_MSG_READ_RESPONSE))
    {
        return false;
    }

    /*
     * Extract all AceLight identifier fields.
     */
    decoded->message_type =
        (ace_message_type_t)message_type;

    decoded->destination =
        (uint8_t)((identifier >> ACE_DESTINATION_SHIFT) &
                  ACE_NODE_ID_MASK);

    decoded->source =
        (uint8_t)((identifier >> ACE_SOURCE_SHIFT) &
                  ACE_NODE_ID_MASK);

    decoded->parameter_id =
        (uint16_t)((identifier >> ACE_PARAMETER_SHIFT) &
                   ACE_PARAMETER_ID_MASK);

    return true;
}


/**
 * @brief Build an AceLight 29-bit extended CAN identifier.
 */
static uint32_t ace_build_identifier(
    ace_message_type_t message_type,
    uint8_t destination,
    uint8_t source,
    uint16_t parameter_id)
{
    uint32_t identifier = 0U;

    /*
     * Mask every value before inserting it into the identifier.
     *
     * This prevents one field from accidentally affecting adjacent
     * identifier bits.
     */
    identifier |=
        (((uint32_t)message_type & ACE_MESSAGE_TYPE_MASK)
         << ACE_MESSAGE_TYPE_SHIFT);

    identifier |=
        (((uint32_t)destination & ACE_NODE_ID_MASK)
         << ACE_DESTINATION_SHIFT);

    identifier |=
        (((uint32_t)source & ACE_NODE_ID_MASK)
         << ACE_SOURCE_SHIFT);

    identifier |=
        (((uint32_t)parameter_id & ACE_PARAMETER_ID_MASK)
         << ACE_PARAMETER_SHIFT);

    return identifier;
}


/* ============================================================
 * Parameter handling
 * ============================================================ */

/**
 * @brief Find a parameter in the supplied device parameter dictionary.
 *
 * AceLight does not know which device owns the parameter table.
 *
 * The same function can therefore operate on a Ranger parameter table,
 * another AceHigh device, or a completely independent implementation.
 */
static const ace_parameter_t *ace_param_find(
    const ace_light_t *ace,
    uint16_t param_id)
{
    uint16_t i;

    if ((ace == NULL) || (ace->parameters == NULL))
    {
        return NULL;
    }

    /*
     * Search the parameter dictionary for the requested ID.
     */
    for (i = 0U; i < ace->parameter_count; i++)
    {
        if (ace->parameters[i].id == param_id)
        {
            return &ace->parameters[i];
        }
    }

    /*
     * Parameter is not implemented by this device.
     */
    return NULL;
}



/**
 * @brief Return the number of bytes occupied by a parameter type.
 */
static uint8_t ace_param_size(
    ace_param_type_t type)
{
    switch (type)
    {
        case ACE_PARAM_U8:
            return 1U;

        case ACE_PARAM_U16:
        case ACE_PARAM_I16:
            return 2U;

        case ACE_PARAM_U32:
        case ACE_PARAM_I32:
            return 4U;

        default:
            return 0U;
    }
}


/**
 * @brief Read a parameter from the device parameter dictionary.
 *
 * The parameter value is serialized into the output buffer in
 * little-endian byte order.
 */
static ace_status_t ace_param_read(
    const ace_light_t *ace,
    uint16_t id,
    uint8_t *data,
    uint8_t *length)
{
    const ace_parameter_t *entry;
    uint32_t value;
    uint8_t size;
    uint8_t i;

    if ((data == NULL) || (length == NULL))
    {
        return ACE_STATUS_INVALID_VALUE;
    }

    /*
     * Search for the requested parameter.
     */
    entry = ace_param_find(ace, id);

    if (entry == NULL)
    {
        return ACE_STATUS_INVALID_PARAMETER;
    }

    /*
     * A write-only parameter cannot be read.
     */
    if (entry->access == ACE_PARAM_WO)
    {
        return ACE_STATUS_WRITE_ONLY;
    }

    /*
     * Determine the number of bytes represented by the parameter.
     */
    size = ace_param_size(entry->type);

    if (size == 0U)
    {
        return ACE_STATUS_INVALID_VALUE;
    }

    /*
     * Read the actual RAM variable according to its declared type.
     *
     * The value is temporarily normalized into a uint32_t container
     * before being serialized into the CAN payload.
     */
    switch (entry->type)
    {
        case ACE_PARAM_U8:

            value = *((uint8_t *)entry->data);
            break;


        case ACE_PARAM_U16:

            value = *((uint16_t *)entry->data);
            break;


        case ACE_PARAM_U32:

            value = *((uint32_t *)entry->data);
            break;


        case ACE_PARAM_I16:

            value =
                (uint32_t)(int32_t)(*((int16_t *)entry->data));

            break;


        case ACE_PARAM_I32:

            value =
                (uint32_t)(*((int32_t *)entry->data));

            break;


        default:

            return ACE_STATUS_INVALID_VALUE;
    }

    /*
     * Serialize the value little-endian.
     *
     * Example:
     *
     * value = 0x12345678
     *
     * data[0] = 0x78
     * data[1] = 0x56
     * data[2] = 0x34
     * data[3] = 0x12
     */
    for (i = 0U; i < size; i++)
    {
        data[i] = (uint8_t)((value >> (8U * i)) & 0xFFU);
    }

    *length = size;

    return ACE_STATUS_OK;
}


/**
 * @brief Write a parameter in the device parameter dictionary.
 *
 * Input bytes are interpreted in little-endian byte order.
 */
static ace_status_t ace_param_write(
    const ace_light_t *ace,
    uint16_t param_id,
    const uint8_t *data,
    uint8_t length)
{
    const ace_parameter_t *entry;

    uint32_t value = 0U;

    uint8_t expected_length;
    uint8_t i;

    /*
     * Search for the requested parameter.
     */
    entry = ace_param_find(ace, param_id);

    if (entry == NULL)
    {
        return ACE_STATUS_INVALID_PARAMETER;
    }

    /*
     * A read-only parameter cannot be written.
     */
    if (entry->access == ACE_PARAM_RO)
    {
        return ACE_STATUS_READ_ONLY;
    }

    /* Determine how many bytes this parameter requires. */
    expected_length = ace_param_size(entry->type);

    if (expected_length == 0U)
    {
        return ACE_STATUS_INVALID_VALUE;
    }

    /* The write payload must exactly match the declared parameter size. */
    if (length != expected_length)
    {
        return ACE_STATUS_INVALID_VALUE;
    }

    /* A write requires a valid data buffer. */
    if (data == NULL)
    {
        return ACE_STATUS_INVALID_VALUE;
    }

    /* Reconstruct the parameter value from little-endian bytes. */
    for (i = 0U; i < length; i++)
    {
        value |= ((uint32_t)data[i] << (8U * i));
    }

    /* Write the reconstructed value into the actual device variable. */
    switch (entry->type)
    {
        case ACE_PARAM_U8:

            *((uint8_t *)entry->data) = (uint8_t)value;

            break;

        case ACE_PARAM_U16:

            *((uint16_t *)entry->data) = (uint16_t)value;

            break;

        case ACE_PARAM_U32:

            *((uint32_t *)entry->data) = value;

            break;

        case ACE_PARAM_I16:

            *((int16_t *)entry->data) = (int16_t)(uint16_t)value;

            break;

        case ACE_PARAM_I32:

            *((int32_t *)entry->data) = (int32_t)value;

            break;

        default:

            return ACE_STATUS_INVALID_VALUE;
    }

    return ACE_STATUS_OK;
}


/* ============================================================
 * Message handling
 * ============================================================ */

/**
 * @brief Process an AceLight READ_REQUEST.
 */
static void ace_handle_read_request(
    ace_light_t *ace,
    const ace_identifier_t *identifier)
{
    uint8_t parameter_data[4];
    uint8_t parameter_length = 0U;

    ace_status_t status;

    /*
     * Attempt to read the requested parameter.
     */
    status = ace_param_read(
        ace,
        identifier->parameter_id,
        parameter_data,
        &parameter_length);

    /*
     * A successful read is represented on the CAN bus using
     * ACE_STATUS_DATA_FOLLOWS.
     *
     * Response payload:
     *
     * Byte 0    = ACE_STATUS_DATA_FOLLOWS
     * Byte 1... = parameter data
     */
    if (status == ACE_STATUS_OK)
    {
        ace_send_response(
            ace,
            ACE_MSG_READ_RESPONSE,
            identifier,
            ACE_STATUS_DATA_FOLLOWS,
            parameter_data,
            parameter_length);
    }
    else
    {
        /*
         * Failed reads contain only the resulting status code.
         */
        ace_send_response(
            ace,
            ACE_MSG_READ_RESPONSE,
            identifier,
            status,
            NULL,
            0U);
    }
}


/**
 * @brief Process an AceLight WRITE_REQUEST.
 */
static void ace_handle_write_request(
    ace_light_t *ace,
    const ace_identifier_t *identifier,
    const uint8_t *data,
    uint8_t length)
{
    ace_status_t status;

    /*
     * Perform the generic parameter write.
     *
     * ace_param_write() handles:
     *
     *   - parameter lookup
     *   - access permission
     *   - payload size
     *   - typed RAM write
     */
    status = ace_param_write(
        ace,
        identifier->parameter_id,
        data,
        length);

    /*
     * Send the resulting status back to the requesting node.
     */
    ace_send_response(
        ace,
        ACE_MSG_WRITE_RESPONSE,
        identifier,
        status,
        NULL,
        0U);
}


/**
 * @brief Process an AceLight BROADCAST message.
 *
 * Current implementation treats a broadcast as a parameter write
 * which does not generate a response.
 *
 * This allows parameters such as ACE_PARAM_SYNC to be distributed
 * to all nodes without every receiving device replying.
 */
static void ace_handle_broadcast(
    ace_light_t *ace,
    const ace_identifier_t *message,
    const uint8_t *data,
    uint8_t length)
{
    /*
     * Use the normal generic parameter write mechanism.
     *
     * The returned status is intentionally ignored because broadcast
     * messages do not produce responses.
     */
    (void)ace_param_write(
        ace,
        message->parameter_id,
        data,
        length);
}


/* ============================================================
 * Response handling
 * ============================================================ */

/**
 * @brief Build and transmit an AceLight response.
 *
 * Response addressing reverses the source and destination of
 * the original request.
 *
 * Request:
 *
 *     Source      = requesting node
 *     Destination = this node
 *     The rx_identifier is the identifier that was
 *
 * Response:
 *
 *     Source      = this node
 *     Destination = requesting node
 *
 * Response payload:
 *
 *     Byte 0      = ACE_STATUS
 *     Byte 1..7   = optional parameter data
 */
static void ace_send_response(
    ace_light_t *ace,
    ace_message_type_t message_type,
    const ace_identifier_t *rx_identifier,
    ace_status_t status,
    const uint8_t *data,
    uint8_t length)
{

	uint32_t tx_identifier; // identifier for then new transmit (send) response
    uint8_t tx_data[8];
    uint8_t i;
    const ace_parameter_t *node_id_entry; // pointer to node_id pointer

    /* One payload byte is always occupied by the AceLight status.
     * Therefore at most seven bytes remain for response data. */
    if (length > 7U)
    {
        return;
    }

    /* Verify that the required objects and transmit callback exist. */
    if ((ace == NULL) ||
        (rx_identifier == NULL) ||
        (ace->transmit == NULL))
    {
        return;
    }

    /*
     * Retrieve this device's Node ID from the parameter dictionary.
     *
     * ACE_PARAM_NODE_ID is an AceLight standard parameter and the
     * parameter dictionary is the single source of truth for the
     * local node identity.
     */

    node_id_entry = ace_param_find(ace, ACE_PARAM_NODE_ID);

    if ((node_id_entry == NULL) ||
        (node_id_entry->data == NULL))
    {
        return;
    }

    uint8_t node_id = *(const uint8_t *)node_id_entry->data; // Go to that address and retrieve the actual uint8_t stored there.


    /* Byte 0 of every READ_RESPONSE and WRITE_RESPONSE contains the AceLight status code. */
    tx_data[0] = (uint8_t)status;

    /* Append optional parameter data after the status byte. */
    if ((data != NULL) && (length > 0U))
    {
        for (i = 0U; i < length; i++)
        {
            tx_data[i + 1U] = data[i];
        }
    }

    /*
     * Build the response identifier.
     *
     * Destination:
     *     Source node of the original request.
     *
     * Source:
     *     This AceLight node.
     *
     * Parameter ID:
     *     Same parameter as the original request.
     */
    tx_identifier = ace_build_identifier(
        message_type,
        rx_identifier->source,  // the new destination is the source in the previously received identifier
        node_id,
        rx_identifier->parameter_id);

    /*
     * Pass the completed frame to the hardware-specific transmit
     * callback.
     *
     * AceLight itself has no knowledge of the underlying CAN driver.
     */
    (void)ace->transmit(
        tx_identifier,
        tx_data,
        (uint8_t)(length + 1U));
}

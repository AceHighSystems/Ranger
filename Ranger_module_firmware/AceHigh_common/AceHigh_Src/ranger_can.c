/*
 * ranger_can.c
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *  Description:
 *  CAN transport layer for Ranger module.
 *
 *  Owns:
 *  - FDCAN start/initialization
 *  - Transmission of response frames
 *  - Transmission of heartbeat frames
 *  - Reception of CAN frames
 *  - ACE command decoding via ace_protocol.c
 *
 *  Does NOT own:
 *  - Application behavior
 *  - Bootloader behavior
 */
#include "ranger_can.h"
#include "ace_protocol.h"
#include "main.h"
#include <string.h>

/* =========================
   External references
   ========================= */

/* FDCAN handle defined in main.c */
extern FDCAN_HandleTypeDef hfdcan1;

/* =========================
   Private variables
   ========================= */

/* Pre-configured TX header for heartbeat messages */
static FDCAN_TxHeaderTypeDef heartbeat_header;

/* Default node ID, used from boot */
static uint8_t ranger_node_id = ACE_NODE_ID_DEFAULT;

/* Variable for holding the requested node ID to change to */
static uint8_t pending_node_id = 0U;

/* Flag for CAN Rx frame received but frame processing and decoding pending */
static volatile uint8_t rx_frame_pending = 0U;

/* Buffer for received CAN Rx frame */
static uint8_t rx_data_buffer[8];

/* flag for node id change request pending */
static uint8_t node_id_change_pending = 0U;



/* =========================
   Private function prototypes
   ========================= */

static void ranger_can_config_filter(void);
static uint32_t ranger_can_get_command_id(void);
static uint32_t ranger_can_get_response_id(void);
static uint32_t ranger_can_get_heartbeat_id(void);

/* =========================
   Public functions
   ========================= */

/**
 * @brief Private helper functions
 *
 * Used for enabling writable node IDs.
 * Updates the response, command and heartbeat message headers
 * with the changed node ID
 */
static uint32_t ranger_can_get_command_id(void)
{
  return ACE_CANID_COMMAND_BASE + ranger_node_id;
}

static uint32_t ranger_can_get_response_id(void)
{
  return ACE_CANID_RESPONSE_BASE + ranger_node_id;
}

static uint32_t ranger_can_get_heartbeat_id(void)
{
  return ACE_CANID_HEARTBEAT_BASE + ranger_node_id;
}

void ranger_reset(void)
{
  NVIC_SystemReset();
}

/**
 * @brief Initialize CAN interface
 *
 * Starts FDCAN peripheral and enables RX interrupt.
 * Also prepares a static TX header for heartbeat messages.
 */
void ranger_can_init(void)
{
  ranger_can_config_filter();

  heartbeat_header.Identifier          = ranger_can_get_heartbeat_id();
  heartbeat_header.IdType              = FDCAN_STANDARD_ID;
  heartbeat_header.TxFrameType         = FDCAN_DATA_FRAME;
  heartbeat_header.DataLength          = FDCAN_DLC_BYTES_8;
  heartbeat_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  heartbeat_header.BitRateSwitch       = FDCAN_BRS_OFF;
  heartbeat_header.FDFormat            = FDCAN_CLASSIC_CAN;
  heartbeat_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  heartbeat_header.MessageMarker       = 0;

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ActivateNotification(&hfdcan1,
                                     FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                     0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief Send response frame (READ / WRITE reply)
 *
 * Frame format:
 * Byte 0: command_id (echo)
 * Byte 1: parameter_id (echo)
 * Byte 2: status_code
 * Byte 3-7: payload
 */
void ranger_can_send_response(uint8_t command_id,
                              uint8_t parameter_id,
                              uint8_t status_code,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
  FDCAN_TxHeaderTypeDef response_header;
  uint8_t data[8] = {0};

  /* Configure TX header for response */
  response_header.Identifier          = ranger_can_get_response_id();
  response_header.IdType              = FDCAN_STANDARD_ID;
  response_header.TxFrameType         = FDCAN_DATA_FRAME;
  response_header.DataLength          = FDCAN_DLC_BYTES_8;
  response_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  response_header.BitRateSwitch       = FDCAN_BRS_OFF;
  response_header.FDFormat            = FDCAN_CLASSIC_CAN;
  response_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  response_header.MessageMarker       = 0;

  /* Build payload */
  data[0] = command_id;
  data[1] = parameter_id;
  data[2] = status_code;

  /* Copy optional payload (max 5 bytes) should the payload be longer only the 5 first bytes are copied*/
  if ((payload != NULL) && (payload_len > 0U))
  {
    if (payload_len > 5U)
    {
      payload_len = 5U;
    }

    memcpy(&data[3], payload, payload_len);
  }

  /* Transmit message */
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &response_header, data) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief Send periodic heartbeat message
 *
 * Frame format:
 * Byte 0: protocol_version
 * Byte 1: system_state
 * Byte 2: module_temperature
 * Byte 3-4: error_flags
 * Byte 5-7: uptime (24-bit)
 */
void ranger_can_send_heartbeat(uint8_t system_state,
                               uint8_t module_temperature,
                               uint16_t error_flags,
                               uint32_t uptime_s)
{
  uint8_t data[8] = {0};

  data[0] = ACE_PROTOCOL_VERSION;
  data[1] = system_state;
  data[2] = module_temperature;
  data[3] = (uint8_t)(error_flags & 0xFFU);
  data[4] = (uint8_t)((error_flags >> 8) & 0xFFU);
  data[5] = (uint8_t)(uptime_s & 0xFFU);
  data[6] = (uint8_t)((uptime_s >> 8) & 0xFFU);
  data[7] = (uint8_t)((uptime_s >> 16) & 0xFFU);

  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &heartbeat_header, data) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief Retrieve latest received Ace command frame (non-blocking)
 *
 * Copies the latest received raw CAN frame from the internal RX mailbox,
 * decodes it into an Ace command frame, and returns it to the caller.
 *
 * @note
 * - Single-frame mailbox: if a new frame arrives before the previous
 *   one is consumed, the new frame is dropped.
 *   Todo: set up a ringbuffer
 */
uint8_t ranger_can_receive_command(ace_command_frame_t *frame)
{
  uint8_t data[8];

  if (rx_frame_pending == 0U)
  {
    return 0U;
  }

  __disable_irq();

  for (uint8_t i = 0U; i < 8U; i++)
  {
    data[i] = rx_data_buffer[i];
  }

  rx_frame_pending = 0U;

  __enable_irq();

  ace_decode_command(data, frame);

  return 1U;
}

/**
 * @brief Get the CAN node ID
 *
 */
uint8_t ranger_can_get_node_id(void)
{
  return ranger_node_id;
}

/**
 * @brief Set request change flag when a Node ID change is requested
 * store the requested node ID in a variable
 */
void ranger_can_request_node_id_change(uint8_t node_id)
{
  if ((node_id == 0U) || (node_id > 127U))
  {
    return;
  }

  pending_node_id = node_id;
  node_id_change_pending = 1U;
}

/**
 * @brief Process the node ID change request
 */
void ranger_can_process_pending_node_id_change(void)
{
  if (node_id_change_pending == 0U)
  {
    return;
  }

  node_id_change_pending = 0U;
  ranger_can_set_node_id(pending_node_id);
  pending_node_id = 0U;
}

/**
 * @brief Set the CAN node ID during runtime
 *  reconfigures the node ID
 *  disables CAN interrupts temporarily and reconfigures filters with new node ID
 *  restarts CAN with new filter
 *  todo save node ID to persistent memory MRAM
 */
void ranger_can_set_node_id(uint8_t node_id)
{
  /* Validate range (1–127) */
  if ((node_id == 0U) || (node_id > 127U))
  {
    return;
  }

  /* No change → do nothing */
  if (node_id == ranger_node_id)
  {
    return;
  }

  __disable_irq();

  /* Stop CAN peripheral */
  if (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK)
  {
    __enable_irq();
    Error_Handler();
  }

  /* Update node ID */
  ranger_node_id = node_id;
  heartbeat_header.Identifier = ranger_can_get_heartbeat_id();

  /* Reconfigure RX filter for new node ID */
  ranger_can_config_filter();

  /* Restart CAN */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    __enable_irq();
    Error_Handler();
  }

  /* Re-enable RX interrupt */
  if (HAL_FDCAN_ActivateNotification(&hfdcan1,
                                     FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                     0) != HAL_OK)
  {
    __enable_irq();
    Error_Handler();
  }

  __enable_irq();
}

/**
 * @brief Configure CAN filter, used when setting node ID during runtime
 */
static void ranger_can_config_filter(void)
{
  FDCAN_FilterTypeDef filter = {0};

  filter.IdType       = FDCAN_STANDARD_ID;
  filter.FilterIndex  = 0;
  filter.FilterType   = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

  filter.FilterID1 = ranger_can_get_command_id();
  filter.FilterID2 = 0x7FFU; /* exact match */

  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  /* Reject everything else */
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                   FDCAN_REJECT,
                                   FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    Error_Handler();
  }
}

/* =========================
   Interrupt callback
   ========================= */

/**
 * @brief FDCAN RX callback (FIFO0)
 *
 * Called by HAL when a new CAN frame is received.
 *
 * Responsibilities:
 * - Read CAN frame from RX FIFO
 * - Filter frames addressed to this node
 * - Store latest frame in internal RX mailbox
 *
 * Notes:
 * - Does NOT perform protocol decoding
 * - Does NOT call application or bootloader logic
 * - Frame is later retrieved via ranger_can_receive()
 *
 * Limitation:
 * - Single-frame mailbox: if a new frame arrives before the previous
 *   one is consumed, it will be dropped
 *   Todo - implement ringbuffer
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];

  if (hfdcan != &hfdcan1)
  {
    return;
  }

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U)
  {
    return;
  }

  if (HAL_FDCAN_GetRxMessage(hfdcan,
                             FDCAN_RX_FIFO0,
                             &rx_header,
                             rx_data) != HAL_OK)
  {
    return;
  }
  /*
   * Filter the message on CAN node ID: if not correct ID,
   * ignore frame.
   */
  if (rx_header.Identifier != ranger_can_get_command_id())
  {
    return;
  }

  if (rx_frame_pending != 0U)
  {
    /*
     * Minimal version:
     * Drop frame if previous one has not been consumed yet.
     * Later you can replace this with a ring buffer.
     */
    return;
  }

  for (uint8_t i = 0; i < 8U; i++)
  {
	  rx_data_buffer[i] = rx_data[i];
  }

  rx_frame_pending = 1U;
}

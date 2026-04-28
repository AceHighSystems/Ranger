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
 *  - Reception of CAN frames (RX callback)
 *
 *  Does NOT own:
 *  - Protocol decoding (ace_protocol.c)
 *  - Application behavior (ranger_app.c)
 */
#include "ranger_app.h"
#include "ranger_can.h"
#include "ranger_param.h"
#include "ace_protocol.h"


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

/* default node ID, used from boot */
static uint8_t ranger_node_id = RANGER_NODE_ID_DEFAULT;

/* variable for holding the requested node ID to change to */
static uint8_t pending_node_id = 0U;

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

  /* Copy optional payload (max 5 bytes) */
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
 * - Read CAN frame
 * - Filter on command ID
 * - Decode protocol frame
 * - Forward to application layer
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];

  /* Decoded protocol frame */
  ace_command_frame_t command_frame;

  /* Ensure this callback belongs to our FDCAN instance */
  if (hfdcan != &hfdcan1)
  {
    return;
  }

  /* Check if new message is available */
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U)
  {
    /* Read message from FIFO */
    if (HAL_FDCAN_GetRxMessage(hfdcan,
                               FDCAN_RX_FIFO0,
                               &rx_header,
                               rx_data) == HAL_OK)
    {
      /* Only process command frames addressed to this node */
      if (rx_header.Identifier == ranger_can_get_command_id())
      {
        /* Decode Ace protocol frame */
        ace_decode_command(rx_data, &command_frame);

        /* Forward to application layer */
        ranger_app_handle_command(&command_frame);
      }
    }
  }
}

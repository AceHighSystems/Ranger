/*
 * ranger_bootloader.c
 *
 *  Created on: Apr 30, 2026
 *      Author: torgj
 *
 *  Description:
 *  - Ranger bootloader
 *
 *  Owns:
 *  - Bootloader opperations
 */

#include "ranger_bootloader.h"
#include "ranger_can.h"
#include "ace_protocol.h"
#include "main.h"


typedef void (*app_entry_t)(void); // function pointer

/* =========================
   Private variables
   ========================= */

static uint32_t boot_start_ms = 0U;
static uint32_t boot_blink_last_ms = 0U;
static uint32_t boot_expected_size = 0;
static uint32_t boot_received_size = 0;
static uint32_t boot_write_address = RANGER_APP_START_ADDR;
static uint16_t boot_expected_sequence = 0;
static uint8_t  boot_session_active = 0;

/* =========================
   Private function prototypes
   ========================= */

static void bootloader_handle_frame(ace_command_frame_t *frame);
static void bootloader_handle_ping(ace_command_frame_t *frame);
static void bootloader_handle_start(ace_command_frame_t *frame);
static void jump_to_app(void);
static uint8_t app_is_valid(void);
static void bootloader_handle_ping(ace_command_frame_t *frame);


/**
 * @brief Initialize the bootloader dependencies
 * 		Sets up the can interface
 * 		Handles bootloader timeout functionality
 */
void ranger_bootloader_init(void)
{
  boot_start_ms = HAL_GetTick();
  boot_blink_last_ms = boot_start_ms;

  ranger_can_init();
}

/**
 * @brief Bootloader task function, called from main application
		Checks the underlying CAN interface for traffic
		Checks that application is valid
		Jumps to application space after a timeout period if no traffic registered
 */
void ranger_bootloader_task(void)
{
  uint32_t now = HAL_GetTick();
  ace_command_frame_t can_frame;

  if ((now - boot_blink_last_ms) >= BOOT_BLINK_INTERVAL_MS)
  {
    boot_blink_last_ms = now;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
  }

  if (ranger_can_receive_command(&can_frame))
  {
    bootloader_handle_frame(&can_frame);
  }

  if ((now - boot_start_ms) > RANGER_BOOT_TIMEOUT_MS)
  {
    if (app_is_valid())
    {
    	jump_to_app();
    }
  }
}


static uint8_t app_is_valid(void)
{
  uint32_t app_stack = *(volatile uint32_t *)RANGER_APP_START_ADDR;
  uint32_t app_reset = *(volatile uint32_t *)(RANGER_APP_START_ADDR + 4U);

  /*
   * Check that the application's initial stack pointer points to SRAM.
   *
   * STM32G4 SRAM is in the 0x20000000 region.
   */
  if ((app_stack & 0x2FFE0000U) != 0x20000000U)
  {
    return 0U;
  }

  /*
   * Check that the application's reset handler points into app flash area.
   */
  if (app_reset < RANGER_APP_START_ADDR)
  {
    return 0U;
  }

  if (app_reset >= RANGER_FLASH_END_ADDR)
  {
    return 0U;
  }

  return 1U;
}


static void jump_to_app(void)
{
  uint32_t app_stack = *(volatile uint32_t *)RANGER_APP_START_ADDR;
  uint32_t app_reset = *(volatile uint32_t *)(RANGER_APP_START_ADDR + 4U);

  app_entry_t app_entry = (app_entry_t)app_reset; // casting, app_entry now points to a function located at app_reset

  __disable_irq(); // disable global interrupts, we dont want those in the middle of a jump to app

  HAL_DeInit();

  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL  = 0U;

  SCB->VTOR = RANGER_APP_START_ADDR; // from now on interrupts should use the app vector table

  __set_MSP(app_stack); // Main Stack Pointer

  __enable_irq();   // enable interrupts again

  app_entry(); // jump to the reset handler
}

/**
 * @brief Handle received CAN frame commands for the bootloader task function
 */
void bootloader_handle_frame(ace_command_frame_t *frame)
{
  switch (frame->command_id)
  {
    case ACE_CMD_BOOT_PING:
      if(frame->parameter_id == ACE_PARAM_BOOT) // check parameter before deciding that this is a bootloader action
      {
    	     bootloader_handle_ping(frame);
      }
      else
      {
    	  ranger_can_send_response(
    			  frame->command_id,
				  frame->parameter_id,
				  ACE_STATUS_INVALID_PARAM,
				  0,
				  0
    	  	  );
      }
      break;

    case ACE_CMD_BOOT_START:
      bootloader_handle_start(frame);
      break;

    default:
      ranger_can_send_response(
          frame->command_id,
          frame->parameter_id,
          ACE_STATUS_UNKNOWN_COMMAND,
          0,
          0
      );
      break;
  }
}

/**
 * @brief Handle the BOOT_PING command
 */
static void bootloader_handle_ping(ace_command_frame_t *frame)
{
  uint8_t payload[5];

  payload[0] = ACE_PROTOCOL_VERSION;
  payload[1] = RANGER_BOOTLOADER_VERSION;
  payload[2] = ACE_STATE_BOOTLOADER; // The current module state is bootloader mode
  payload[3] = 0x00;
  payload[4] = 0x00;

  /* Keep bootloader alive when pinged */
  boot_start_ms = HAL_GetTick();

  ranger_can_send_response(
      ACE_CMD_BOOT_PING,
      frame->parameter_id,
	  ACE_STATUS_DATA_FOLLOWS,
      payload,
      5
  );
}

/**
 * @brief Handle the BOOT_START command
 */

static void bootloader_handle_start(ace_command_frame_t *frame)
{
  uint8_t payload[5] = {0};

  /*
   * For now:
   * payload[0..3] from host = firmware size, little-endian
   * payload[4] reserved
   */

  boot_expected_size =
      ((uint32_t)frame->payload[0]) |
      ((uint32_t)frame->payload[1] << 8) |
      ((uint32_t)frame->payload[2] << 16) |
      ((uint32_t)frame->payload[3] << 24);

  boot_received_size = 0;
  boot_write_address = RANGER_APP_START_ADDR;
  boot_expected_sequence = 0;
  boot_session_active = 1;

  boot_start_ms = HAL_GetTick();

  /*
   * Response payload:
   * byte 0: session active
   * byte 1-4: accepted firmware size
   */
  payload[0] = boot_session_active;
  payload[1] = (uint8_t)(boot_expected_size & 0xFF);
  payload[2] = (uint8_t)((boot_expected_size >> 8) & 0xFF);
  payload[3] = (uint8_t)((boot_expected_size >> 16) & 0xFF);
  payload[4] = (uint8_t)((boot_expected_size >> 24) & 0xFF);

  ranger_can_send_response(
      ACE_CMD_BOOT_START,
      frame->parameter_id,
	  ACE_STATUS_DATA_FOLLOWS,
      payload,
      5
  );
}


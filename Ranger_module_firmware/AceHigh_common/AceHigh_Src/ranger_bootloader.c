/*
 * ranger_bootloader.c
 *
 *  Created on: Apr 30, 2026
 *      Author: Tor Kaufmann Gjerde
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
static uint32_t boot_received = 0;
static uint32_t boot_write_address = RANGER_APP_START_ADDR;
static uint16_t boot_expected_sequence = 0;
static uint8_t  boot_session_active = 0;

/* =========================
   Private function prototypes
   ========================= */

static void bootloader_handle_frame(ace_command_frame_t *frame);
static void bootloader_handle_ping(ace_command_frame_t *frame);
static void bootloader_handle_start(ace_command_frame_t *frame);
static void bootloader_handle_data(ace_command_frame_t *frame);
static void bootloader_handle_end(ace_command_frame_t *frame);

static uint8_t write_flash(uint32_t address, const uint8_t *data, uint8_t len);
static uint8_t erase_app_area(uint32_t firmware_size);
static void jump_to_app(void);
static uint8_t app_is_valid(void);


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

static uint8_t erase_app_area(uint32_t firmware_size)
{
  (void)firmware_size;
  return 1U; // success
}

static uint8_t write_flash(uint32_t address, const uint8_t *data, uint8_t len)
{
  (void)address;
  (void)data;
  (void)len;
  return 1U; // success
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
				  ACE_STATUS_UNKNOWN_PARAM,
				  0,
				  0
    	  	  );
    	}
    	break;

    case ACE_CMD_BOOT_START:
    	if (frame->parameter_id == ACE_PARAM_BOOT)
    	{
    		bootloader_handle_start(frame);
    	}
    	else
    	{
    		ranger_can_send_response(
    			frame->command_id,
				frame->parameter_id,
				ACE_STATUS_UNKNOWN_PARAM,
				0,
				0
  	  	  );
    	}
    	break;

    case ACE_CMD_BOOT_DATA:
    	if (frame->parameter_id == ACE_PARAM_BOOT)
    	{
    		bootloader_handle_data(frame);
    	}
    	else
    	{
    		ranger_can_send_response(
    			frame->command_id,
				frame->parameter_id,
				ACE_STATUS_UNKNOWN_PARAM,
				0,
				0
        	);
    	}
    	break;

    case ACE_CMD_BOOT_END:
    	if (frame->parameter_id == ACE_PARAM_BOOT)
    	{
    		bootloader_handle_end(frame);
    	}
    	else
    	{
    		ranger_can_send_response(
    				frame->command_id,
					frame->parameter_id,
					ACE_STATUS_UNKNOWN_PARAM,
					0,
					0
    		);
      }
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

  payload[0] = ACE_STATE_BOOTLOADER;
  payload[1] = ACE_PROTOCOL_VERSION;
  payload[2] = RANGER_BOOTLOADER_VERSION;
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
 *
 * NOTE:
 * 	- Erases application space for previous content
 * 	- Set application space flash start address
 * 	- Stores the expected firmware size in bytes
 * 	- sets boot session flag to active
 * 	- resets boot timer
 * 	- sends the expected firmware size and boot session active in response payload
 * 	- initialize boot_received counter to 0 (counter for current bootloader session)
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
      ((uint32_t)frame->payload[1]) |
      ((uint32_t)frame->payload[2] << 8) |
      ((uint32_t)frame->payload[3] << 16) |
      ((uint32_t)frame->payload[4] << 24);

  if ((boot_expected_size == 0U) ||
      ((RANGER_APP_START_ADDR + boot_expected_size) > RANGER_FLASH_END_ADDR))
  {
	payload[0] = ACE_STATE_FAULT; // if expected size is zero or larger than available flash
	  	// specific macro to be added

    ranger_can_send_response(
        ACE_CMD_BOOT_START,
        frame->parameter_id,
		ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  if (!erase_app_area(boot_expected_size))
  {

	payload[0] = ACE_STATE_FAULT; // fault - erase application did not succeed
	  	  	// specific macro to be added

    ranger_can_send_response(
        ACE_CMD_BOOT_START,
        frame->parameter_id,
		ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  boot_received = 0; // counter for total amount of received boot bytes
  boot_write_address = RANGER_APP_START_ADDR;
  boot_expected_sequence = 0;
  boot_session_active = 1;

  boot_start_ms = HAL_GetTick();

  /*
   * Response payload:
   * byte 0: boot session active
   * byte 1-4: accepted firmware size
   */
  payload[0] =  ACE_STATE_BOOTLOADER;
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

/**
 * @brief Handle one BOOT_DATA frame.
 *
 * BOOT_DATA is used by the host PC to send firmware bytes to the bootloader.
 *
 * Ace command frame layout:
 *
 * byte 0: command_id
 * byte 1: parameter_id
 * byte 2: payload[0] = sequence_lsb
 * byte 3: payload[1] = sequence_msb
 * byte 4: payload[2] = firmware byte 0
 * byte 5: payload[3] = firmware byte 1
 * byte 6: payload[4] = firmware byte 2
 * byte 7: payload[5] = firmware byte 3
 *
 * This means each BOOT_DATA frame carries 4 firmware bytes.
 */
static void bootloader_handle_data(ace_command_frame_t *frame)
{
  uint16_t received_sequence;
  uint8_t bytes_to_write = 4U;
  uint8_t payload[5] = {0};

  /*
   * BOOT_DATA is only valid after BOOT_START has created an active
   * firmware update session.
   */
  if (!boot_session_active)
  {

	payload[0] = ACE_STATE_FAULT; // fault boot session not active
	// specific macro to be added

    ranger_can_send_response(
        ACE_CMD_BOOT_DATA,
        frame->parameter_id,
		ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * The sequence counter is sent as two bytes:
   *
   * payload[0] = least significant byte
   * payload[1] = most significant byte
   *
   * Example:
   * payload[0] = 0x34
   * payload[1] = 0x12
   *
   * received_sequence becomes 0x1234.
   *
   * The cast to uint16_t makes sure the shift happens safely
   * in the intended integer width.
   */
  received_sequence =
      ((uint16_t)frame->payload[0]) |
      ((uint16_t)frame->payload[1] << 8);

  /*
   * Check that frames arrive in the expected order.
   *
   * First BOOT_DATA frame should have sequence 0.
   * Second should have sequence 1.
   * Third should have sequence 2.
   * etc.
   */
  if (received_sequence != boot_expected_sequence)
  {
	payload[0] = ACE_STATE_FAULT; // fault as received sequence not equal to expected
	payload[1] = (uint8_t)(boot_expected_sequence & 0xFF); // lsb
	payload[2] = (uint8_t)((boot_expected_sequence >> 8) & 0xFF); // msb
	// specific macro to be added

    ranger_can_send_response(
        ACE_CMD_BOOT_DATA,
        frame->parameter_id,
		ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * Normally each BOOT_DATA frame writes 4 firmware bytes.
   *
   * The final frame may contain fewer than 4 useful bytes if the firmware
   * size is not divisible by 4.
   *
   * Example:
   * expected size = 10 bytes
   *
   * frame 0 writes 4 bytes
   * frame 1 writes 4 bytes
   * frame 2 writes 2 bytes
   *
   * boot_received: counter, tracks how many bytes have been received in total
   * boot_expected_size: the fixed number of total bytes expected to be received
   * bytes_to_write: fixed at 4 bytes - 4 bytes maximum received per Ace protocol payload frame
   */
  if ((boot_received + bytes_to_write) > boot_expected_size)
  {
    bytes_to_write = (uint8_t)(boot_expected_size - boot_received);
  }

  /*
   * Write the firmware bytes into flash.
   *
   * &frame->payload[2] means:
   * "address of payload element 2"
   *
   * Since payload[2] is the first firmware data byte,
   * this passes a pointer to the first firmware byte.
   * use the bytes_to_write variable to advance the pointer address for writing all 4 bytes
   */
  if (!write_flash(boot_write_address, &frame->payload[2], bytes_to_write))
  {
	payload[0] = ACE_STATE_FAULT; // fault as received sequence not equal to expected
	// specific macro to be added

    ranger_can_send_response(
        ACE_CMD_BOOT_DATA,
        frame->parameter_id,
		ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * Advance the flash write address by the number of bytes just written.
   */
  boot_write_address += bytes_to_write;

  /*
   * Track how many firmware bytes have been received in total.
   */
  boot_received += bytes_to_write;

  /*
   * The next BOOT_DATA frame must use the next sequence number.
   */
  boot_expected_sequence++;

  /*
   * Refresh bootloader timeout.
   *
   * This prevents the bootloader from timing out and jumping to the
   * application while a firmware update is in progress.
   */
  boot_start_ms = HAL_GetTick();

  /*
   * Acknowledge that this BOOT_DATA frame was accepted and written.
   */
  ranger_can_send_response(
      ACE_CMD_BOOT_DATA,
      frame->parameter_id,
      ACE_STATUS_OK,
      0,
      0
  );
}

/**
 * @brief Handle the BOOT_END command.
 *
 * BOOT_END tells the bootloader:
 * "The host has sent all firmware data frames."
 *
 * At this point the bootloader should:
 * - Check that a boot session is active
 * - Check that the received byte count matches the expected firmware size
 * - Validate that the new application looks bootable
 * - End the boot session
 *
 * Later versions can also:
 * - Flush a partial flash buffer
 * - Compute and compare CRC
 * - Store firmware metadata
 * - Mark application image as valid
 */
static void bootloader_handle_end(ace_command_frame_t *frame)
{
  uint8_t payload[5] = {0};

  /*
   * BOOT_END only makes sense if BOOT_START has already started
   * a firmware update session.
   */
  if (!boot_session_active)
  {
    payload[0] = ACE_STATE_FAULT;

    ranger_can_send_response(
        ACE_CMD_BOOT_END,
        frame->parameter_id,
        ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * No more BOOT_DATA frames should be accepted after this point,
   * unless the host starts a new session using BOOT_START.
   */
  boot_session_active = 0U;

  /*
   * Check that the number of received firmware bytes matches the size
   * announced during BOOT_START.
   */
  if (boot_received != boot_expected_size)
  {
    payload[0] = ACE_STATE_FAULT;

    payload[1] = (uint8_t)(boot_received & 0xFF);
    payload[2] = (uint8_t)((boot_received >> 8) & 0xFF);
    payload[3] = (uint8_t)((boot_received >> 16) & 0xFF);
    payload[4] = (uint8_t)((boot_received >> 24) & 0xFF);

    ranger_can_send_response(
        ACE_CMD_BOOT_END,
        frame->parameter_id,
        ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * Check that the application vector table looks valid.
   *
   * This checks:
   * - Initial stack pointer points to SRAM
   * - Reset handler points into application flash area
   */
  if (!app_is_valid())
  {
    payload[0] = ACE_STATE_FAULT;

    ranger_can_send_response(
        ACE_CMD_BOOT_END,
        frame->parameter_id,
        ACE_STATUS_DATA_FOLLOWS,
        payload,
        5
    );
    return;
  }

  /*
   * Success response.
   *
   * payload[0] = bootloader/application state
   * payload[1..4] = received firmware byte count
   */
  payload[0] = ACE_STATE_APP_VALID;
  payload[1] = (uint8_t)(boot_received & 0xFF);
  payload[2] = (uint8_t)((boot_received >> 8) & 0xFF);
  payload[3] = (uint8_t)((boot_received >> 16) & 0xFF);
  payload[4] = (uint8_t)((boot_received >> 24) & 0xFF);

  ranger_can_send_response(
      ACE_CMD_BOOT_END,
      frame->parameter_id,
      ACE_STATUS_DATA_FOLLOWS,
      payload,
      5
  );

  HAL_Delay(50);
  jump_to_app();

}


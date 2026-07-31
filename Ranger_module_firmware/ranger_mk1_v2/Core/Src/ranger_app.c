/*
 * ranger_app.c
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *  Description:
 *  Ranger application layer.
 *
 *  Owns:
 *  - Parameter state (e.g. LED state, driver circuitry states and all control hardware logic states)
 *  - Command handling (READ / WRITE)
 *  - Application timing (uptime, heartbeat scheduling)
 *  - Simple status indication (LED blink)
 *
 *  Does NOT own:
 *  - CAN hardware (ranger_can.c)
 *  - Protocol decoding (ace_protocol.c)
 */
#include "main.h"
#include "ranger_can.h"
#include "ranger_param.h"
#include "ace_protocol.h"
#include "ranger_app.h"
#include "DRV8462.h"
#include "INA229.h"
#include "FDC2214.h"

/* =========================
   Application state
   ========================= */

/*
 * Set by the TIM3 interrupt when the requested number of STEP pulses
 * has been generated.
 *
 * Volatile is required because this variable is modified inside an ISR.
 */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

static volatile bool step_move_complete = false;
static volatile bool step_move_active = false;
static volatile uint32_t step_pulses_remaining = 0U;

static uint32_t step_move_start_tick = 0U;
static uint32_t step_move_timeout_ms = 0U;

static uint8_t dir_old = 0U;

/* Current state of LED on PA1 (exposed via parameter interface) */
static uint8_t led_pa1_state = 0U;

/* Uptime counter in seconds */
static uint32_t uptime_s = 0U;

/* sensor measurements */
int32_t current;

/* FDC sensor measurements */
uint32_t raw_ch0;
uint32_t raw_ch1;
uint32_t raw_ch2;
uint32_t raw_ch3;
uint32_t raw_ch4;
uint32_t raw_ch5;
uint32_t raw_ch6;
uint32_t raw_ch7;
uint32_t raw_ch8;
uint32_t raw_ch9;
uint32_t raw_ch10;
uint32_t raw_ch11;

/* Timing references (ms) */
static uint32_t last_uptime_ms = 0U;
static uint32_t last_heartbeat_ms = 0U;
static uint32_t last_blink_ms = 0U;
static uint32_t last_ina_ms = 0;

/* Global helper */

/* =========================
   Internal helpers
   ========================= */
extern TIM_HandleTypeDef htim2;

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

#define FDC2214_ADDR_LOW   (0x2A << 1)
#define FDC2214_ADDR_HIGH  (0x2B << 1)

/**
 * @brief Set LED on PA1 and update internal state
 *
 * This is the only place that should directly control the LED.
 * Keeps hardware control and parameter state in sync.
 */
static void ranger_app_set_led_pa1(uint8_t state);

/**
 * @brief Handle READ command for a given parameter
 * Builds response payload and sends it via CAN.
 */
static void ranger_app_handle_read(const ace_command_frame_t *frame);

/**
 * @brief Handle WRITE command for a given parameter
 * Validates input payload and applies changes to hardware/state.
 */
static void ranger_app_handle_write(const ace_command_frame_t *frame);

/**
 * @brief local application functions
 */
static void ranger_app_check_reset(void);
static void ranger_app_check_step_enable(void);
static void ranger_app_check_step_move(void);
static void ranger_step_move(int32_t steps);
static void ranger_app_check_step_status(void);
static void ranger_load_next_step_chunk(void);



/* =========================
   Implementation
   ========================= */

static void ranger_app_set_led_pa1(uint8_t state)
{
  if (state != 0U)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    led_pa1_state = 1U;
  }
  else
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    led_pa1_state = 0U;
  }
}

/**
 * @brief Initialize application layer
 *
 * Called once after hardware init.
 *
 * Responsibilities:
 * - Set initial output states
 * - Reset internal variables
 * - Initialize timing references
 */
void ranger_app_init(void)
{
  /* Ensure known GPIO states for LED */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  /* Ensure known GPIO states for stepper stepper driver IC */
  HAL_GPIO_WritePin(GPIOB, DRV_DIR, GPIO_PIN_RESET); // DRV8462 Set direction pin
  HAL_GPIO_WritePin(GPIOB, DRV_MODE | DRV_CS, GPIO_PIN_SET); // DRV8462 Mode pin high for SPI mode
  HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOB, DRV_SLEEP, GPIO_PIN_SET); // DRV8462 nSLEEP high to disable sleep
  HAL_Delay(2);

  drv8462_init_fullstep_spi_mode();
  ranger_app_set_led_pa1(0U);

  ina229_init();
  uint8_t check = fdc2214_0_init();

  fdc2214_0_init();
  fdc2214_1_init();
  fdc2214_2_init();

  if(check != 1){
	  g_param.error_flag = check;
  }

  uptime_s = 0U;

  uint32_t now = HAL_GetTick();
  last_uptime_ms = now;
  last_heartbeat_ms = now;
  last_blink_ms = now;
}

  /* Initialize global parameter table g_param */



/**
 * @brief Application periodic task
 *
 * Called continuously from main loop.
 *
 * Responsibilities:
 * - Maintain uptime counter
 * - Send heartbeat periodically
 * - Toggle alive indicator LED
 *
 * This function acts as the main scheduler for simple time-based tasks.
 */
void ranger_app_tick(void)
{
  uint32_t now = HAL_GetTick();

  ace_command_frame_t command_frame;

  /* check for new CAN messages, if new message received handle it*/
  if (ranger_can_receive_command(&command_frame))
  {
    ranger_app_handle_command(&command_frame);
    /* change node ID if a request for change was previously sent*/
     ranger_can_process_pending_node_id_change();
  }

  /* Update uptime every 1 second */
  if ((now - last_uptime_ms) >= 1000U)
  {
    last_uptime_ms += 1000U;
    uptime_s++;
  }

  /* Send heartbeat every 5 seconds */
  if ((now - last_heartbeat_ms) >= 5000U)
  {
    last_heartbeat_ms += 5000U;

    /*ranger_can_send_heartbeat(ACE_STATE_STANDBY,
                              0x00U,
                              0x0000U,
                              uptime_s);
	*/
  }

  /* Blink LED on PA0 as "alive" indicator */
  if ((now - last_blink_ms) >= 250U)
  {
	last_blink_ms += 250U;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
  }

  //ranger_can_request_node_id_change(frame->payload[0]);

  /* Tasks scheduled to run every x miliseconds */
  if ((now - last_ina_ms) >=5U)
  {
      last_ina_ms = now;

      /* Read sensor values */
      g_param.current = (int32_t)(ina229_read_current()* 1000.0f);
      g_param.voltage = (int32_t)(ina229_read_volt() * 1000.0f);
      g_param.temperature = (int32_t)ina229_read_temp();

      //param.test = fdc2214_read_device_id();

      g_param.fdc0_ch0 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_LOW, 0);
      g_param.fdc0_ch1 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_LOW, 1);
      g_param.fdc0_ch2 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_LOW, 2);
      g_param.fdc0_ch3 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_LOW, 3);

      g_param.fdc1_ch0 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 0);
      g_param.fdc1_ch1 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 1);
      g_param.fdc1_ch2 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 2);
      g_param.fdc1_ch3 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 3);

      g_param.fdc2_ch0 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 0);
      g_param.fdc2_ch1 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 1);
      g_param.fdc2_ch2 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 2);
      g_param.fdc2_ch3 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 3);


	  /* set step frequency with latest parameter value */


	  ranger_app_check_reset();
	  ranger_app_check_step_enable();

	  ranger_app_check_step_status();
	  ranger_app_check_step_move();
  }

}

/**
 * @brief Entry point for handling decoded protocol commands
 *
 * Called from application tick after CAN frame reception and ACE decoding.
 *
 * Dispatches commands to READ / WRITE handlers.
 */
void ranger_app_handle_command(const ace_command_frame_t *frame)
{
  if (frame == NULL)
  {
    return;
  }

  switch (frame->command_id)
  {
    case ACE_CMD_READ:
      ranger_app_handle_read(frame);
      break;

    case ACE_CMD_WRITE:
      ranger_app_handle_write(frame);
      break;

    default:
      /* Unknown command → respond with error */
      ranger_can_send_response(frame->command_id,
                               frame->parameter_id,
                               ACE_STATUS_UNKNOWN_COMMAND,
                               NULL,
                               0U);
      break;
  }
}

/**
 * @brief Handle READ command
 *
 * Reads parameter value and returns it via CAN response.
 */
static void ranger_app_handle_read(const ace_command_frame_t *frame)
{
  uint8_t payload[4] = {0};
  uint32_t param_value;

  if(ranger_param_read(frame->parameter_id, &param_value)){

	  payload[0] = (uint8_t)(param_value & 0xFFU);
	  payload[1] = (uint8_t)((param_value >> 8) & 0xFFU);
	  payload[2] = (uint8_t)((param_value >> 16) & 0xFFU);
	  payload[3] = (uint8_t)((param_value >> 24) & 0xFFU);

	  ranger_can_send_response(ACE_CMD_READ,
	  	  					   frame->parameter_id,
	  						   ACE_STATUS_DATA_FOLLOWS,
	  						   payload,
	  						   4);
  }
  else
  {
	  ranger_can_send_response(ACE_CMD_READ,
                               frame->parameter_id,
							   ACE_STATUS_UNKNOWN_PARAM,
							   NULL,
							   0U);
  }
}

/**
 * @brief Handle WRITE command
 *
 * Validates payload and applies new parameter value.
 */
static void ranger_app_handle_write(const ace_command_frame_t *frame)
{
  uint32_t param_value;

  param_value = ((uint32_t)frame->payload[0]) |
		 	 	((uint32_t)frame->payload[1] << 8)|
				((uint32_t)frame->payload[2] << 16)|
				((uint32_t)frame->payload[3] << 24);

  if(ranger_param_write(frame->parameter_id, param_value)){

	  ranger_can_send_response(ACE_CMD_WRITE,
	  	  					   frame->parameter_id,
							   ACE_STATUS_OK,
	  						   0,
	  						   0);
  }
  else
  {
	  ranger_can_send_response(ACE_CMD_WRITE,
                               frame->parameter_id,
							   ACE_STATUS_UNKNOWN_PARAM,
							   NULL,
							   0U);
  }
}


static void ranger_app_check_reset(void)
{
	if(g_param.reset == 1)
	{
		NVIC_SystemReset();
	}
}


static void ranger_app_check_step_enable(void)
{
	if(g_param.step_enable == 0)
	{
		HAL_GPIO_WritePin(GPIOB, DRV_ENABLE, GPIO_PIN_RESET);
	}
	if(g_param.step_enable == 1)
	{
		HAL_GPIO_WritePin(GPIOB, DRV_ENABLE, GPIO_PIN_SET);
	}
}

static void ranger_app_check_step_move(void)
{
    int32_t requested_steps;

    /*
     * g_param.step_move acts as a one-command mailbox.
     */
    if (g_param.step_move == 0)
    {
        return;
    }

    /*
     * Leave the command in the mailbox while a move is active.
     *
     * This creates a one-command pending queue:
     * the command will start after the current move completes.
     *
     * A later CAN write before completion will overwrite the pending
     * command with the newest value.
     */
    if (step_move_active)
    {
        return;
    }

    /*
     * Copy the command before clearing the mailbox.
     */
    requested_steps = g_param.step_move;
    g_param.step_move = 0;

    /*
     * This function starts the move and returns immediately.
     */
    ranger_step_move(requested_steps);
}


static void ranger_app_check_step_status(void)
{
    if (!step_move_active)
    {
        return;
    }

    /*
     * The TIM3 interrupt sets this flag after the final pulse chunk.
     */
    if (step_move_complete)
    {
        step_move_complete = false;
        step_move_active = false;
        step_pulses_remaining = 0U;

        return;
    }

    /*
     * Timeout protection. Unsigned subtraction remains valid across
     * HAL_GetTick() rollover.
     */
    if ((HAL_GetTick() - step_move_start_tick) >= step_move_timeout_ms)
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
        HAL_TIM_Base_Stop_IT(&htim3);

        step_pulses_remaining = 0U;
        step_move_complete = false;
        step_move_active = false;

        /*
         * Optional:
         * Set a dedicated motion timeout fault here.
         *
         * Example:
         * g_param.error_flag |= RANGER_ERROR_STEP_TIMEOUT;
         */
    }
}

static void ranger_set_step_dir(uint8_t dir)
{
    /*
     * Normalize any nonzero value to one.
     */
    dir = (dir != 0U) ? 1U : 0U;

    if (dir == dir_old)
    {
        return;
    }

    if (dir == 0U)
    {
        HAL_GPIO_WritePin(GPIOB, DRV_DIR, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOB, DRV_DIR, GPIO_PIN_SET);
    }

    dir_old = dir;
}


/**
 * @brief Start an asynchronous finite STEP move.
 *
 * TIM2 generates the STEP pulse train.
 * TIM3 counts the generated pulses.
 *
 * Moves larger than 65,536 pulses are divided into multiple TIM3
 * counting chunks. TIM2 is stopped briefly while each new chunk is
 * loaded so that no uncounted STEP pulses occur between chunks.
 *
 * @param steps
 *        Positive: positive direction.
 *        Negative: negative direction.
 *        Zero: no movement.
 *
 * @note This function is non-blocking.
 * @note The main application loop continues servicing CAN and sensors.
 * @note Do not call this function from an interrupt handler.
 */
static void ranger_step_move(int32_t steps)
{
    uint32_t pulse_count;
    uint32_t pulse_frequency_hz;
    uint32_t chunk_count;
    uint64_t expected_move_ms;
    uint64_t timeout_ms;

    if (steps == 0)
    {
        return;
    }

    if (step_move_active)
    {
        return;
    }

    /*
     * Convert the signed command into direction and unsigned magnitude.
     *
     * This conversion also safely handles INT32_MIN.
     */
    if (steps > 0)
    {
        ranger_set_step_dir(1U);
        pulse_count = (uint32_t)steps;
    }
    else
    {
        ranger_set_step_dir(0U);
        pulse_count = 0U - (uint32_t)steps;
    }

    /*
     * PROFILE_VELOCITY is assumed to contain STEP pulses per second.
     */
    pulse_frequency_hz = g_param.profile_velocity;

    if (pulse_frequency_hz == 0U)
    {
        /*
         * A zero pulse frequency cannot produce motion.
         */
        return;
    }

    /*
     * Calculate nominal move duration using 64-bit arithmetic.
     * The division is rounded upward.
     */
    expected_move_ms =
        (((uint64_t)pulse_count * 1000ULL) +
         ((uint64_t)pulse_frequency_hz - 1ULL)) /
        (uint64_t)pulse_frequency_hz;

    /*
     * Determine the number of 16-bit TIM3 chunks.
     */
    chunk_count =
        (pulse_count + (65536U - 1U)) / 65536U;

    /*
     * Add:
     * - 250 ms general margin
     * - 2 ms margin for each timer chunk
     *
     * The chunk margin covers the brief stop/reload/restart operation.
     */
    timeout_ms =
        expected_move_ms +
        250ULL +
        ((uint64_t)chunk_count * 2ULL);

    /*
     * Saturate to the 32-bit HAL tick range.
     */
    if (timeout_ms > UINT32_MAX)
    {
        step_move_timeout_ms = UINT32_MAX;
    }
    else
    {
        step_move_timeout_ms = (uint32_t)timeout_ms;
    }

    /*
     * Put both timers into a known stopped state before configuring
     * the first pulse-count chunk.
     */
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
    HAL_TIM_Base_Stop_IT(&htim3);

    step_pulses_remaining = pulse_count;
    step_move_complete = false;
    step_move_active = true;
    step_move_start_tick = HAL_GetTick();

    /*
     * Configure and start TIM3 before starting TIM2. This ensures that
     * the first generated STEP pulse is counted.
     */
    ranger_load_next_step_chunk();

    /*
     * ranger_load_next_step_chunk() clears step_move_active if TIM3
     * could not be started.
     */
    if (!step_move_active)
    {
        return;
    }

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
    {
        HAL_TIM_Base_Stop_IT(&htim3);

        step_pulses_remaining = 0U;
        step_move_complete = false;
        step_move_active = false;

        /*
         * Optional:
         * Set a timer start fault here.
         */
        return;
    }
}


static void ranger_load_next_step_chunk(void)
{
    uint32_t chunk;

    if (step_pulses_remaining == 0U)
    {
        /*
         * There is no next chunk to load.
         * Final completion is normally handled by the TIM3 callback.
         */
        step_move_complete = true;
        return;
    }

    /*
     * TIM3 is a 16-bit timer and can count at most 65,536 events:
     *
     * counter values 0 through 65,535 inclusive.
     */
    if (step_pulses_remaining > 65536U)
    {
        chunk = 65536U;
    }
    else
    {
        chunk = step_pulses_remaining;
    }

    /*
     * Reserve this chunk before starting TIM3.
     *
     * step_pulses_remaining therefore represents pulses not yet assigned
     * to the currently running TIM3 chunk.
     */
    step_pulses_remaining -= chunk;

    /*
     * TIM3 must already be stopped when this function is called.
     */
    __HAL_TIM_DISABLE(&htim3);
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);

    __HAL_TIM_SET_COUNTER(&htim3, 0U);

    /*
     * TIM3 counts from zero through ARR inclusive.
     *
     * ARR = chunk - 1 produces exactly 'chunk' count events.
     */
    __HAL_TIM_SET_AUTORELOAD(&htim3, chunk - 1U);

    /*
     * Transfer the new ARR value into the active timer register.
     *
     * EGR.UG may set the update flag, so the flag must be cleared
     * after generating the update event.
     */
    htim3.Instance->EGR = TIM_EGR_UG;

    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);

    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
    {
        step_pulses_remaining = 0U;
        step_move_complete = false;
        step_move_active = false;

        /*
         * Optional:
         * Set a TIM3 start fault here.
         */
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return;
    }

    if (htim->Instance != TIM3)
    {
        return;
    }

    /*
     * Stop STEP generation immediately at the chunk boundary.
     *
     * This prevents TIM2 from generating pulses while TIM3 is being
     * reconfigured. Without this stop, one or more pulses could occur
     * without being counted at each 65,536-pulse boundary.
     */
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
    HAL_TIM_Base_Stop_IT(&htim3);

    if (step_pulses_remaining > 0U)
    {
        /*
         * Configure and start TIM3 for the next pulse chunk.
         */
        ranger_load_next_step_chunk();

        if (!step_move_active)
        {
            /*
             * TIM3 failed to start.
             */
            return;
        }

        /*
         * Resume STEP generation only after TIM3 is ready.
         */
        if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
        {
            HAL_TIM_Base_Stop_IT(&htim3);

            step_pulses_remaining = 0U;
            step_move_complete = false;
            step_move_active = false;

            /*
             * Optional:
             * Set a TIM2 restart fault here.
             */
        }
    }
    else
    {
        /*
         * The final TIM3 chunk has completed.
         *
         * The foreground application task clears step_move_active.
         */
        step_move_complete = true;
    }
}

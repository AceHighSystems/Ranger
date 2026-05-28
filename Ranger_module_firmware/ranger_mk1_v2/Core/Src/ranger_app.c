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

uint8_t dir_old = 0;
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
void ranger_set_step_frequency(uint32_t freq_hz);
static void ranger_app_check_step_dir(void);


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

  fdc2214_1_init();
  fdc2214_2_init();
  fdc2214_3_init();

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

      g_param.fdc1_ch0 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_HIGH, 0);
      g_param.fdc1_ch1 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_HIGH, 1);
      g_param.fdc1_ch2 = fdc2214_read_ch(&hi2c1, FDC2214_ADDR_HIGH, 2);

      g_param.fdc2_ch0 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 0);
      g_param.fdc2_ch1 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 1);
      g_param.fdc2_ch2 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_LOW, 2);

      g_param.fdc3_ch0 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 0);
      g_param.fdc3_ch1 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 1);
      g_param.fdc3_ch2 = fdc2214_read_ch(&hi2c2, FDC2214_ADDR_HIGH, 2);

	  /* set step frequency with latest parameter value */
	  ranger_set_step_frequency(g_param.step_freq);

	  ranger_app_check_reset();
	  ranger_app_check_step_enable();
	  ranger_app_check_step_dir();
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


static void ranger_app_check_step_dir(void)
{
	uint8_t dir_new = g_param.step_dir;

	if(dir_new =! dir_old){
		if(g_param.step_dir == 0)
		{
			HAL_GPIO_WritePin(GPIOB, DRV_DIR, GPIO_PIN_RESET);
		}
		if(g_param.step_dir == 1)
		{
			HAL_GPIO_WritePin(GPIOB, DRV_DIR, GPIO_PIN_SET);
		}

	}
	dir_old = dir_new;

}

/**
 * @brief Change PWM frequency
 * NOTE:
 * The pre-scaler is set to 169+1 such that tick frequency is equal to 170/170 = 1MHz
 */
void ranger_set_step_frequency(uint32_t freq_hz)
{
    static uint32_t last_freq_hz = 0;
    static uint8_t pwm_running = 0;

    if (freq_hz == last_freq_hz)
    {
        return;
    }

    last_freq_hz = freq_hz;

    if (freq_hz == 0)
    {
        if (pwm_running)
        {
            HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
            pwm_running = 0;
        }
        return;
    }

    uint32_t arr = (1000000UL / freq_hz) - 1UL;

    if (arr < 4)
    {
        arr = 4; // avoid invalid pulse width situation
    }

    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 3);

    TIM2->EGR = TIM_EGR_UG;

    if (!pwm_running)
    {
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
        pwm_running = 1;
    }
}

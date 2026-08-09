/*
 * ranger_app.c
 *
 *  Created on: Apr 28, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *  Description:
 *  Ranger application layer.
*/

#include "main.h"
#include "ranger_can.h"
#include "ranger_param.h"
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

  ranger_can_init();


  /* Initialize peripheral drivers*/
  drv8462_init_fullstep_spi_mode();
  ranger_app_set_led_pa1(0U);
  ina229_init();
  fdc2214_0_init();
  fdc2214_1_init();
  fdc2214_2_init();


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



  /* Update uptime every 1 second */
  if ((now - last_uptime_ms) >= 1000U)
  {
    last_uptime_ms += 1000U;
    uptime_s++;
  }



  /* Blink LED on PA0 as "alive" indicator */
  if ((now - last_blink_ms) >= 250U)
  {
	last_blink_ms += 250U;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
  }

  /* Tasks scheduled to run every x miliseconds */
  if ((now - last_ina_ms) >=10U)
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

  }

  ranger_app_check_reset();
  ranger_app_check_step_enable();

  ranger_app_check_step_status();
  ranger_app_check_step_move();

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
    /*
     * TIM2 counter frequency after the configured prescaler.
     *
     * This assumes TIM2 is configured to count at 1 MHz:
     * one timer count equals 1 microsecond.
     */
    const uint32_t tim2_counter_hz = 1000000U;

    /*
     * Desired STEP pulse high time.
     *
     * A value of 3 produces a nominal 3 microsecond high pulse
     * when the PWM period is long enough.
     */
    const uint32_t step_high_time_ticks = 3U;

    uint32_t pulse_count;
    uint32_t pulse_frequency_hz;
    uint32_t timer_period_ticks;
    uint32_t pulse_high_ticks;
    uint32_t chunk_count;
    uint64_t expected_move_ms;
    uint64_t timeout_ms;

    /*
     * Ignore zero-length moves.
     */
    if (steps == 0)
    {
        return;
    }

    /*
     * Do not start another finite move while one is already active.
     */
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
     * PROFILE_VELOCITY contains the requested STEP pulse frequency
     * in pulses per second.
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
     * At least two TIM2 counts are required for each PWM period:
     *
     * - At least one count with STEP high.
     * - At least one count with STEP low.
     *
     * With a 1 MHz timer counter, the maximum usable STEP frequency
     * is therefore 500 kHz.
     */
    if (pulse_frequency_hz > (tim2_counter_hz / 2U))
    {
        pulse_frequency_hz = tim2_counter_hz / 2U;
    }

    /*
     * Calculate the number of TIM2 counts in one PWM period.
     *
     * The addition of half the requested frequency rounds the result
     * to the nearest integer instead of always rounding downward.
     *
     * PWM frequency:
     *
     *     frequency = TIM2 counter frequency / period ticks
     *
     * TIM2 ARR:
     *
     *     ARR = period ticks - 1
     */
    timer_period_ticks =
        (tim2_counter_hz + (pulse_frequency_hz / 2U)) /
        pulse_frequency_hz;

    /*
     * Enforce the minimum period needed to provide both a high
     * and low part of the STEP waveform.
     */
    if (timer_period_ticks < 2U)
    {
        timer_period_ticks = 2U;
    }

    /*
     * Calculate the actual frequency produced by the integer timer
     * period. This may differ slightly from the requested frequency
     * because ARR can only contain an integer number of timer ticks.
     *
     * Use the actual frequency for the timeout calculation.
     */
    pulse_frequency_hz =
        tim2_counter_hz / timer_period_ticks;

    /*
     * Use a nominal STEP high time of 3 microseconds.
     */
    pulse_high_ticks = step_high_time_ticks;

    /*
     * At high STEP frequencies, the complete PWM period may be shorter
     * than the requested fixed pulse-high time.
     *
     * In that case, reduce the pulse width to approximately 50 percent
     * of the PWM period.
     */
    if (pulse_high_ticks >= timer_period_ticks)
    {
        pulse_high_ticks = timer_period_ticks / 2U;
    }

    /*
     * CCR2 must be at least one timer tick to produce a high pulse.
     */
    if (pulse_high_ticks == 0U)
    {
        pulse_high_ticks = 1U;
    }

    /*
     * Calculate nominal move duration using 64-bit arithmetic.
     *
     * pulse_frequency_hz now contains the actual frequency generated
     * by TIM2, rather than only the requested PROFILE_VELOCITY value.
     *
     * The division is rounded upward.
     */
    expected_move_ms =
        (((uint64_t)pulse_count * 1000ULL) +
         ((uint64_t)pulse_frequency_hz - 1ULL)) /
        (uint64_t)pulse_frequency_hz;

    /*
     * Determine the number of 16-bit TIM3 chunks.
     *
     * TIM3 can count a maximum of 65,536 STEP pulses per chunk.
     */
    chunk_count =
        (pulse_count + (65536U - 1U)) / 65536U;

    /*
     * Add:
     *
     * - 100 ms general timeout margin.
     * - 2 ms margin for each TIM3 counting chunk.
     *
     * The per-chunk margin covers the brief TIM2 stop and TIM3
     * reload operation at each 65,536-pulse boundary.
     */
    timeout_ms =
        expected_move_ms +
        100ULL +
        ((uint64_t)chunk_count * 2ULL);

    /*
     * Saturate the timeout to the 32-bit HAL tick range.
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
     * Put both timers into a known stopped state before updating TIM2
     * and configuring the first TIM3 pulse-count chunk.
     */
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
    HAL_TIM_Base_Stop_IT(&htim3);

    /*
     * Disable TIM2 while changing ARR, CCR2 and CNT.
     */
    __HAL_TIM_DISABLE(&htim2);

    /*
     * Update the TIM2 PWM period.
     *
     * TIM2 counts from zero through ARR inclusive, so:
     *
     *     ARR = period ticks - 1
     */
    __HAL_TIM_SET_AUTORELOAD(
        &htim2,
        timer_period_ticks - 1U
    );

    /*
     * Update the STEP pulse high time.
     *
     * In PWM mode 1, the output is active while:
     *
     *     CNT < CCR2
     */
    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_2,
        pulse_high_ticks
    );

    /*
     * Generate an update event to transfer the new ARR and CCR2 values
     * from their preload registers into the active timer registers.
     *
     * The update event may set the TIM2 update flag, so clear it below.
     */
    htim2.Instance->EGR = TIM_EGR_UG;

    /*
     * Start the new PWM sequence from the beginning of the period.
     */
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

    /*
     * Initialize the finite-move state.
     */
    step_pulses_remaining = pulse_count;
    step_move_complete = false;
    step_move_active = true;
    step_move_start_tick = HAL_GetTick();

    /*
     * Configure and start TIM3 before starting TIM2.
     *
     * This ensures that TIM3 is ready to count the first generated
     * STEP pulse.
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

    /*
     * Start STEP pulse generation using the newly loaded TIM2 ARR
     * and CCR2 values.
     */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
    {
        /*
         * Stop TIM3 because no valid STEP pulse train was started.
         */
        HAL_TIM_Base_Stop_IT(&htim3);

        /*
         * Clear the finite-move state.
         */
        step_pulses_remaining = 0U;
        step_move_complete = false;
        step_move_active = false;

        /*
         * Optional:
         * Set a TIM2 PWM start fault here.
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

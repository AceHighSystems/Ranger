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
#include "main.h"

typedef void (*app_entry_t)(void); // function pointer


#define BOOT_BLINK_INTERVAL_MS  100U

static uint32_t boot_start_ms = 0U;
static uint32_t boot_blink_last_ms = 0U;


static uint8_t ranger_app_is_valid(void)
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


static void ranger_jump_to_app(void)
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


void ranger_bootloader_init(void)
{
  boot_start_ms = HAL_GetTick();
  boot_blink_last_ms = boot_start_ms;
}


void ranger_bootloader_task(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - boot_blink_last_ms) >= BOOT_BLINK_INTERVAL_MS)
  {
    boot_blink_last_ms = now;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);   // change pin if needed
  }

  if ((now - boot_start_ms) > RANGER_BOOT_TIMEOUT_MS)
  {
    if (ranger_app_is_valid())
    {
      ranger_jump_to_app();
    }
  }
}

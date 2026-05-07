/*
 * ranger_bootloader.h
 *
 *  Created on: Apr 30, 2026
 *      Author: torgj
 *
 *  Minimal Ranger-Mk1 CAN bootloader support
 */

#ifndef INC_RANGER_BOOTLOADER_H_
#define INC_RANGER_BOOTLOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Application location in flash */
#define RANGER_APP_START_ADDR    0x08008000U
#define RANGER_FLASH_END_ADDR    0x08080000U

#define RANGER_APP_FLASH_SIZE  (RANGER_FLASH_END_ADDR - RANGER_APP_START_ADDR)

/* Bootloader behavior */
#define RANGER_BOOTLOADER_VERSION	0x01
#define RANGER_BOOT_TIMEOUT_MS    	1000U
#define BOOT_BLINK_INTERVAL_MS   	100U


/**
 * @brief Initialize bootloader over CAN
 *
 * Responsibilities:
 * - Start FDCAN peripheral
 * - Enable RX interrupts
 * - Prepare internal TX headers
 */
void ranger_bootloader_init(void);

/**
 * @brief bootloader task, called continously from main
 *
 * Responsibilities:
 */
void ranger_bootloader_task(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_RANGER_BOOTLOADER_H_ */



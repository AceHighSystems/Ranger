/*
 * ranger_params.h
 *
 *  Created on: Apr 27, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_RANGER_PARAM_H_
#define INC_RANGER_PARAM_H_

#include <stdbool.h>
#include <stddef.h>
#include "ace_protocol.h"

/* =========================
   System (0x10–0x1F)
   ========================= */
#define RANGER_PARAM_NODE_ID   		  0x10U
#define PARAM_STATE            	      0x12U
#define PARAM_UPTIME           	   	  0x13U
#define PARAM_RESET			   		  0x14U

/* =========================
   Motion (0x40–0x5F)
   ========================= */
#define PARAM_STEP_ENABLE      	 	  0x40U
#define PARAM_STEP_MICRO	  		  0x41U
#define PARAM_STEP_DIR		   		  0x42U
#define PARAM_STEP_FREQ		  		  0x43U

/* =========================
   Encoder (0x60–0x7F)
   ========================= */
#define PARAM_ENCODER_POSITION 		  0x60U

/* =========================
   Telemetry (0x80–0x9F)
   ========================= */
#define PARAM_VOLTAGE          		  0x80U
#define PARAM_CURRENT        		  0x81U
#define PARAM_TEMPERATURE          	  0x82U

/* =========================
   Diagnostics (0xC0–0xDF)
   ========================= */
#define PARAM_LED_PA1          		  0xC0U
#define PARAM_TEST          		  0xC1U
#define PARAM_ERROR_FLAG              0xC2U

#define PARAM_FDC0_CH0        		  0xD0U
#define PARAM_FDC0_CH1       		  0xD1U
#define PARAM_FDC0_CH2       		  0xD2U



void ranger_param_init(void);
void ranger_param_handle_command(const ace_command_frame_t *frame);

bool ranger_param_write(uint8_t id, uint32_t value);
bool ranger_param_read(uint8_t id, uint32_t *value);

typedef struct
{
	uint8_t   reset;

    int32_t   voltage;
    int32_t   current;
    int32_t   temperature;

    uint8_t   step_enable; // Is it better to map this to HW sleep?
    uint8_t   step_microstep;
    uint8_t	  step_dir;

	uint32_t  step_freq;

	uint8_t   led1;

    uint32_t  uptime_s;
    uint32_t  error_flag;

    uint32_t  test;

    uint32_t  fdc0_ch0;
    uint32_t  fdc0_ch1;
    uint32_t  fdc0_ch2;

} ranger_param_t;

extern ranger_param_t g_param;


typedef enum
{
    PARAM_U8,
    PARAM_U16,
    PARAM_U32,
    PARAM_I16,
    PARAM_I32
} ranger_param_type_t;


typedef enum
{
    PARAM_RO,
    PARAM_RW
} ranger_param_access_t;

typedef struct
{
    uint8_t id;
    void *ptr;
    ranger_param_type_t type;
    ranger_param_access_t access;
} ranger_param_entry_t;

#endif /* INC_RANGER_PARAM_H_ */



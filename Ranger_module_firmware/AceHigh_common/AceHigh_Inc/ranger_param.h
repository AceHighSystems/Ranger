/*
 * ranger_param.h
 *
 *  Created on: Apr 27, 2026
 *      Author: Tor Kaufmann Gjerde
 */

#ifndef INC_RANGER_PARAM_H_
#define INC_RANGER_PARAM_H_

#include <stdint.h>
#include "ace_light.h"

/* ============================================================
 * AceLight reserved parameter IDs 0x00 to 0x0F
 * ============================================================ */

/* =========================
   System (0x10–0x1F)
   ========================= */
#define PARAM_RESET     	 	      0x10U


/* =========================
   Motion (0x40–0x5F)
   ========================= */
#define PARAM_STEP_ENABLE      	 	  0x40U
#define PARAM_STEP_MOVE				  0x41U
#define PARAM_TARGET_POSITION	 	  0x42U
#define PARAM_POSITION	 	          0x43U
#define PARAM_ANGLE	 	 			  0x44U

#define PARAM_PROFILE_VELOCITY        0x45U
#define PARAM_PROFILE_ACCELERATION    0x46U
#define PARAM_PROFILE_DECELERATION    0x47U
#define PARAM_MICROSTEP	  		      0x48U

/* =========================
   Encoder (0x60–0x7F)
   ========================= */

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

#define PARAM_RAW_CH0       		  0xD0U
#define PARAM_RAW_CH1      		      0xD1U
#define PARAM_RAW_CH2      		      0xD2U
#define PARAM_RAW_CH3      		      0xD3U
#define PARAM_RAW_CH4      		      0xD4U
#define PARAM_RAW_CH5      		      0xD5U
#define PARAM_RAW_CH6      		      0xD6U
#define PARAM_RAW_CH7      		      0xD7U
#define PARAM_RAW_CH8      		      0xD8U
#define PARAM_RAW_CH9      		      0xD9U
#define PARAM_RAW_CH10      		  0xDAU
#define PARAM_RAW_CH11     		      0xDBU


/*
 * Ranger runtime parameter storage.
 */
typedef struct
{
	/* AceLight standard parameters */
	uint8_t   sync;
	uint8_t   node_id;
	uint32_t  device_type;
	uint32_t  serial_number;
	uint32_t  firmware_version;
	uint32_t  hardware_version;
	uint32_t  protocol_version;

	/* Ranger system parameters */
	uint8_t   reset;

	/* Telemetry */
    int32_t   voltage;
    int32_t   current;
    int32_t   temperature;

    /* Motion */
    uint8_t   step_enable;
    int32_t   step_move;
    int32_t	  target_position;
    int32_t	  position;
    int32_t   angle;

	uint32_t  profile_velocity;
	uint32_t  profile_acceleration;
	uint32_t  profile_deceleration;
    uint32_t  microstep;

    /* Diagnostics */
	uint8_t   led1;
    uint32_t  uptime_s;
    uint32_t  error_flag;

    uint32_t  test;

    /* Encoder raw channels - prototyping */
    uint32_t  fdc0_ch0;
    uint32_t  fdc0_ch1;
    uint32_t  fdc0_ch2;
    uint32_t  fdc0_ch3;

    uint32_t  fdc1_ch0;
    uint32_t  fdc1_ch1;
    uint32_t  fdc1_ch2;
    uint32_t  fdc1_ch3;

    uint32_t  fdc2_ch0;
    uint32_t  fdc2_ch1;
    uint32_t  fdc2_ch2;
    uint32_t  fdc2_ch3;
} ranger_param_t;



/* Ranger runtime parameter instance. Defined in ranger_param.c. */
extern ranger_param_t g_param;

/* Complete Ranger AceLight parameter dictionary. Defined in ranger_param.c. */
extern const ace_parameter_t ranger_param_table[];

/*  Number of entries in ranger_param_table[]. */
extern const uint16_t ranger_param_count;


#endif /* INC_RANGER_PARAM_H_ */



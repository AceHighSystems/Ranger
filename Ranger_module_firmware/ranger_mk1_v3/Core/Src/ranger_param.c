


#include "ace_light.h"
#include "ranger_param.h"


/*
 * Ranger runtime parameter storage.
 *
 * Variables not explicitly initialized below are initialized to zero
 * by the C runtime.
 */
ranger_param_t g_param =
{
    /* AceLight standard parameters */
    .sync             = 0U,
    .node_id          = 2U,
    .device_type      = 0U,
    .serial_number    = 0U,
    .firmware_version = 0U,
    .hardware_version = 0U,
    .protocol_version = 0U,

    /* Ranger parameters */
    .reset           = 0U,
    .step_enable     = 0U,
    .step_move       = 0,
    .target_position = 0,
    .microstep       = 1U,
    .led1            = 1U,
    .test            = 2U,
    .error_flag      = 0U
};


/*
 * Ranger AceLight parameter dictionary.
 *
 * This is the only Ranger-specific information required by the
 * generic AceLight parameter handling code.
 *
 * Each entry defines:
 *
 *   Parameter ID
 *   RAM location
 *   Data type
 *   Access permission
 */
const ace_parameter_t ranger_param_table[] =
{
	/* AceLight standard parameters */

	{ ACE_PARAM_SYNC,   		   &g_param.sync,        	       ACE_PARAM_U8,  ACE_PARAM_RW },
	{ ACE_PARAM_NODE_ID,   		   &g_param.node_id,        	   ACE_PARAM_U8,  ACE_PARAM_RW },
	{ ACE_PARAM_DEVICE_TYPE,   	   &g_param.device_type,           ACE_PARAM_U32, ACE_PARAM_RO },
	{ ACE_PARAM_SERIAL_NUMBER,     &g_param.serial_number,         ACE_PARAM_U32, ACE_PARAM_RO },
	{ ACE_PARAM_FIRMWARE_VERSION,  &g_param.firmware_version,      ACE_PARAM_U32, ACE_PARAM_RO },
	{ ACE_PARAM_HARDWARE_VERSION,  &g_param.hardware_version,      ACE_PARAM_U32, ACE_PARAM_RO },
	{ ACE_PARAM_PROTOCOL_VERSION,  &g_param.protocol_version,      ACE_PARAM_U32, ACE_PARAM_RO },

	/* Device specific parameters - Ranger Mk1 */

    { PARAM_VOLTAGE,   		 	   &g_param.voltage,        	   ACE_PARAM_I32, ACE_PARAM_RO },
	{ PARAM_CURRENT,  		  	   &g_param.current,        	   ACE_PARAM_I32, ACE_PARAM_RO },
	{ PARAM_TEMPERATURE,  	  	   &g_param.temperature,           ACE_PARAM_I32, ACE_PARAM_RO },

    { PARAM_STEP_ENABLE,     	   &g_param.step_enable,           ACE_PARAM_U8,  ACE_PARAM_RW },
	{ PARAM_STEP_MOVE,       	   &g_param.step_move,             ACE_PARAM_I32, ACE_PARAM_RW },
	{ PARAM_TARGET_POSITION,  	   &g_param.target_position,       ACE_PARAM_I32, ACE_PARAM_RW },
	{ PARAM_POSITION,         	   &g_param.position,              ACE_PARAM_I32, ACE_PARAM_RO },
	{ PARAM_ANGLE,           	   &g_param.angle,                 ACE_PARAM_I32, ACE_PARAM_RO },
	{ PARAM_PROFILE_VELOCITY, 	   &g_param.profile_velocity,      ACE_PARAM_U32, ACE_PARAM_RW },
	{ PARAM_PROFILE_ACCELERATION,  &g_param.profile_acceleration,  ACE_PARAM_U32, ACE_PARAM_RW },
	{ PARAM_PROFILE_DECELERATION,  &g_param.profile_deceleration,  ACE_PARAM_U32, ACE_PARAM_RW },
    { PARAM_MICROSTEP,       	   &g_param.microstep,             ACE_PARAM_U32, ACE_PARAM_RW },

    { PARAM_LED_PA1,		 	   &g_param.led1,         		   ACE_PARAM_U8,  ACE_PARAM_RW },
	{ PARAM_RESET,		     	   &g_param.reset,         		   ACE_PARAM_U8,  ACE_PARAM_RW },
	{ PARAM_TEST,		     	   &g_param.test,         		   ACE_PARAM_U32, ACE_PARAM_RW },
	{ PARAM_ERROR_FLAG,		  	   &g_param.error_flag,            ACE_PARAM_U32, ACE_PARAM_RO },

    { PARAM_RAW_CH0,		 	   &g_param.fdc0_ch0,              ACE_PARAM_U32, ACE_PARAM_RO },
	{ PARAM_RAW_CH1,		  	   &g_param.fdc0_ch1,              ACE_PARAM_U32, ACE_PARAM_RO },
	{ PARAM_RAW_CH2,		 	   &g_param.fdc0_ch2,              ACE_PARAM_U32, ACE_PARAM_RO },
	{ PARAM_RAW_CH3,		  	   &g_param.fdc0_ch3,              ACE_PARAM_U32, ACE_PARAM_RO },

	{ PARAM_RAW_CH4,		 	   &g_param.fdc1_ch0,              ACE_PARAM_U32, ACE_PARAM_RO },
    { PARAM_RAW_CH5,		  	   &g_param.fdc1_ch1,              ACE_PARAM_U32, ACE_PARAM_RO },
    { PARAM_RAW_CH6,		  	   &g_param.fdc1_ch2,              ACE_PARAM_U32, ACE_PARAM_RO },
	{ PARAM_RAW_CH7,		 	   &g_param.fdc1_ch3,              ACE_PARAM_U32, ACE_PARAM_RO },

    { PARAM_RAW_CH8,		  	   &g_param.fdc2_ch0,              ACE_PARAM_U32, ACE_PARAM_RO },
    { PARAM_RAW_CH9,		  	   &g_param.fdc2_ch1,              ACE_PARAM_U32, ACE_PARAM_RO },
    { PARAM_RAW_CH10,		 	   &g_param.fdc2_ch2,              ACE_PARAM_U32, ACE_PARAM_RO },
    { PARAM_RAW_CH11,		  	   &g_param.fdc2_ch3,              ACE_PARAM_U32, ACE_PARAM_RO }

};



/*
 * Number of entries in the Ranger parameter dictionary.
 */
const uint16_t ranger_param_count =
    (uint16_t)(sizeof(ranger_param_table) /
               sizeof(ranger_param_table[0]));





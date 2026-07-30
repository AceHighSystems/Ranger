# Created on: May 10, 2026
#      Author: Tor Kaufmann Gjerde

# Ranger Mk1 module specific parameter IDs
# IDs holds ace protocol specific values or specific data values

# ===============================
#  System parameters (0x10–0x1F)
# ===============================
PARAM_NODE_ID       = 0x10
PARAM_STATE         = 0x12
PARAM_UPTIME        = 0x13
PARAM_RESET			= 0x14

# ===============================
#  Motion parameters (0x40–0x5F)
# ===============================
PARAM_STEP_ENABLE            = 0x40
PARAM_STEP_MOVE	             = 0x41
PARAM_TARGET_POSITION        = 0x42
PARAM_POSITION	             = 0x43
PARAM_ANGLE                  = 0x44

PARAM_PROFILE_VELOCITY       = 0x45
PARAM_PROFILE_ACCELERATION   = 0x46
ARAM_PROFILE_DECELERATION    = 0x47
PARAM_MICROSTEP	  		     = 0x48

# ===============================
#  Encoder parameters (0x60–0x7F)
# ===============================
PARAM_ENCODER_POSITION  = 0x60

# ===============================
#  Tememetry parameters (0x80–0x9F)
# ===============================
PARAM_VOLTAGE           = 0x80
PARAM_CURRENT           = 0x81
PARAM_TEMPERATURE       = 0x82


# ===============================
#  Diagnostics and prototype parameters (0xC0–0xDF)
# ===============================
PARAM_LED_PA1           = 0xC0
PARAM_TEST              = 0xC1
PARAM_ERROR_FLAG        = 0xC2
PARAM_STEP_ERROR        = 0xC3

PARAM_RAW_CH0       	= 0xD0
PARAM_RAW_CH1      		= 0xD1
PARAM_RAW_CH2      		= 0xD2
PARAM_RAW_CH3      		= 0xD3
PARAM_RAW_CH4      		= 0xD4
PARAM_RAW_CH5      		= 0xD5
PARAM_RAW_CH6      		= 0xD6
PARAM_RAW_CH7      		= 0xD7
PARAM_RAW_CH8      		= 0xD8
PARAM_RAW_CH9      		= 0xD9
PARAM_RAW_CH10      	= 0xDA
PARAM_RAW_CH11     		= 0xDB

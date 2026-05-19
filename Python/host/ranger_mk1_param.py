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
PARAM_STEP_ENABLE       = 0x40
PARAM_STEP_MICRO        = 0x41
PARAM_STEP_DIR          = 0x42
PARAM_STEP_FREQ         = 0x43

# ===============================
#  Encoder parameters (0x60–0x7F)
# ===============================
PARAM_ENCODER_POSITION  = 0x60

# ===============================
#  Tememetry parameters (0x80–0x9F)
# ===============================
PARAM_VOLTAGE           = 0x80

# ===============================
#  Diagnostics and prototype parameters (0xC0–0xDF)
# ===============================
PARAM_LED_PA1           = 0xC0

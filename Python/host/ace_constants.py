# Created on: May 6, 2026
#      Author: Tor Kaufmann Gjerde

# Ace Protocol specific constants
CANID_COMMAND_BASE              = 0x600
CANID_RESPONSE_BASE             = 0x580
CANID_HEARTBEAT_BASE            = 0x700

# Ace Protocol version -$$ Todo: Make compiled variable 
PROTOCOL_VERSION                = 0x01

# Ace protocol specific commands IDs - used in the first byte of the CAN 8-Byte data field / Ace protocol Command ID field
# currently either read or write are the valid operations in the protocol
CMD_READ                        = 0x01
CMD_WRITE                       = 0x02

# Following Parameter ID is specific for the bootloader of ace modules */
PARAM_BOOT			            = 0x30

# Ace protocol specific command IDs for Bootloader - used in the first byte of the CAN 8-Byte data field / Ace protocol Command ID field
CMD_BOOT_PING                   = 0x40
CMD_BOOT_START                  = 0x41
CMD_BOOT_DATA                   = 0x42
CMD_BOOT_END                    = 0x43
CMD_BOOT_RUN_APP                = 0x44

# Following STATUS codes are used for the Ace CAN response message frames */
STATUS_OK        		        = 0x10 
STATUS_QUEUED                   = 0x11 	
STATUS_DATA_FOLLOWS             = 0x12
STATUS_UNKNOWN_COMMAND          = 0x13
STATUS_UNKNOWN_PARAM            = 0x14

# Module state codes - states specific for AceHigh modules and present in Ace protocol files
# Response is present in CAN frame payload NOT Status field
STATE_STANDBY           		= 0x21
STATE_FAULT             		= 0x22
STATE_EXECUTING         		= 0x23
STATE_APP_VALID					= 0x24
STATE_BOOTLOADER_ACTIVE     	= 0x25
STATE_BOOTLOADER_INACTIVE   	= 0x26
STATE_FIRMWARE_SIZE_FAULT   	= 0x27
STATE_FIRMWARE_ERASE_FAULT  	= 0x28
STATE_FIRMWARE_SEQUENCE_FAULT  	= 0x29
STATE_FIRMWARE_WRITE_FAULT  	= 0x2A
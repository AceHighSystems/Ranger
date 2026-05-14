#!/usr/bin/env python3

import time
import threading
import can
import queue
import ace_constants as ACE
import ranger_mk1_param as RANGER
# -----------------------------
# Ace protocol / Ranger constants
# -----------------------------

# Current active Ranger node ID -ie. node ic value
NODE_ID = 0x02

# Ace protocol specific IDs - not added in CAN 8-byte data field but used in CAN arbitration field
ACE_CAN_ID_COMMAND   = 0x600 + NODE_ID   # host -> Ranger
ACE_CAN_ID_RESPONSE  = 0x580 + NODE_ID   # Ranger -> host
ACE_CAN_ID_HEARTBEAT = 0x700 + NODE_ID   # Ranger -> host

# Ace protocol specific commands IDs - used in the first byte of the CAN 8-Byte data field / Ace protocol Command ID field
# currently either read or write are the valid operations in the protocol
ACE_CMD_READ         = 0x01
ACE_CMD_WRITE        = 0x02

# Ace protocol specific command IDs for Bootloader - used in the first byte of the CAN 8-Byte data field / Ace protocol Command ID field
ACE_CMD_BOOT_PING    = 0x40
ACE_CMD_BOOT_START   = 0x41
ACE_CMD_BOOT_DATA    = 0x42
ACE_CMD_BOOT_END     = 0x43
ACE_CMD_BOOT_RUN_APP = 0x44

# The parameter ID links to a specific value or system / module variable 
ACE_PARAM_BOOT                = 0x30   # Parameter ID specific for the booatloader of AceHigh modules
RANGER_PARAM_LED_PA1          = 0xC0   # parameter ID for LED variable
RANGER_PARAM_NODE_ID          = 0x10   # parameter ID for Node variable

# Status codes - specific for Ace protocol
# Valid status codes for response frame sent after reception of a command frame from host
ACE_STATUS_OK           = 0x10
ACE_STATUS_QUEUED       = 0x11
ACE_STATUS_DATA_FOLLOWS = 0x12
ACE_STATUS_UNKNOWN_COMMAND  = 0x13
ACE_STATUS_UNKNOWN_PARAM    = 0x14

rx_queue = queue.Queue()

CHANNEL = "/dev/cu.usbmodem2080317458421"
BITRATE = 1000000

print_lock = threading.Lock()
running = True

def safe_print(*args, **kwargs):
    with print_lock:
        print(*args, **kwargs)


def status_to_string(status: int) -> str:
    lookup = {
        ACE_STATUS_OK: "Accepted, status OK",
        ACE_STATUS_QUEUED: "Accepted, queued",
        ACE_STATUS_DATA_FOLLOWS: "Accepted, data follows",
        ACE_STATUS_UNKNOWN_COMMAND: "Unknown command",
        ACE_STATUS_UNKNOWN_PARAM: "Invalid parameter",
    }
    return lookup.get(status, f"Unknown status 0x{status:02X}")


def update_can_ids() -> None:
    global ACE_CAN_ID_COMMAND, ACE_CAN_ID_RESPONSE, ACE_CAN_ID_HEARTBEAT

    ACE_CAN_ID_COMMAND   = 0x600 + NODE_ID
    ACE_CAN_ID_RESPONSE  = 0x580 + NODE_ID
    ACE_CAN_ID_HEARTBEAT = 0x700 + NODE_ID


# START build custom commands -----------------------------------------------------------------------------------------

def build_reset() -> can.Message:
    data = [ACE_CMD_WRITE, ACE_PARAM_BOOT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

def build_read_led() -> can.Message:
    data = [ACE_CMD_READ, RANGER_PARAM_LED_PA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

def build_write_led(state: int) -> can.Message:
    data = [
        ACE_CMD_WRITE,
        RANGER_PARAM_LED_PA1,
        0x01 if state else 0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    ]
    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

def build_write_node_id(new_node_id: int) -> can.Message:
    data = [
        ACE_CMD_WRITE,
        RANGER_PARAM_NODE_ID,
        new_node_id & 0xFF, # masks the new_node_id leaving only the lowest 8 bits. (Only 11-bits available in std. CAN frame)
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    ]
    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

def build_read_node_id() -> can.Message:
    data = [ACE_CMD_READ, RANGER_PARAM_NODE_ID, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

# END build custom commands --------------------------------------------------------------------------------

def print_message(prefix: str, msg: can.Message) -> None:
    hex_data = " ".join(f"{b:02X}" for b in msg.data)
    safe_print(f"{prefix} ID=0x{msg.arbitration_id:03X} DLC={msg.dlc} DATA=[{hex_data}]")


def decode_response(msg: can.Message) -> None:
    if len(msg.data) < 8:
        safe_print("Response too short")
        return

    command_id   = msg.data[0]
    parameter_id = msg.data[1]
    status_code  = msg.data[2]
    payload      = msg.data[3:8]

    if (command_id != ACE_CMD_BOOT_DATA):
        print_message("Response", msg)
        safe_print(f"  command_id   = 0x{command_id:02X}")
        safe_print(f"  parameter_id = 0x{parameter_id:02X}")
        safe_print(f"  status_code  = 0x{status_code:02X} ({status_to_string(status_code)})")
        safe_print(f"  payload      = {[f'0x{x:02X}' for x in payload]}")

    if command_id == ACE_CMD_READ and status_code == ACE_STATUS_DATA_FOLLOWS and parameter_id == RANGER_PARAM_LED_PA1:
        led_state = payload[0]
        safe_print(f" decoded LED state = {'OFF' if led_state else 'ON'}")

    if command_id == ACE_CMD_READ and status_code == ACE_STATUS_DATA_FOLLOWS and parameter_id == RANGER_PARAM_NODE_ID:
        node_id = payload[0]
        safe_print(f" decoded Node ID = 0x{node_id:02X} ({node_id})")

    if command_id == ACE_CMD_WRITE and parameter_id == RANGER_PARAM_NODE_ID:
        if status_code == ACE_STATUS_OK:
            safe_print("  Node ID write accepted")
        else:
            safe_print("  Node ID write failed")


def listener_thread(bus: can.Bus) -> None:
    global running

    safe_print("Background listener started...")
    while running:
        try:
            msg = bus.recv(timeout=1.0)
            if msg is None:
                continue

            if msg.arbitration_id == ACE_CAN_ID_HEARTBEAT:
                print_message("Heartbeat", msg)
            elif msg.arbitration_id == ACE_CAN_ID_RESPONSE:
                decode_response(msg)
                rx_queue.put(msg) # Only put ACE_CAN_ID_RESPONSE messages into the queue
            else:
                print_message("Other", msg)
            
        except Exception as exc:
            safe_print(f"Listener error: {exc}")
            break

    safe_print("Background listener stopped.")


def wait_for_response(timeout=1.0):

    try:
        msg = rx_queue.get(timeout=timeout)
        return msg

    except queue.Empty:
        return None


def send_command(bus: can.Bus, msg: can.Message, label: str) -> None:
    try:
        safe_print(f"\n---- {label} ----")
        print_message("TX", msg)
        bus.send(msg)
    except Exception as exc:
        safe_print(f"Send error: {exc}")


def print_menu() -> None:
    safe_print("\nCommands:")
    safe_print("  1  -> WRITE LED ON")
    safe_print("  2  -> WRITE LED OFF")
    safe_print("  3  -> READ LED STATE")
    safe_print("  4  -> WRITE LED ON, then READ")
    safe_print("  5  -> WRITE LED OFF, then READ")
    safe_print("  6  -> READ NODE ID")
    safe_print("  7  -> WRITE NODE ID")
    safe_print("  b  -> SEND BOOTLOADER PING")
    safe_print("  p  -> Program Firmware")
    safe_print("  m  -> show menu")
    safe_print("  q  -> quit\n")

# START Program new Firmware  --------------------------------------------------------------------------------

def program_new_firmware(bus: can.Bus):

    # safe_print("Starting firmware update...")
    
    # data = [ACE.CMD_WRITE, RANGER.PARAM_RESET, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    # can_message = can.Message(
    #     arbitration_id=ACE_CAN_ID_COMMAND,
    #     is_extended_id=False,
    #     data=data
    # )

    # bus.send(can_message)
    # response_msg = wait_for_response(timeout=5.0)
   
    # if response_msg is None:
    #     safe_print("Module could not be reset")
    #     return
    
    # if(response_msg.data[1] != RANGER.PARAM_RESET):
    #     safe_print("Reset parameter ID not received by module")

    # if(response_msg.data[2] == ACE.STATUS_OK):
    #     safe_print("Status ok, resetting module")

    # time.sleep(0.2) # Let the module reset before continuing

    firmware_path = "/Users/tor/Documents/Ranger/Ranger_module_firmware/ranger_mk1_v2/Debug/ranger_mk1_v2.bin"

    with open(firmware_path, "rb") as f:
        firmware = f.read()

    fw_size = len(firmware)
    safe_print(f"Firmware file: {firmware_path}")
    safe_print(f"Firmware size: {fw_size} bytes / 0x{fw_size:08X}")

    # _BOOT_PING command

    data = [ACE_CMD_BOOT_PING, ACE_PARAM_BOOT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )
    bus.send(can_message)
    response_msg = wait_for_response(timeout=5.0)

    # Print received frame with Ace protocol version and module bootloader version before continuing
    
    # _BOOT_START command - sends command to module, module erases flash and makes ready for new firmware transfer
    data = [ACE_CMD_BOOT_START,
        ACE_PARAM_BOOT,
        0x00,
        fw_size & 0xFF,
        (fw_size >> 8) & 0xFF,
        (fw_size >> 16) & 0xFF,
        (fw_size >> 24) & 0xFF,
        0x00]

    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )
    bus.send(can_message)
    response_msg = wait_for_response(timeout=10.0)

    if response_msg is None:
        safe_print("BOOT_DATA timeout")
        return

    if(response_msg.data[3] != ACE.STATE_BOOTLOADER_ACTIVE):
        # The bootloader did not return ACE_STATE_BOOTLOADER_ACTIVE so a fault occoured
        safe_print("BOOT_DATA_START did not complete: Bootloader not active: ", response_msg)

    if(response_msg.data[3] == ACE.STATE_FIRMWARE_ERASE_FAULT):
        safe_print("Module flash erase fault: ", response_msg)
        return                     

    if(response_msg.data[3] == ACE.STATE_FIRMWARE_SIZE_FAULT):
        safe_print("Firmware size miss-match: ", response_msg)
        return   

    # _BOOT_DATA
    # Transfer firmware in chunks of 4 bytes per CAN frame
    # Increment sequence counter and send as two bytes LSB and MSB 

    BYTES_PER_LINE = 4
    seq_counter = 0
    print("Progress: ", end="", flush=True)
    for offset in range(0, len(firmware), BYTES_PER_LINE):
        fw_array = firmware[offset:offset + BYTES_PER_LINE]

        seq_LSB = seq_counter & 0xFF
        seq_MSB = (seq_counter >> 8) & 0xFF

        while len(fw_array) < 4:  # pad the frame if bytes are less than 4
            fw_array += b"\xFF"   # Flash erased state padding
        
        data = [ACE_CMD_BOOT_DATA, ACE_PARAM_BOOT, seq_LSB, seq_MSB, fw_array[0], fw_array[1], fw_array[2], fw_array[3]]
        
        #safe_print("Transferring data")
        #safe_print(f"TX: {' '.join(f'{b:02X}' for b in data)}")

        can_message = can.Message(
             arbitration_id=ACE_CAN_ID_COMMAND,
             is_extended_id=False,
             data=data
         )
        bus.send(can_message)

        response_msg = wait_for_response(timeout=10.0)
        
        if response_msg is None:
            safe_print("BOOT_DATA timeout")
            return

        if(response_msg.data[3] != ACE.STATE_BOOTLOADER_ACTIVE):
            # The bootloader did not return ACE_STATE_BOOTLOADER_ACTIVE so a fault occoured
            safe_print("BOOT_DATA did not complete correctly: Bootloader not active: ", response_msg)
            return

        if(response_msg.data[3] == ACE.STATE_FIRMWARE_SEQUENCE_FAULT):
            safe_print("CAN frame firmware sequence error: ", response_msg)
            return

        if(response_msg.data[3] == ACE.STATE_FIRMWARE_ERASE_FAULT):
            safe_print("Module flash erase fault: ", response_msg)
            return            

        if(response_msg.data[3] == ACE.STATE_FIRMWARE_WRITE_FAULT):
            safe_print("Module flash write fault: ", response_msg)
            return            

        if(response_msg.data[3] == ACE.STATE_FIRMWARE_SIZE_FAULT):
            safe_print("Firmware size miss-match: ", response_msg)
            return   

        seq_counter = seq_counter + 1 # increment sequence counter for next CAN frame 
        
        if (seq_counter % 100 == 0): # a little GUI loading firmware bar
            safe_print("/", end="", flush=True)
    
    safe_print(" ") #newline for following peints    
        
    time.sleep(0.1)

    # _BOOT_END
    # Signal end the Firmware transfer from the host (module should jump to the application automatically)
    data = [ACE_CMD_BOOT_END, ACE_PARAM_BOOT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )
    bus.send(can_message)
    response_msg = wait_for_response(timeout=10.0)

    if response_msg is None:
        safe_print("BOOT_END timeout")
        return

    if(response_msg.data[3] == ACE.STATE_APP_VALID):
        # Application was marked as valid by the MCU
        safe_print("Transfer complete, jumping to application")



# END Program new Firmware --------------------------------------------------------------------------------

# Main loop definition STARTS here -------------------------------------------------------------------------------------

def main() -> None:
    global running
    global NODE_ID

    safe_print("Opening CAN bus...")
    with can.Bus(interface="slcan", channel=CHANNEL, bitrate=BITRATE) as bus:
        safe_print("Connected.")

        t = threading.Thread(target=listener_thread, args=(bus,), daemon=True)
        t.start()

        print_menu()

        try:
            while True:
                cmd = input("> ").strip().lower()

                if cmd == "1":
                    send_command(bus, build_write_led(0), "WRITE LED ON")

                elif cmd == "2":
                    send_command(bus, build_write_led(1), "WRITE LED OFF")

                elif cmd == "3":
                    send_command(bus, build_read_led(), "READ LED STATE")

                elif cmd == "4":
                    send_command(bus, build_write_led(0), "WRITE LED ON")
                    time.sleep(0.1)
                    send_command(bus, build_read_led(), "READ LED STATE")

                elif cmd == "5":
                    send_command(bus, build_write_led(1), "WRITE LED OFF")
                    time.sleep(0.1)
                    send_command(bus, build_read_led(), "READ LED STATE")

                elif cmd == "6":
                    send_command(bus, build_read_node_id(), "READ NODE ID")

                elif cmd == "7":
                    try:
                        new_id = int(input("Enter new Node ID (0-127): ").strip(), 0)

                        if not (0 <= new_id <= 127):
                            safe_print("Invalid Node ID range")
                            continue

                        send_command(
                            bus,
                            build_write_node_id(new_id),
                            f"WRITE NODE ID -> {new_id}"
                        )

                        time.sleep(0.2)

                        NODE_ID = new_id
                        update_can_ids()

                        safe_print(f"Switched host to new Node ID: {NODE_ID}")

                    except ValueError:
                        safe_print("Invalid input")

                elif cmd == "p":
                    program_new_firmware(bus);

                elif cmd == "r":
                    send_command(bus, build_reset(), "Software reset module")

                elif cmd == "m":
                    print_menu()

                elif cmd == "q":
                    safe_print("Quitting...")
                    break

                elif cmd == "":
                    continue

                else:
                    safe_print("Unknown command. Press 'm' for menu.")

        except KeyboardInterrupt:
            safe_print("\nKeyboard interrupt received. Exiting...")

        running = False
        time.sleep(0.2)


if __name__ == "__main__":
    main()
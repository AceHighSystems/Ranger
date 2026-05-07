#!/usr/bin/env python3

import time
import threading
import can
import argparse

# -----------------------------
# Ace protocol / Ranger constants
# -----------------------------

# Current active Ranger node ID -ie. node ic value
NODE_ID = 0x02

# Ace protocol specific IDs - not added in CAN 8-byte data field but used in CAN arbitration field
ACE_CAN_ID_COMMAND   = 0x600 + NODE_ID   # host -> Ranger
ACE_CAN_ID_RESPONSE  = 0x580 + NODE_ID   # Ranger -> host
ACE_CAN_ID_HEARTBEAT = 0x700 + NODE_ID   # Ranger -> host

#Ace protocol specific commands IDs - used in the first byte of the CAN 8-Byte data field / Ace protocol Command ID field
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
STATUS_OK              = 0x10
STATUS_QUEUED          = 0x11
STATUS_DATA_FOLLOWS    = 0x12
STATUS_UNKNOWN_COMMAND = 0x13
STATUS_INVALID_PARAM   = 0x14

# Module state codes - states specific for AceHigh modules 
ACE_STATE_STANDBY      = 0x01  # Module is idle not executing any functions, ready for commands
ACE_STATE_FAULT        = 0x02  # Module is in fault state
ACE_STATE_EXECUTING    = 0x03  # Module is executing functions, but can receive new commands
ACE_STATE_BOOTLOADER   = 0x04  # Module is in bootloader mode

CHANNEL = "/dev/cu.usbmodem2080317458421"
BITRATE = 1000000

print_lock = threading.Lock()
running = True

def safe_print(*args, **kwargs):
    with print_lock:
        print(*args, **kwargs)


def status_to_string(status: int) -> str:
    lookup = {
        STATUS_OK: "Accepted, status OK",
        STATUS_QUEUED: "Accepted, queued",
        STATUS_DATA_FOLLOWS: "Accepted, data follows",
        STATUS_UNKNOWN_COMMAND: "Unknown command",
        STATUS_INVALID_PARAM: "Invalid parameter",
    }
    return lookup.get(status, f"Unknown status 0x{status:02X}")


def update_can_ids() -> None:
    global ACE_CAN_ID_COMMAND, ACE_CAN_ID_RESPONSE, ACE_CAN_ID_HEARTBEAT

    ACE_CAN_ID_COMMAND   = 0x600 + NODE_ID
    ACE_CAN_ID_RESPONSE  = 0x580 + NODE_ID
    ACE_CAN_ID_HEARTBEAT = 0x700 + NODE_ID


# START build custom commands -----------------------------------------------------------------------------------------

# BOOTLOADER commands START: ------------------------------------------------------------------------------------------
#   1. Send BOOT_PING
#   2. Check response - error handling
#   3. Send BOOT_START , size = 12 for test
#   4. Check payload[0] == 1
#   5. send BOOT_DATA seq 0, bytes 0x11 0x22 0x33 0x44
#   6. send BOOT_DATA seq 1, bytes 0x55 0x66 0x77 0x88
#   7. send BOOT_DATA seq 2, bytes 0x99 0xAA 0xBB 0xCC
#   8. send BOOT_END
#   9. Expect jump to application


# BOOTLOADER commands END: ------------------------------------------------------------------------------------------

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

    print_message("Response", msg)
    safe_print(f"  command_id   = 0x{command_id:02X}")
    safe_print(f"  parameter_id = 0x{parameter_id:02X}")
    safe_print(f"  status_code  = 0x{status_code:02X} ({status_to_string(status_code)})")
    safe_print(f"  payload      = {[f'0x{x:02X}' for x in payload]}")

    if command_id == ACE_CMD_READ and status_code == STATUS_DATA_FOLLOWS and parameter_id == RANGER_PARAM_LED_PA1:
        led_state = payload[0]
        safe_print(f"  decoded LED state = {'OFF' if led_state else 'ON'}")

    if command_id == ACE_CMD_READ and status_code == STATUS_DATA_FOLLOWS and parameter_id == RANGER_PARAM_NODE_ID:
        node_id = payload[0]
        safe_print(f"  decoded Node ID = 0x{node_id:02X} ({node_id})")

    if command_id == ACE_CMD_WRITE and parameter_id == RANGER_PARAM_NODE_ID:
        if status_code == STATUS_OK:
            safe_print("  Node ID write accepted")
        else:
            safe_print("  Node ID write failed")
    
    if command_id == ACE_CMD_BOOT_PING and parameter_id == ACE_PARAM_BOOT:
        safe_print("  BOOTPING reply received")

        protocol_version = payload[0]
        bootloader_version = payload[1]
        module_state = payload[2]

        safe_print(f"  Protocol version = 0x{protocol_version:02X}")
        safe_print(f"  Bootloader version = 0x{bootloader_version:02X}")
        safe_print(f"  Module state = 0x{module_state:02X}")
        if status_code == STATUS_OK:
            safe_print("  Module alive")
        if module_state == ACE_STATE_BOOTLOADER:
            safe_print("  Module is in BOOTLOAD loop")
    
    if command_id == ACE_CMD_BOOT_DATA and parameter_id == ACE_PARAM_BOOT:
        variable = 0 # Do nothing


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
            else:
                print_message("Other", msg)

        except Exception as exc:
            safe_print(f"Listener error: {exc}")
            break

    safe_print("Background listener stopped.")


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


def program_new_firmware(bus: can.Bus):

    safe_print("Starting firmware update...")

    # Load firmware .bin file 
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", help="Path to firmware .bin file")
    args = parser.parse_args()

    with open(args.firmware, "rb") as f:
        firmware = f.read()

    # Send _BOOT_PING command - for now CAN listener is running in the background
    data = [ACE_CMD_BOOT_PING, ACE_PARAM_BOOT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )
    bus.send(can_message)
    time.sleep(0.1)

    # Send _BOOT_START command
    fw_size = len(firmware)

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
    time.sleep(6)


    # _BOOT_DATA
    # Transfer firmware in chunks of 4 bytes per CAN frame
    # Increment sequence counter and send as two bytes LSB and MSB 

    BYTES_PER_LINE = 4
    seq_counter = 0

    for offset in range(0, len(firmware), BYTES_PER_LINE):
        fw_array = firmware[offset:offset + BYTES_PER_LINE]

        safe_print("Transferring data")
    
        seq_LSB = seq_counter & 0xFF
        seq_MSB = (seq_counter >> 8) & 0xFF

        while len(fw_array) < 4: # pad the frame if bytes are less than 4
            fw_array += b"\xFF"   # Flash erased state padding

        
        data = [ACE_CMD_BOOT_DATA, ACE_PARAM_BOOT, seq_LSB, seq_MSB, fw_array[0], fw_array[1], fw_array[2], fw_array[3]]
        can_message = can.Message(
             arbitration_id=ACE_CAN_ID_COMMAND,
             is_extended_id=False,
             data=data
         )
        bus.send(can_message)
        seq_counter = seq_counter + 1
        print(f"TX: {' '.join(f'{b:02X}' for b in data)}")
        time.sleep(0.01)

    # _BOOT_END
    # Signal end the Firmware transfer from the host (module should jump to the application automatically)
    data = [ACE_CMD_BOOT_END, ACE_PARAM_BOOT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )
    bus.send(can_message)

    safe_print("Transfer complete!")
    safe_print("Jumping to application")

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

                elif cmd == "b":
                    send_command(bus, build_boot_ping(), "SEND BOOTLOADER PING")

                elif cmd == "p":
                    program_new_firmware(bus);

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
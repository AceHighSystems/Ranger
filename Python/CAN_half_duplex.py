

# Author: Tor Kaufmann Gjerde 
#!/usr/bin/env python3

import time
import can

# -----------------------------
# ACE Light Protocol / Ranger constants
# -----------------------------
NODE_ID = 0x01

CAN_ID_COMMAND  = 0x600 + NODE_ID   # host -> Ranger
CAN_ID_RESPONSE = 0x580 + NODE_ID   # Ranger -> host
CAN_ID_HEARTBEAT = 0x700 + NODE_ID  # Ranger -> host

CMD_WRITE = 0x01
CMD_READ  = 0x02

PARAM_LED_PA1 = 0x01

STATUS_EXECUTING       = 0x00
STATUS_QUEUED          = 0x01
STATUS_DATA_FOLLOWS    = 0x02
STATUS_UNKNOWN_COMMAND = 0x10
STATUS_INVALID_PARAM   = 0x11


def status_to_string(status: int) -> str:
    lookup = {
        STATUS_EXECUTING: "Accepted, executing",
        STATUS_QUEUED: "Accepted, queued",
        STATUS_DATA_FOLLOWS: "Accepted, data follows",
        STATUS_UNKNOWN_COMMAND: "Unknown command",
        STATUS_INVALID_PARAM: "Invalid parameter",
    }
    return lookup.get(status, f"Unknown status 0x{status:02X}")


def build_write_led(state: int) -> can.Message:
    """
    Command frame layout, per page 16:
      Byte 0: command_id
      Byte 1: parameter_id
      Byte 2..7: payload
    """
    data = [
        CMD_WRITE,      # Byte 0
        PARAM_LED_PA1,  # Byte 1
        0x01 if state else 0x00,  # Byte 2 payload0
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    ]
    return can.Message(
        arbitration_id=CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )


def build_read_led() -> can.Message:
    data = [
        CMD_READ,       # Byte 0
        PARAM_LED_PA1,  # Byte 1
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    ]
    return can.Message(
        arbitration_id=CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )


def print_message(prefix: str, msg: can.Message) -> None:
    hex_data = " ".join(f"{b:02X}" for b in msg.data)
    print(f"{prefix} ID=0x{msg.arbitration_id:03X} DLC={msg.dlc} DATA=[{hex_data}]")


def wait_for_response(bus: can.Bus, timeout: float = 5.0):
    """
    Wait for a response frame from this node.
    Ignores heartbeats and unrelated traffic.
    """
    deadline = time.time() + timeout
    while time.time() < deadline:
        msg = bus.recv(timeout=0.1)
        if msg is None:
            continue

        if msg.arbitration_id == CAN_ID_RESPONSE:
            return msg

        if msg.arbitration_id == CAN_ID_HEARTBEAT:
            print_message("Heartbeat", msg)
            continue

        print_message("Other", msg)

    return None


def decode_response(msg: can.Message) -> None:
    """
    Response frame layout from page 17:
      Byte 0: command_id
      Byte 1: status_code
      Byte 2: parameter_id
      Byte 3..7: payload
    """
    if len(msg.data) < 8:
        print("Response too short")
        return

    command_id   = msg.data[0]
    status_code  = msg.data[1]
    parameter_id = msg.data[2]
    payload      = msg.data[3:8]

    print_message("Response", msg)
    print(f"  command_id   = 0x{command_id:02X}")
    print(f"  status_code  = 0x{status_code:02X} ({status_to_string(status_code)})")
    print(f"  parameter_id = 0x{parameter_id:02X}")
    print(f"  payload      = {[f'0x{x:02X}' for x in payload]}")

    if command_id == CMD_READ and status_code == STATUS_DATA_FOLLOWS and parameter_id == PARAM_LED_PA1:
        led_state = payload[0]
        print(f"  decoded LED state = {'ON' if led_state else 'OFF'}")


def send_and_wait(bus: can.Bus, msg: can.Message, timeout: float = 1.0) -> None:
    print_message("TX", msg)
    bus.send(msg)
    rsp = wait_for_response(bus, timeout=timeout)
    if rsp is None:
        print("No response received")
        return
    decode_response(rsp)


def main() -> None:
    # Adjust channel to your actual CANable/slcan port
    channel = "/dev/cu.usbmodem2080317458421"

    print("Opening CAN bus...")
    with can.Bus(interface="slcan", channel=channel, bitrate=1000000) as bus:
        print("Connected.")
        print()

        # 1. Turn LED ON
        print("---- WRITE LED ON ----")
        send_and_wait(bus, build_write_led(1))
        print()

        time.sleep(0.2)

        # 2. Read LED state
        print("---- READ LED STATE ----")
        send_and_wait(bus, build_read_led())
        print()

        time.sleep(0.2)

        # 3. Turn LED OFF
        print("---- WRITE LED OFF ----")
        send_and_wait(bus, build_write_led(0))
        print()

        time.sleep(0.2)

        # 4. Read LED state again
        print("---- READ LED STATE ----")
        send_and_wait(bus, build_read_led())
        print()


if __name__ == "__main__":
    main()
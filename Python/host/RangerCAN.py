#!/usr/bin/env python3

import time
import threading
import can
import queue
from collections import deque
from matplotlib.ticker import AutoMinorLocator
import matplotlib.pyplot as plt
from PyQt6 import QtWidgets, QtCore
import pyqtgraph as pg
import sys
import ace_constants as ACE
import ranger_mk1_param as RANGER

# -----------------------------
# Debugging
# -----------------------------

PRINT_CAN_RESPONSES = False

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
ACE_STATUS_OK               = 0x10
ACE_STATUS_QUEUED           = 0x11
ACE_STATUS_DATA_FOLLOWS     = 0x12
ACE_STATUS_UNKNOWN_COMMAND  = 0x13
ACE_STATUS_UNKNOWN_PARAM    = 0x14

PARAMETERS = {
    "reset":        RANGER.PARAM_RESET,
    "step_enable":  RANGER.PARAM_STEP_ENABLE,
    "step_move":         RANGER.PARAM_STEP_MOVE,
    "target":       RANGER.PARAM_TARGET_POSITION,
    "velocity":     RANGER.PARAM_PROFILE_VELOCITY,
    "led":          RANGER.PARAM_LED_PA1,
    "voltage":      RANGER.PARAM_VOLTAGE,
    "current":      RANGER.PARAM_CURRENT,
    "temp":         RANGER.PARAM_TEMPERATURE,
    "test":         RANGER.PARAM_TEST,
    "error":        RANGER.PARAM_ERROR_FLAG,
    "ch0":          RANGER.PARAM_RAW_CH0,
    "ch1":          RANGER.PARAM_RAW_CH1,
    "ch2":          RANGER.PARAM_RAW_CH2,
    "ch3":          RANGER.PARAM_RAW_CH3,
    "ch4":          RANGER.PARAM_RAW_CH4,
    "ch5":          RANGER.PARAM_RAW_CH5,
    "ch6":          RANGER.PARAM_RAW_CH6,
    "ch7":          RANGER.PARAM_RAW_CH7,
    "ch8":          RANGER.PARAM_RAW_CH8,
    "ch9":          RANGER.PARAM_RAW_CH9,
    "ch10":         RANGER.PARAM_RAW_CH10,
    "ch11":         RANGER.PARAM_RAW_CH11
}


rx_queue = queue.Queue()

CHANNEL = "/dev/cu.usbmodem2080317458421"
# Windows use CHANNEL = "COM7"
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


# START build custom commands and helpers -----------------------------------------------------------------------------------------
def payload_to_i32(payload):
    value = (
        payload[0]
        | (payload[1] << 8)
        | (payload[2] << 16)
        | (payload[3] << 24)
    )

    if value & 0x80000000:
        value -= 0x100000000

    return value


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

def build_read(parameter_name) -> can.Message:
    if parameter_name not in PARAMETERS:
        print(f"Unknown parameter: {parameter_name}")
        return

    parameter_id = PARAMETERS[parameter_name]

    data = [
        ACE.CMD_READ,
        parameter_id,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    ]

    return can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )


def build_write(parameter_name, value) -> can.Message:
    if parameter_name not in PARAMETERS:
        print(f"Unknown parameter: {parameter_name}")
        return

    parameter_id = PARAMETERS[parameter_name]

    data = [
        ACE.CMD_WRITE,
        parameter_id,
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF,
        0x00,
        0x00
    ]

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

    if (PRINT_CAN_RESPONSES and command_id != ACE_CMD_BOOT_DATA):
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

    if command_id == ACE_CMD_READ and status_code == ACE_STATUS_DATA_FOLLOWS and parameter_id == RANGER.PARAM_VOLTAGE:
        voltage = payload_to_i32(payload)/1000
        safe_print(f"Voltage = {voltage:.3f} V")
    
    if command_id == ACE_CMD_READ and status_code == ACE_STATUS_DATA_FOLLOWS and parameter_id == RANGER.PARAM_CURRENT:
        current = payload_to_i32(payload)/1000
        safe_print(f"Current = {current:.3f} A")

    if command_id == ACE_CMD_READ and status_code == ACE_STATUS_DATA_FOLLOWS and parameter_id == RANGER.PARAM_TEMPERATURE:
        temp = payload_to_i32(payload)/1000
        safe_print(f"Temperature = {temp:.3f} °C")   

 
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
            #else:
               # print_message("Other", msg)
            
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
    #try:
        #safe_print(f"\n---- {label} ----")
        #print_message("TX", msg)
    bus.send(msg)
    #except Exception as exc:
        #safe_print(f"Send error: {exc}")


def print_menu() -> None:
    safe_print("\nCommands:")
    safe_print("  1  -> WRITE LED ON")
    safe_print("  2  -> WRITE LED OFF")
    safe_print("  3  -> READ LED STATE")
    safe_print("  4  -> WRITE LED ON, then READ")
    safe_print("  5  -> WRITE LED OFF, then READ")
    safe_print("  6  -> READ NODE ID")
    safe_print("  7  -> WRITE NODE ID")
    safe_print("  8  -> Read parameter test")
    safe_print("  9  -> Write parameter test")
    safe_print("  b  -> SEND BOOTLOADER PING")
    safe_print("  p  -> Program Firmware")
    safe_print("  m  -> show menu")
    safe_print("  q  -> quit\n")

# START Program new Firmware  --------------------------------------------------------------------------------

def program_new_firmware(bus: can.Bus):

    safe_print("Starting firmware update...")
    
    data = [ACE.CMD_WRITE, RANGER.PARAM_RESET, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00]
    
    can_message = can.Message(
        arbitration_id=ACE_CAN_ID_COMMAND,
        is_extended_id=False,
        data=data
    )

    bus.send(can_message)
    #response_msg = wait_for_response(timeout=10.0)
    time.sleep(0.2) # let the module reset before continuing
    #if response_msg is None:
     #   safe_print("Module could not be reset")
      #  return
    
    #if(response_msg.data[1] != RANGER.PARAM_RESET):
   #      safe_print("Reset parameter ID not received by module")

  #  if(response_msg.data[2] == ACE.STATUS_OK):
   #      safe_print("Status ok, resetting module")

    # Mac use: firmware_path = "/Users/tor/Documents/Ranger/Ranger_module_firmware/ranger_mk1_v2/Debug/ranger_mk1_v2.bin"

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



# START PLOTTING functions --------------------------------------------------------------------------------
PLOT_LEN = 1000
UPDATE_MS = 10      # 50 Hz GUI refresh
CAN_POLL_MS = 1     # fast polling


def decode_u32_le(payload):
    return (
        payload[0]
        | (payload[1] << 8)
        | (payload[2] << 16)
        | (payload[3] << 24)
    )


def plot_channels_live_pyqtgraph(bus):
    channels = [
        ("ch0", RANGER.PARAM_RAW_CH0, 0),
        ("ch1", RANGER.PARAM_RAW_CH1, 1),
        ("ch2", RANGER.PARAM_RAW_CH2, 2),
        ("ch3", RANGER.PARAM_RAW_CH3, 3),
        ("ch4", RANGER.PARAM_RAW_CH4, 4),
        ("ch5", RANGER.PARAM_RAW_CH5, 5),
        ("ch6", RANGER.PARAM_RAW_CH6, 6),
        ("ch7", RANGER.PARAM_RAW_CH7, 7),
        ("ch8", RANGER.PARAM_RAW_CH8, 8),
        ("ch9", RANGER.PARAM_RAW_CH9, 9),
        ("ch10", RANGER.PARAM_RAW_CH10, 10),
        ("ch11", RANGER.PARAM_RAW_CH11, 11),
    ]

    channel_data = [
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
        deque(maxlen=PLOT_LEN),
    ]

    app = QtWidgets.QApplication.instance()
    if app is None:
        app = QtWidgets.QApplication(sys.argv)

    win = pg.GraphicsLayoutWidget(title="Ranger FDC Live Plot")
    win.resize(1000, 700)
    win.show()

    plots = []
    curves = []

    for i, (name, _, _) in enumerate(channels):
        plot = win.addPlot(row=i, col=0, title=name)
        plot.showGrid(x=True, y=True)
        plot.setLabel("left", "Raw value")
        plot.setLabel("bottom", "Sample")

        curve = plot.plot(
            pen=pg.mkPen(color="#52F7AD", width=2),
            name=name
        )

        plots.append(plot)
        curves.append(curve)

    def poll_can():
        for ch_name, expected_param, plot_index in channels:
            send_command(
                bus,
                build_read(ch_name),
                f"READ {ch_name}"
            )

            msg = wait_for_response(timeout=0.1)

            if msg is None:
                continue

            command_id   = msg.data[0]
            parameter_id = msg.data[1]
            status_code  = msg.data[2]
            payload      = msg.data[3:8]

            if (
                command_id == ACE_CMD_READ and
                status_code == ACE_STATUS_DATA_FOLLOWS and
                parameter_id == expected_param
            ):
                value = decode_u32_le(payload)
                channel_data[plot_index].append(value)

    def update_plot():
        for i in range(12):
            data = list(channel_data[i])

            if not data:
                continue

            curves[i].setData(data)

    can_timer = QtCore.QTimer()
    can_timer.timeout.connect(poll_can)
    can_timer.start(CAN_POLL_MS)

    plot_timer = QtCore.QTimer()
    plot_timer.timeout.connect(update_plot)
    plot_timer.start(UPDATE_MS)

    try:
        app.exec()
    except KeyboardInterrupt:
        safe_print("Stopping live plot...")
        win.close()



def plot_channels_live(bus):
    channels = [
        ("ch0", RANGER.PARAM_RAW_CH0, 0),
        ("ch1", RANGER.PARAM_RAW_CH1, 1),
        ("ch2", RANGER.PARAM_RAW_CH2, 2),
        ("ch3", RANGER.PARAM_RAW_CH3, 3),
        ("ch4", RANGER.PARAM_RAW_CH4, 4),
        ("ch5", RANGER.PARAM_RAW_CH5, 5),
        ("ch6", RANGER.PARAM_RAW_CH6, 6),
        ("ch7", RANGER.PARAM_RAW_CH7, 7),
        ("ch8", RANGER.PARAM_RAW_CH8, 8),
        ("ch9", RANGER.PARAM_RAW_CH9, 9),
        ("ch10", RANGER.PARAM_RAW_CH10, 10),
        ("ch11", RANGER.PARAM_RAW_CH11, 11),
    ]

    plt.ion()

    last_plot_update = 0

    try:
        while True:
            for ch_name, expected_param, plot_index in channels:

                send_command(
                    bus,
                    build_read(ch_name),
                    f"READ {ch_name}"
                )

                # Big improvement: do not wait 1 second per channel
                msg = wait_for_response(timeout=0.02)

                if msg is None:
                    continue

                command_id   = msg.data[0]
                parameter_id = msg.data[1]
                status_code  = msg.data[2]
                payload      = msg.data[3:8]

                if (
                    command_id == ACE_CMD_READ and
                    status_code == ACE_STATUS_DATA_FOLLOWS and
                    parameter_id == expected_param
                ):
                    value = (
                        payload[0]
                        | (payload[1] << 8)
                        | (payload[2] << 16)
                        | (payload[3] << 24)
                    )

                    channel_data[plot_index].append(value)

            now = time.time()

            # Limit plot refresh rate
            if now - last_plot_update >= UPDATE_DT:
                last_plot_update = now

                for i in range(12):
                    data = list(channel_data[i])

                    if not data:
                        continue

                    x = range(len(data))
                    lines[i].set_data(x, data)

                    ax[i].set_xlim(0, PLOT_LEN)

                    ymin = min(data)
                    ymax = max(data)

                    if ymin == ymax:
                        ymin -= 1
                        ymax += 1

                    ax[i].set_ylim(ymin, ymax)

                fig.canvas.draw_idle()
                fig.canvas.flush_events()

    except KeyboardInterrupt:
        safe_print("Stopping live plot...")
        plt.ioff()
        plt.close()

# END PLOTTING functions --------------------------------------------------------------------------------

def read_all_channels(bus):

    channels = [
        ("ch0",  RANGER.PARAM_RAW_CH0),
        ("ch1",  RANGER.PARAM_RAW_CH1),
        ("ch2",  RANGER.PARAM_RAW_CH2),
        ("ch3",  RANGER.PARAM_RAW_CH3),
        ("ch4",  RANGER.PARAM_RAW_CH4),
        ("ch5",  RANGER.PARAM_RAW_CH5),
        ("ch6",  RANGER.PARAM_RAW_CH6),
        ("ch7",  RANGER.PARAM_RAW_CH7),
        ("ch8",  RANGER.PARAM_RAW_CH8),
        ("ch9",  RANGER.PARAM_RAW_CH9),
        ("ch10", RANGER.PARAM_RAW_CH10),
        ("ch11", RANGER.PARAM_RAW_CH11),
    ]

    values = [0] * 12

    for i, (ch_name, expected_param) in enumerate(channels):

        send_command(
            bus,
            build_read(ch_name),
            f"READ {ch_name}"
        )

        msg = wait_for_response(timeout=2)

        if msg is None:
            continue

        command_id   = msg.data[0]
        parameter_id = msg.data[1]
        status_code  = msg.data[2]
        payload      = msg.data[3:8]

        if (
            command_id == ACE_CMD_READ and
            status_code == ACE_STATUS_DATA_FOLLOWS and
            parameter_id == expected_param
        ):
            values[i] = decode_u32_le(payload)

    return values



def calibrate(bus):
    duration_s = 100
    start_time = time.monotonic()

    ch_max = [float("-inf")] * 12
    ch_min = [float("inf")] * 12

    samples = []

    while (time.monotonic() - start_time) < duration_s:
        values = read_all_channels(bus)

        if values is None:
            continue

        samples.append(values.copy())

        for i, value in enumerate(values):
            ch_max[i] = max(ch_max[i], value)
            ch_min[i] = min(ch_min[i], value)

    max_sum = [0.0] * 12
    min_sum = [0.0] * 12
    max_count = [0] * 12
    min_count = [0] * 12

    extrema_window = 0.01

    for values in samples:
        for i, value in enumerate(values):
            span = ch_max[i] - ch_min[i]

            if span <= 0:
                continue

            max_threshold = ch_max[i] - extrema_window * span
            min_threshold = ch_min[i] + extrema_window * span

            if value >= max_threshold:
                max_sum[i] += value
                max_count[i] += 1

            if value <= min_threshold:
                min_sum[i] += value
                min_count[i] += 1

    avg_max = [0.0] * 12
    avg_min = [0.0] * 12
    ch_offset = [0.0] * 12
    ch_gain = [0.0] * 12

    for i in range(12):
        avg_max[i] = max_sum[i] / max_count[i] if max_count[i] > 0 else ch_max[i]
        avg_min[i] = min_sum[i] / min_count[i] if min_count[i] > 0 else ch_min[i]

        ch_offset[i] = (avg_max[i] + avg_min[i]) / 2.0
        ch_gain[i] = (avg_max[i] - avg_min[i]) / 2.0

    safe_print("Calibration complete")
    safe_print("ch_min    =", ch_min)
    safe_print("ch_max    =", ch_max)
    safe_print("avg_min   =", avg_min)
    safe_print("avg_max   =", avg_max)
    safe_print("min_count =", min_count)
    safe_print("max_count =", max_count)
    safe_print("ch_offset =", ch_offset)
    safe_print("ch_gain   =", ch_gain)

    return ch_offset, ch_gain

import time
import math
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets

import time
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets

def get_angle(bus, ch_offset, ch_gain):

    app = QtWidgets.QApplication([])

    win = pg.GraphicsLayoutWidget(show=True)
    win.setWindowTitle("Differential Sin/Cos")

    #
    # TOP PLOT
    # sin_diff and cos_diff vs time
    #
    plot_time = win.addPlot(title="Differential Sin/Cos vs Time")

    plot_time.addLegend()
    plot_time.showGrid(x=True, y=True)

    plot_time.setYRange(-1.2, 1.2)

    sin_curve = plot_time.plot(
        pen='r',
        name='sin_diff'
    )

    cos_curve = plot_time.plot(
        pen='b',
        name='cos_diff'
    )

    #
    # Angle text display
    #
    angle_text = pg.TextItem(
        text="Angle: 0.00000 deg",
        color='w',
        anchor=(0, 0)
    )

    plot_time.addItem(angle_text)

    #
    # place text in upper left corner
    #
    angle_text.setPos(10, 1.05)

    #
    # NEXT ROW
    #
    win.nextRow()

    #
    # BOTTOM PLOT
    # XY circle plot
    #
    plot_xy = win.addPlot(title="Sin/Cos XY Plot")

    plot_xy.showGrid(x=True, y=True)

    plot_xy.setXRange(-1.2, 1.2)
    plot_xy.setYRange(-1.2, 1.2)

    plot_xy.setAspectLocked(True)

    xy_curve = plot_xy.plot(
        pen=None,
        symbol='o',
        symbolSize=3
    )

    history_len = 1000

    sin_history = [0] * history_len
    cos_history = [0] * history_len

    xy_x = [0] * history_len
    xy_y = [0] * history_len

    while True:

        ch_raw = read_all_channels(bus)

        if ch_raw is None:
            continue

        ch_norm = [0] * 12

        for i in range(12):

            if ch_gain[i] != 0:

                ch_norm[i] = (
                    (ch_raw[i] - ch_offset[i])
                    / ch_gain[i]
                )

        #
        # grouped channels
        #
        sin_1 = (ch_norm[0] + ch_norm[4] + ch_norm[8]) / 3
        sin_2 = (ch_norm[2] + ch_norm[6] + ch_norm[10]) / 3

        cos_1 = (ch_norm[1] + ch_norm[5] + ch_norm[9]) / 3
        cos_2 = (ch_norm[3] + ch_norm[7] + ch_norm[11]) / 3

        #
        # differential signals
        #
        sin_diff = (sin_1 - sin_2) / 2
        cos_diff = (cos_1 - cos_2) / 2

        #
        # angle calculation
        #
        angle_rad = math.atan2(sin_diff, cos_diff)

        angle_deg = math.degrees(angle_rad) / 3

        #
        # wrap to 0..120 deg
        #
        if angle_deg < 0:
            angle_deg += 120.0

        #
        # update text
        #
        angle_text.setText(
            f"Angle: {angle_deg:.5f} deg"
        )

        #
        # update rolling buffers
        #
        sin_history.append(sin_diff)
        sin_history.pop(0)

        cos_history.append(cos_diff)
        cos_history.pop(0)

        #
        # XY buffers
        #
        xy_x.append(cos_diff)
        xy_x.pop(0)

        xy_y.append(sin_diff)
        xy_y.pop(0)

        #
        # update plots
        #
        sin_curve.setData(sin_history)
        cos_curve.setData(cos_history)

        xy_curve.setData(xy_x, xy_y)

        app.processEvents()

        time.sleep(0.02)
    


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
        
        while True:
            command = input("RANGER> ")

            tokens = command.strip().split()

            if len(tokens) == 0:
                continue

            # -------------------------
            # READ
            # -------------------------
            if tokens[0] == "read":

                if len(tokens) != 2:
                    print("Usage: read <parameter>")
                    continue

                parameter_name = tokens[1]

                send_command(
                    bus,
                    build_read(parameter_name),
                    f"READ {parameter_name}"
                )

            # -------------------------
            # WRITE
            # -------------------------
            elif tokens[0] == "write":

                if len(tokens) != 3:
                    print("Usage: write <parameter> <value>")
                    continue

                parameter_name = tokens[1]

                try:
                    value = int(tokens[2])

                except ValueError:
                    print("Invalid value")
                    continue

                send_command(
                    bus,
                    build_write(parameter_name, value),
                    f"WRITE {parameter_name} = {value}"
                )

            # -------------------------
            # Program
            # -------------------------
            elif tokens[0] == "program":
                program_new_firmware(bus)
            
            
            # -------------------------
            # Plot encoder data measurments
            # -------------------------
            elif tokens[0] == "plot":
                plot_channels_live_pyqtgraph(bus)

            # -------------------------
            # read all channels
            # -------------------------
            elif tokens[0] == "readall":
                values = read_all_channels(bus)
                safe_print(values)    
            
            # -------------------------
            # Encoder calibration and angle read
            # -------------------------
            elif tokens[0] == "cali":
                ch_offset, ch_gain = calibrate(bus)
                get_angle(bus, ch_offset, ch_gain)

            # -------------------------
            # Exit python script
            # -------------------------
            elif tokens[0] in ["q", "quit", "exit"]:
                safe_print("Exiting...")
                running = False
                break

if __name__ == "__main__":
    main()
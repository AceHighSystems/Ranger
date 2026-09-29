#!/usr/bin/env python3

import can
import struct

# ============================================================
# Configuration
# ============================================================

CHANNEL = "/dev/cu.usbmodem2080317458421"
BITRATE = 1_000_000

HOST_ID   = 1
RANGER_ID = 2

# ============================================================
# AceLight
# ============================================================

BROADCAST      = 0x01
WRITE_REQUEST  = 0x02
WRITE_RESPONSE = 0x03
READ_REQUEST   = 0x04
READ_RESPONSE  = 0x05

OK           = 0x01
DATA_FOLLOWS = 0x02

# name: (parameter ID, data type)

PARAM = {
    "sync":                 (0x01, "U8"),
    "node_id":              (0x02, "U8"),
    "device_type":          (0x03, "U32"),
    "serial_number":        (0x04, "U32"),
    "firmware_version":     (0x05, "U32"),
    "hardware_version":     (0x06, "U32"),
    "protocol_version":     (0x07, "U32"),

    "step_enable":          (0x40, "U8"),
    "step_move":            (0x41, "I32"),
    "target_position":      (0x42, "I32"),
    "position":             (0x43, "I32"),
    "angle":                (0x44, "I32"),
    "profile_velocity":     (0x45, "U32"),
    "profile_acceleration": (0x46, "U32"),
    "profile_deceleration": (0x47, "U32"),
    "microstep":            (0x48, "U32"),

    "voltage":              (0x80, "I32"),
    "current":              (0x81, "I32"),
    "temperature":          (0x82, "I32"),

    "led":                  (0xC0, "U8"),
    "test":                 (0xC1, "U32"),
    "error_flag":           (0xC2, "U32"),
}

FMT = {
    "U8":  "<B",
    "U16": "<H",
    "U32": "<I",
    "I16": "<h",
    "I32": "<i",
}

# ============================================================
# Identifier
# ============================================================

def make_id(msg_type, destination, source, parameter):
    return ((msg_type & 7) << 26 |
            (destination & 0x7F) << 18 |
            (source & 0x7F) << 10 |
            (parameter & 0x1FF))


def decode_id(identifier):
    return (
        (identifier >> 26) & 7,
        (identifier >> 18) & 0x7F,
        (identifier >> 10) & 0x7F,
        identifier & 0x1FF
    )

# ============================================================
# Request / response
# ============================================================

def request(bus, msg_type, param_id, data=b""):

    bus.send(can.Message(
        arbitration_id=make_id(msg_type, RANGER_ID, HOST_ID, param_id),
        is_extended_id=True,
        data=data
    ))

    while True:

        msg = bus.recv(1.0)

        if msg is None:
            print("No response")
            return None

        msg_type_rx, destination, source, parameter = decode_id(msg.arbitration_id)

        if (destination == HOST_ID and
            source == RANGER_ID and
            parameter == param_id and
            msg_type_rx in (READ_RESPONSE, WRITE_RESPONSE)):

            return msg.data

# ============================================================
# Commands
# ============================================================

def read_param(bus, name):

    if name not in PARAM:
        print("Unknown parameter")
        return

    param_id, data_type = PARAM[name]
    data = request(bus, READ_REQUEST, param_id)

    if not data:
        return

    if data[0] != DATA_FOLLOWS:
        print(f"Status: 0x{data[0]:02X}")
        return

    value = struct.unpack(FMT[data_type], data[1:])[0]

    print(f"{name} = {value}")


def write_param(bus, name, value):

    if name not in PARAM:
        print("Unknown parameter")
        return

    param_id, data_type = PARAM[name]

    data = struct.pack(FMT[data_type], int(value))

    response = request(bus, WRITE_REQUEST, param_id, data)

    if response:
        print("OK" if response[0] == OK else f"Status: 0x{response[0]:02X}")

# ============================================================
# Main
# ============================================================

with can.Bus(
    interface="slcan",
    channel=CHANNEL,
    bitrate=BITRATE
) as bus:

    print("AceLight connected")
    print("Commands: read <parameter>, write <parameter> <value>, exit")

    while True:

        cmd = input("> ").lower().split()

        if not cmd:
            continue

        if cmd[0] == "exit":
            break

        if cmd[0] == "read" and len(cmd) == 2:
            read_param(bus, cmd[1])

        elif cmd[0] == "write" and len(cmd) == 3:
            write_param(bus, cmd[1], cmd[2])

        else:
            print("Invalid command")
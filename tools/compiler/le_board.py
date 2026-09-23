#!/usr/bin/env python3
"""
LogicElements Board Discovery & Profile Utility
Allows desktop tooling, IDEs, and compilers to discover board capabilities
either by querying live hardware over UART or by importing .leconfig files.
"""

import sys
import os
import json
import argparse
import struct
from typing import Dict, Any, Optional

# Protocol constants
SYNC_BYTE = 0xAA
CMD_GET_CAPS = 0x02
CMD_CAPS_DATA = 0x82
CMD_GET_CUSTOM_NODES = 0x03
CMD_CUSTOM_NODES_DATA = 0x83

def crc16_ccitt(data: bytes) -> int:
    """Calculates the 16-bit CCITT CRC checksum for a byte sequence.

    Args:
        data: Input byte sequence to hash.

    Returns:
        The computed 16-bit CRC checksum.
    """
    crc = 0
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def load_board_profile(filepath: str) -> Dict[str, Any]:
    """Loads and parses a board profile configuration file (.leconfig).

    Args:
        filepath: Filesystem path to the .leconfig JSON profile.

    Returns:
        Parsed dictionary containing board specifications, limits, and pin map.

    Raises:
        FileNotFoundError: If the specified profile does not exist.
    """
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Board profile not found: {filepath}")
    with open(filepath, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data

def save_board_profile(profile: Dict[str, Any], filepath: str):
    """Saves a board profile dictionary to disk in formatted JSON.

    Args:
        profile: Board specification dictionary to serialize.
        filepath: Destination filesystem path.
    """
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(profile, f, indent=2)
    print(f"[OK] Saved board profile to: {filepath}")

def query_live_board_serial(port: str, baud: int = 115200, timeout: float = 2.0) -> Dict[str, Any]:
    """Queries a live connected board over UART to retrieve capabilities.

    Args:
        port: Serial communication port name (e.g. 'COM3' or '/dev/ttyUSB0').
        baud: Serial baud rate in symbols per second (default: 115200).
        timeout: Read timeout in seconds.

    Returns:
        Structured dictionary containing device limits, pin mappings, and features.

    Raises:
        TimeoutError: If the device fails to respond within the timeout window.
    """
    try:
        import serial
    except ImportError:
        print("Error: pyserial is required for live device discovery. Install via 'pip install pyserial'.")
        sys.exit(1)

    print(f"Connecting to device on {port} @ {baud} baud...")
    with serial.Serial(port, baud, timeout=timeout) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # Try Method 1: Framed Binary Packet (LE_CMD_GET_CAPS)
        frame = bytearray([SYNC_BYTE, CMD_GET_CAPS, 0x01, 0x00, 0x00])
        crc = crc16_ccitt(frame[1:])
        frame.extend([crc & 0xFF, (crc >> 8) & 0xFF])
        ser.write(frame)

        # Wait for SYNC_BYTE
        header = ser.read(5)
        if len(header) == 5 and header[0] == SYNC_BYTE and header[1] == CMD_CAPS_DATA:
            length = header[3] | (header[4] << 8)
            payload = ser.read(length)
            crc_bytes = ser.read(2)
            if len(payload) == length:
                profile = parse_binary_caps_payload(payload)

                # Query custom nodes binary packet
                cn_frame = bytearray([SYNC_BYTE, CMD_GET_CUSTOM_NODES, 0x01, 0x00, 0x00])
                cn_crc = crc16_ccitt(cn_frame[1:])
                cn_frame.extend([cn_crc & 0xFF, (cn_crc >> 8) & 0xFF])
                ser.write(cn_frame)

                cn_header = ser.read(5)
                if len(cn_header) == 5 and cn_header[0] == SYNC_BYTE and cn_header[1] == CMD_CUSTOM_NODES_DATA:
                    cn_len = cn_header[3] | (cn_header[4] << 8)
                    cn_payload = ser.read(cn_len)
                    ser.read(2)  # CRC
                    if len(cn_payload) == cn_len:
                        try:
                            profile["custom_nodes"] = json.loads(cn_payload.decode("utf-8"))
                        except Exception:
                            profile["custom_nodes"] = []
                return profile

        # Try Method 2: Fallback to Terminal CLI "caps" command
        ser.write(b"\r\ncaps\r\n")
        lines = ser.read(2048).decode("utf-8", errors="ignore")
        start_idx = lines.find("{")
        end_idx = lines.rfind("}")
        if start_idx != -1 and end_idx != -1:
            json_str = lines[start_idx : end_idx + 1]
            return json.loads(json_str)

        raise TimeoutError(f"No valid response from device on {port}")

def parse_binary_caps_payload(data: bytes) -> Dict[str, Any]:
    """Unpacks a binary le_caps_payload_t structure into a board profile.

    Args:
        data: Raw byte sequence containing the capabilities struct.

    Returns:
        Structured dictionary representing board configuration.
    """
    # struct format: <BBB 32s H H H H H H H B H H B B
    fmt = "<BBB32sHHHHHHHBHHBB"
    fields = struct.unpack(fmt, data[:struct.calcsize(fmt)])
    
    plat_name = fields[3].split(b"\x00")[0].decode("ascii", errors="ignore")
    flags = fields[13]

    profile = {
        "device": {
            "name": plat_name,
            "firmware_version": f"{fields[1]}.{fields[2]}",
            "protocol_version": fields[0]
        },
        "limits": {
            "digital_inputs": fields[4],
            "digital_outputs": fields[5],
            "analog_inputs": fields[6],
            "bool_registers": fields[7],
            "floats": fields[8],
            "timers": fields[9],
            "counters": fields[10],
            "config_slots": fields[11],
            "slot_size_bytes": fields[12] if len(fields) > 12 else 2048
        },
        "features": {
            "protection": bool(flags & (1 << 0)),
            "serial_bus": bool(flags & (1 << 1)),
            "i2c_devices": fields[14],
            "spi_devices": fields[15]
        },
        "pin_map": {
            "inputs": {},
            "outputs": {}
        }
    }
    return profile

def print_profile_info(profile: Dict[str, Any]):
    """Displays a formatted summary table of board capabilities to standard output.

    Args:
        profile: Board specification dictionary to display.
    """
    dev = profile.get("device", {})
    lim = profile.get("limits", {})
    feat = profile.get("features", {})
    pin = profile.get("pin_map", {})

    print("=" * 60)
    print(" LOGICELEMENTS TARGET BOARD PROFILE")
    print("=" * 60)
    print(f"Board Name:          {dev.get('name', 'Unknown')}")
    print(f"Firmware Version:    v{dev.get('firmware_version', '1.0')}")
    print(f"Protocol Version:    {dev.get('protocol_version', 1)}")
    print("-" * 60)
    print("HARDWARE LIMITS & MEMORY:")
    print(f"  Digital Inputs (%I):   {lim.get('digital_inputs', 0)}")
    print(f"  Digital Outputs (%Q):  {lim.get('digital_outputs', 0)}")
    print(f"  Internal Coils (%M):   {lim.get('coils', 0)}")
    print(f"  Float Registers (%R):  {lim.get('floats', 0)}")
    print(f"  Timers:                {lim.get('timers', 0)}")
    print(f"  Counters:              {lim.get('counters', 0)}")
    print(f"  Config Slots:          {lim.get('config_slots', 0)} slots ({lim.get('slot_size_bytes', 0)} bytes each)")
    print("-" * 60)
    print("SUPPORTED SUB-SYSTEMS:")
    print(f"  Protection & Control:  {'ENABLED' if feat.get('protection') else 'DISABLED'}")
    print(f"  Serial Bus Elements:   {'ENABLED' if feat.get('serial_bus') else 'DISABLED'}")
    if feat.get('serial_bus'):
        print(f"    Max I2C Devices:     {feat.get('i2c_devices', 0)}")
        print(f"    Max SPI Devices:     {feat.get('spi_devices', 0)}")
    print("-" * 60)
    inputs = pin.get("inputs", {})
    if inputs:
        print(f"PIN MAPPINGS (Inputs: {len(inputs)}):")
        for addr, info in inputs.items():
            print(f"  {addr:6} -> {info.get('alias', '-'):16} (Pin: {info.get('pin', '-')}) : {info.get('desc', '')}")
    outputs = pin.get("outputs", {})
    if outputs:
        print(f"PIN MAPPINGS (Outputs: {len(outputs)}):")
        for addr, info in outputs.items():
            print(f"  {addr:6} -> {info.get('alias', '-'):16} (Pin: {info.get('pin', '-')}) : {info.get('desc', '')}")
    custom_nodes = profile.get("custom_nodes", [])
    if custom_nodes:
        print("-" * 60)
        print(f"CUSTOM BOARD NODES ({len(custom_nodes)}):")
        for cn in custom_nodes:
            tid = cn.get("type_id", cn.get("name", "Unnamed"))
            dname = cn.get("display_name", tid)
            fn_id = cn.get("function_id", 0)
            print(f"  [{fn_id}] {dname:<24} ({tid})")
    print("=" * 60)

def generate_template(filepath: str):
    """Generates a starter .leconfig template file for custom hardware.

    Args:
        filepath: Destination filesystem path for the generated template.
    """
    template = {
        "device": {
            "name": "My_Custom_MCU",
            "firmware_version": "1.0",
            "protocol_version": 1
        },
        "limits": {
            "digital_inputs": 16,
            "digital_outputs": 16,
            "coils": 128,
            "floats": 64,
            "timers": 16,
            "counters": 8,
            "config_slots": 3,
            "slot_size_bytes": 2048
        },
        "features": {
            "protection": True,
            "serial_bus": True,
            "i2c_devices": 4,
            "spi_devices": 4
        },
        "pin_map": {
            "inputs": {
                "%I0": {"alias": "START_PB", "pin": "P0_0", "desc": "Start Pushbutton"},
                "%I1": {"alias": "STOP_PB",  "pin": "P0_1", "desc": "Stop Pushbutton"}
            },
            "outputs": {
                "%Q0": {"alias": "RUN_RELAY", "pin": "P1_0", "desc": "Running Coil Contactor"}
            }
        }
    }
    save_board_profile(template, filepath)

def main():
    """Command-line interface entry point for board discovery."""
    parser = argparse.ArgumentParser(description="LogicElements Board Discovery & Profile Utility")
    parser.add_argument("--info", "-i", type=str, help="Display capabilities of an .leconfig file")
    parser.add_argument("--port", "-p", type=str, help="Serial port to query live device (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--output", "-o", type=str, help="Output .leconfig destination file")
    parser.add_argument("--template", action="store_true", help="Generate a blank .leconfig template file")

    args = parser.parse_args()

    if args.info:
        profile = load_board_profile(args.info)
        print_profile_info(profile)
        return

    if args.template:
        out_path = args.output or "custom_board.leconfig"
        generate_template(out_path)
        return

    if args.port:
        profile = query_live_board_serial(args.port, args.baud)
        print_profile_info(profile)
        if args.output:
            save_board_profile(profile, args.output)
        return

    parser.print_help()

if __name__ == "__main__":
    main()

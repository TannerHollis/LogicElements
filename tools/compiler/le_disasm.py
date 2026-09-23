#!/usr/bin/env python3
"""
LogicElements Disassembler (.lebin -> readable assembly/instruction listing)
Wraps the canonical LogicElements native C++ disassembler.
"""

import sys
import argparse
import ctypes
from ctypes import c_uint8, c_size_t, c_int, c_char_p

try:
    from le_compiler import get_compiler_lib
except ImportError:
    from .le_compiler import get_compiler_lib

def disassemble(bin_data: bytes, user_bool_count: int = -1, user_float_count: int = -1, user_int_count: int = -1) -> str:
    """Disassembles a .lebin binary file into human-readable listing.

    Args:
        bin_data: Raw bytes of the .lebin binary file.
        user_bool_count: Number of user bool registers (-1 for unknown).
        user_float_count: Number of user float registers (-1 for unknown).
        user_int_count: Number of user int registers (-1 for unknown).

    Returns:
        Formatted disassembly listing string.
    """
    if len(bin_data) < 26:
        print("Error: Binary file is too small to contain a valid header.", file=sys.stderr)
        return ""

    lib = get_compiler_lib()
    raw_arr = (c_uint8 * len(bin_data)).from_buffer_copy(bin_data)
    out_dis = c_char_p()
    out_err = c_char_p()

    rc = lib.le_disassemble_bin(
        raw_arr,
        len(bin_data),
        user_bool_count,
        user_float_count,
        user_int_count,
        ctypes.byref(out_dis),
        ctypes.byref(out_err)
    )

    if rc != 0 or not out_dis.value:
        err_msg = out_err.value.decode("utf-8") if out_err.value else "Disassembly failed"
        if out_err.value:
            lib.le_compiler_free_buffer(out_err)
        raise RuntimeError(err_msg)

    try:
        text = out_dis.value.decode("utf-8")
        return text
    finally:
        lib.le_compiler_free_buffer(out_dis)

def main():
    """Command-line interface entry point for disassembling .lebin files."""
    parser = argparse.ArgumentParser(description="LogicElements Disassembler")
    parser.add_argument("file", help="Path to .lebin file")
    args = parser.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()

    try:
        text = disassemble(data)
        if text:
            print(text, end="")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()

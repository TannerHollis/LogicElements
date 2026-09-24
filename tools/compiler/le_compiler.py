#!/usr/bin/env python3
"""
LogicElements Compiler (JSON -> .lebin)
Python ctypes wrapper calling the canonical LogicElements native C++ compiler library (le_compiler.dll/.so).
"""

import os
import sys
import json
import ctypes
import ctypes.util
import argparse
from ctypes import c_int, c_char_p, c_uint8, c_size_t, c_uint32, POINTER, Structure
from typing import Dict, List, Any, Optional

# Binary constants for compatibility with existing scripts
LE_BIN_MAGIC = 0x4C454231  # ASCII "LEB1"
LE_BIN_VERSION = 7
LE_FLAG_AUTOSTART = 0x0001

# Address Regions
LE_REGION_DIN   = 0x0000
LE_REGION_DOUT  = 0x1000
LE_REGION_BOOL  = 0x2000
LE_REGION_COIL  = 0x2000
LE_REGION_FLOAT = 0x4000
LE_REGION_TIMER = 0x8000
LE_REGION_CONST = 0xC000

LE_CONST_FALSE  = 0xC000
LE_CONST_TRUE   = 0xC001
LE_CONST_ZERO_F = 0xC002
LE_CONST_ONE_F  = 0xC003
LE_ADDR_UNUSED  = 0xFFFF

LE_OPT_NONE = 0
LE_OPT_BASIC = 1
LE_OPT_FULL = 2
LE_OPT_SIZE = 3

class _LeCompilerOptions(Structure):
    _fields_ = [
        ("opt_level", c_int),
        ("eliminate_dead_code", c_int),
        ("fold_constants", c_int),
        ("fold_inversions", c_int),
        ("eliminate_common_subexpr", c_int),
        ("direct_destination", c_int),
        ("inline_user_nodes", c_int),
        ("max_pass_iterations", c_int),
    ]

    @classmethod
    def from_level(cls, level: int):
        opts = cls()
        opts.opt_level = level
        opts.max_pass_iterations = 5
        if level == LE_OPT_NONE:
            opts.eliminate_dead_code = 0
            opts.fold_constants = 0
            opts.fold_inversions = 0
            opts.eliminate_common_subexpr = 0
            opts.direct_destination = 0
            opts.inline_user_nodes = 0
        elif level == LE_OPT_BASIC:
            opts.eliminate_dead_code = 1
            opts.fold_constants = 1
            opts.fold_inversions = 1
            opts.eliminate_common_subexpr = 0
            opts.direct_destination = 0
            opts.inline_user_nodes = 0
        elif level == LE_OPT_FULL:
            opts.eliminate_dead_code = 1
            opts.fold_constants = 1
            opts.fold_inversions = 1
            opts.eliminate_common_subexpr = 1
            opts.direct_destination = 1
            opts.inline_user_nodes = 1
        elif level == LE_OPT_SIZE:
            opts.eliminate_dead_code = 1
            opts.fold_constants = 1
            opts.fold_inversions = 1
            opts.eliminate_common_subexpr = 1
            opts.direct_destination = 1
            opts.inline_user_nodes = 0
        return opts

class _LeCompileResult(Structure):
    _fields_ = [
        ("success", c_int),
        ("error_message", c_char_p),
        ("warnings", c_char_p),
        ("binary_data", POINTER(c_uint8)),
        ("binary_size", c_size_t),
        ("c_header_code", c_char_p),
        ("disassembly_text", c_char_p),
        ("json_circuit", c_char_p),
        ("instruction_count", c_int),
        ("din_count", c_int),
        ("dout_count", c_int),
        ("ain_count", c_int),
        ("bool_reg_count", c_int),
        ("float_count", c_int),
        ("complex_count", c_int),
        ("int_count", c_int),
        ("timer_count", c_int),
        ("counter_count", c_int),
        ("alias_count", c_int),
        ("crc32", c_uint32),
        ("user_bool_count", c_int),
        ("temp_bool_count", c_int),
        ("user_float_count", c_int),
        ("temp_float_count", c_int),
        ("user_complex_count", c_int),
        ("temp_complex_count", c_int),
        ("user_int_count", c_int),
        ("temp_int_count", c_int),
        ("eliminated_instructions", c_int),
        ("eliminated_registers", c_int),
    ]

_cached_lib = None

def get_compiler_lib():
    """Locates and loads the native le_compiler shared library."""
    global _cached_lib
    if _cached_lib is not None:
        return _cached_lib

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(script_dir, "..", ".."))

    candidates = [
        os.path.join(root_dir, "build", "Debug", "le_compiler.dll"),
        os.path.join(root_dir, "build", "Release", "le_compiler.dll"),
        os.path.join(root_dir, "build", "le_compiler.dll"),
        os.path.join(root_dir, "build", "Debug", "lible_compiler.so"),
        os.path.join(root_dir, "build", "Release", "lible_compiler.so"),
        os.path.join(root_dir, "build", "lible_compiler.so"),
        os.path.join(root_dir, "build", "Debug", "lible_compiler.dylib"),
        os.path.join(root_dir, "build", "Release", "lible_compiler.dylib"),
        os.path.join(script_dir, "le_compiler.dll"),
        os.path.join(script_dir, "lible_compiler.so"),
        os.path.join(script_dir, "lible_compiler.dylib"),
        os.path.join(root_dir, "bin", "le_compiler.dll"),
    ]

    lib_path = None
    for p in candidates:
        if os.path.isfile(p):
            lib_path = p
            break

    if not lib_path:
        lib_path = ctypes.util.find_library("le_compiler")

    if not lib_path:
        raise FileNotFoundError(
            "Could not locate the compiled LogicElements compiler library (le_compiler.dll/so).\n"
            "Please build the project first using CMake: 'cmake --build build'."
        )

    lib = ctypes.CDLL(lib_path)

    lib.le_compile_json.argtypes = [c_char_p, c_char_p, POINTER(_LeCompileResult)]
    lib.le_compile_json.restype = c_int

    lib.le_compile_json_ex.argtypes = [c_char_p, c_char_p, POINTER(_LeCompilerOptions), POINTER(_LeCompileResult)]
    lib.le_compile_json_ex.restype = c_int

    lib.le_compile_result_free.argtypes = [POINTER(_LeCompileResult)]
    lib.le_compile_result_free.restype = None

    lib.le_disassemble_bin.argtypes = [
        POINTER(c_uint8), c_size_t, c_int, c_int, c_int,
        c_char_p, POINTER(c_char_p), POINTER(c_char_p)
    ]
    lib.le_disassemble_bin.restype = c_int

    lib.le_export_c_header.argtypes = [POINTER(c_uint8), c_size_t, c_char_p, POINTER(c_char_p)]
    lib.le_export_c_header.restype = c_int

    lib.le_compiler_free_buffer.argtypes = [ctypes.c_void_p]
    lib.le_compiler_free_buffer.restype = None

    lib.le_compiler_get_version.argtypes = []
    lib.le_compiler_get_version.restype = c_char_p

    _cached_lib = lib
    return lib

class LeCompiler:
    """Compiles visual logic element networks into executable .lebin binaries.
    Backed by the canonical LogicElements native C++ compiler library.
    """

    def __init__(self, board_profile: Optional[Dict[str, Any]] = None, opt_level: int = LE_OPT_FULL):
        self.board_profile = board_profile
        self.opt_level = opt_level
        self.din_map: Dict[str, int] = {}
        self.dout_map: Dict[str, int] = {}
        self.instructions: List[Any] = []
        self.coil_count = 0
        self.timer_count = 0
        self.float_count = 0
        self.eliminated_instructions = 0
        self.eliminated_registers = 0
        self.disassembly_text = ""
        self.c_header_code = ""

    def compile(self, json_data: Any, opt_level: Optional[int] = None) -> bytes:
        """Compiles a circuit configuration dictionary or JSON string into a .lebin binary.

        Args:
            json_data: Dictionary or JSON string containing elements and nets.
            opt_level: Optional optimization level override (LE_OPT_NONE, LE_OPT_BASIC, LE_OPT_FULL, LE_OPT_SIZE).

        Returns:
            A bytes object containing the complete v5 header and bytecode payload.
        """
        if isinstance(json_data, dict):
            circuit_json_str = json.dumps(json_data)
        else:
            circuit_json_str = str(json_data)

        board_json_str = json.dumps(self.board_profile) if self.board_profile else None

        effective_opt = self.opt_level if opt_level is None else opt_level
        opts = _LeCompilerOptions.from_level(effective_opt)

        lib = get_compiler_lib()
        res = _LeCompileResult()

        b_circuit = circuit_json_str.encode("utf-8")
        b_board = board_json_str.encode("utf-8") if board_json_str else None

        rc = lib.le_compile_json_ex(b_circuit, b_board, ctypes.byref(opts), ctypes.byref(res))

        try:
            if rc != 0 or res.success == 0:
                err_msg = res.error_message.decode("utf-8") if res.error_message else "Compilation failed"
                raise ValueError(err_msg)

            self.instructions = [None] * res.instruction_count
            self.coil_count = res.bool_reg_count
            self.float_count = res.float_count
            self.timer_count = res.timer_count
            self.eliminated_instructions = res.eliminated_instructions
            self.eliminated_registers = res.eliminated_registers
            self.disassembly_text = res.disassembly_text.decode("utf-8") if res.disassembly_text else ""
            self.c_header_code = res.c_header_code.decode("utf-8") if res.c_header_code else ""

            bin_bytes = bytes(ctypes.string_at(res.binary_data, res.binary_size))
            return bin_bytes
        finally:
            lib.le_compile_result_free(ctypes.byref(res))

    def export_c_header(self, bin_data: bytes, array_name: str = "le_default_program") -> str:
        """Exports binary data as a C static byte array header."""
        lib = get_compiler_lib()
        raw_arr = (c_uint8 * len(bin_data)).from_buffer_copy(bin_data)
        out_hdr = c_char_p()

        rc = lib.le_export_c_header(raw_arr, len(bin_data), array_name.encode("utf-8"), ctypes.byref(out_hdr))
        if rc != 0 or not out_hdr.value:
            raise RuntimeError("Failed to generate C header")

        try:
            return out_hdr.value.decode("utf-8")
        finally:
            lib.le_compiler_free_buffer(out_hdr)

    def validate_against_board(self, bin_size: int):
        """No-op: board validation is performed automatically during native compilation."""
        pass

def main():
    """Command-line interface entry point for compiling circuits."""
    parser = argparse.ArgumentParser(description="LogicElements Circuit Compiler")
    parser.add_argument("input_json", help="Path to input circuit JSON file")
    parser.add_argument("-o", "--output", help="Path to output .lebin binary file")
    parser.add_argument("-b", "--board", help="Path to target .leconfig profile to enforce board capabilities")
    parser.add_argument("--header", help="Path to output C header file (.h)")
    parser.add_argument("-O0", dest="opt_none", action="store_true", help="Disable all optimizations")
    parser.add_argument("-O1", dest="opt_basic", action="store_true", help="Basic safe optimizations")
    parser.add_argument("-O2", dest="opt_full", action="store_true", help="Default full optimizations")
    parser.add_argument("-Os", dest="opt_size", action="store_true", help="Size-focused optimizations")
    parser.add_argument("--stats", action="store_true", help="Print optimization statistics")
    args = parser.parse_args()

    opt_level = LE_OPT_FULL
    if args.opt_none:
        opt_level = LE_OPT_NONE
    elif args.opt_basic:
        opt_level = LE_OPT_BASIC
    elif args.opt_size:
        opt_level = LE_OPT_SIZE
    elif args.opt_full:
        opt_level = LE_OPT_FULL

    board_profile = None
    if args.board:
        with open(args.board, "r", encoding="utf-8") as f:
            board_profile = json.load(f)
        dev_name = board_profile.get("device", {}).get("name", "Unknown")
        print(f"[INFO] Compiling for target board profile: {dev_name}")

    with open(args.input_json, "r", encoding="utf-8") as f:
        data = json.load(f)

    compiler = LeCompiler(board_profile=board_profile, opt_level=opt_level)
    try:
        bin_data = compiler.compile(data)
    except ValueError as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        sys.exit(1)

    if board_profile:
        print("[OK] Board capabilities validation PASSED.")

    if args.stats or opt_level != LE_OPT_NONE:
        print(f"[INFO] Optimization: {compiler.eliminated_instructions} instructions eliminated, {compiler.eliminated_registers} registers eliminated.")

    out_path = args.output
    if not out_path and not args.header:
        out_path = args.input_json.rsplit(".", 1)[0] + ".lebin"

    if out_path:
        with open(out_path, "wb") as f:
            f.write(bin_data)
        print(f"[OK] Successfully compiled {len(compiler.instructions)} instructions into {len(bin_data)} bytes -> {out_path}")

    if args.header:
        c_code = compiler.export_c_header(bin_data)
        with open(args.header, "w", encoding="utf-8") as f:
            f.write(c_code)
        print(f"[OK] Generated C embedded program header -> {args.header}")

if __name__ == "__main__":
    main()

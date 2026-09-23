#!/usr/bin/env python3
"""
Custom Nodes Example Runner
Compiles motor_control_circuit.json against custom_board.leconfig using le_compiler.py,
prints the disassembly listing, and validates generated EXT_CALL instructions.
"""

import os
import sys
import json

# Ensure tools/compiler is in import search path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
sys.path.insert(0, os.path.join(ROOT_DIR, "tools", "compiler"))

from le_compiler import LeCompiler, LE_OPT_FULL, LE_OPT_NONE

def main():
    board_path = os.path.join(SCRIPT_DIR, "custom_board.leconfig")
    circuit_path = os.path.join(SCRIPT_DIR, "motor_control_circuit.json")
    out_bin_path = os.path.join(SCRIPT_DIR, "motor_control.lebin")

    print("============================================================")
    print(" LOGICELEMENTS CUSTOM NODES EXAMPLE")
    print("============================================================")
    print(f"Board Profile: {board_path}")
    print(f"Circuit JSON:  {circuit_path}")

    with open(board_path, "r", encoding="utf-8") as f:
        board_data = json.load(f)

    with open(circuit_path, "r", encoding="utf-8") as f:
        circuit_data = json.load(f)

    print("\n[1] Registered Custom Nodes on Board:")
    for node in board_data.get("custom_nodes", []):
        print(f"  - {node['type_id']} (func_id: {node['function_id']}, category: {node['category']}): {node['description']}")

    print("\n[2] Compiling circuit with native optimizing compiler (-O2)...")
    compiler = LeCompiler(board_profile=board_data, opt_level=LE_OPT_FULL)
    binary_bytes = compiler.compile(circuit_data)

    with open(out_bin_path, "wb") as f:
        f.write(binary_bytes)

    print(f"[OK] Successfully compiled into {len(binary_bytes)} bytes -> {out_bin_path}")
    print(f"     Instructions: {len(compiler.instructions)}")
    print(f"     Eliminated instructions: {compiler.eliminated_instructions}")
    print(f"     Eliminated registers: {compiler.eliminated_registers}")

    print("\n[3] Disassembly Listing:")
    print("------------------------------------------------------------")
    print(compiler.disassembly_text)
    print("------------------------------------------------------------")

    # Assertions to ensure custom nodes were compiled properly
    disasm = compiler.disassembly_text
    assert "BLOCK" in disasm, "Disassembly must contain EXT_CALL opcodes"
    assert "fn:0x81" in disasm or "BLOCK(0x81)" in disasm, "Disassembly must contain PWM function call"
    assert "fn:0x82" in disasm or "BLOCK(0x82)" in disasm, "Disassembly must contain Encoder function call"
    assert "fn:0x83" in disasm or "BLOCK(0x83)" in disasm, "Disassembly must contain Filter function call"

    print("[PASS] Verified all custom nodes compiled to hardware LE_OP_BLOCK calls!")

if __name__ == "__main__":
    main()


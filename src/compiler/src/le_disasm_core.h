/**
 * @file le_disasm_core.h
 * @brief Internal bytecode disassembly and address formatting routines.
 */

#ifndef LE_DISASM_CORE_H
#define LE_DISASM_CORE_H

#include <stdint.h>
#include <string>

namespace LogicElements {

/**
 * @brief Formats a 16-bit process image address into symbolic assembly notation.
 *
 * @param addr Encoded 16-bit process image address.
 * @param user_bool_count Number of user boolean registers (`-1` if unknown).
 * @param user_float_count Number of user float registers (`-1` if unknown).
 * @param user_int_count Number of user integer registers (`-1` if unknown).
 * @return Formatted string representation (e.g. `%I[0]`, `%Q[1]`, `%M[2]`, `%R[3]`).
 */
std::string format_address(uint16_t addr, int user_bool_count = -1, int user_float_count = -1, int user_int_count = -1);

/**
 * @brief Formats an opcode and its modifier bitmask into symbolic mnemonic text.
 *
 * @param op Numerical opcode identifier.
 * @param mod Modifier bitmask flags (@ref LE_MOD_INVERT_A, etc.).
 * @return Formatted mnemonic string.
 */
std::string format_opcode(uint8_t op, uint8_t mod);

/**
 * @brief Disassembles binary bytecode into a complete assembly listing.
 *
 * @param bin_data Pointer to the raw binary bytecode payload.
 * @param bin_len Length of @p bin_data in bytes.
 * @param user_bool_count Number of user boolean registers (`-1` if unknown).
 * @param user_float_count Number of user float registers (`-1` if unknown).
 * @param user_int_count Number of user integer registers (`-1` if unknown).
 * @return Multi-line formatted disassembly string.
 */
std::string disassemble_binary(const uint8_t* bin_data, size_t bin_len, int user_bool_count = -1, int user_float_count = -1, int user_int_count = -1);

} // namespace LogicElements

#endif /* LE_DISASM_CORE_H */

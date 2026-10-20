/**
 * @file le_disasm_core.h
 * @brief Internal bytecode disassembly and process image address formatting routines.
 *
 * This module converts compiled binary bytecode containers (.lebin v13 LEB2) into structured,
 * human-readable assembly listings. It decodes container chunk headers, instruction streams,
 * rate domains (fast, medium, slow), block-call parameters, and process image memory registers.
 *
 * ## Thread Safety
 * All functions in this module are stateless and thread-safe. Multiple threads can disassemble
 * distinct binary payloads simultaneously without synchronization.
 */

#ifndef LE_DISASM_CORE_H
#define LE_DISASM_CORE_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace LogicElements {

/**
 * @brief Formats an encoded 16-bit process image address into symbolic assembly notation.
 *
 * Translates hardware register bits into user-friendly identifiers such as `%IN[0]`,
 * `%OUT[1]`, `%AIN[2]`, `%AOUT[0]`, `%B[3]` (or `T_B[0]` for temporary registers),
 * `%N[0]` (integers), and `%F[0]` (floats).
 *
 * @param[in] addr Encoded 16-bit process image address (containing region and index fields).
 * @param[in] user_bool_count Number of user-declared boolean registers, or `-1` if unknown.
 * @param[in] user_float_count Number of user-declared float registers, or `-1` if unknown.
 * @param[in] user_int_count Number of user-declared integer registers, or `-1` if unknown.
 * @return Formatted mnemonic address string.
 */
std::string format_address(
    uint16_t addr,
    int user_bool_count = -1,
    int user_float_count = -1,
    int user_int_count = -1
);

/**
 * @brief Formats an opcode into symbolic mnemonic text.
 *
 * @param[in] op Numerical opcode identifier (e.g. @ref LE_OP_AND, @ref LE_OP_ADD_F).
 * @param[in] mod Modifier bitmask flags (e.g. @ref LE_MOD_INVERT_A, @ref LE_MOD_INVERT_B).
 * @return Symbolic mnemonic string (e.g. `"AND"`, `"ADD_F"`, `"FMA_F"`).
 */
std::string format_opcode(uint8_t op, uint8_t mod);

/**
 * @brief Formats an instruction modifier byte into human-readable flags or index representation.
 *
 * @param[in] op Numerical opcode identifier.
 * @param[in] mod Modifier byte (inversion bitflags, instance index, or function ID).
 * @return Formatted modifier string (e.g. `0x00`, `~A`, `~B`, `~OUT`, `#0`, `fn:0x01`).
 */
std::string format_modifier(uint8_t op, uint8_t mod);

/**
 * @brief Disassembles binary bytecode into a complete assembly listing.
 *
 * Parses chunked containers (v13 LEB2) or legacy flat binaries, displaying container headers,
 * data segments, instruction streams annotated with rate domains, and state table summaries.
 *
 * @param[in] bin_data Pointer to the raw bytecode payload. Must not be null.
 * @param[in] bin_len Length of @p bin_data in bytes.
 * @param[in] user_bool_count Number of named user boolean registers, or `-1` if unknown.
 * @param[in] user_float_count Number of named user float registers, or `-1` if unknown.
 * @param[in] user_int_count Number of named user integer registers, or `-1` if unknown.
 * @param[in] board_profile_json Optional board profile JSON string used to annotate custom
 *                               node calls with human-readable node names and categories.
 * @return Multi-line formatted assembly listing string.
 *
 * @throws std::runtime_error If the binary contains malformed chunk headers or invalid lengths.
 */
std::string disassemble_binary(
    const uint8_t* bin_data,
    size_t bin_len,
    int user_bool_count = -1,
    int user_float_count = -1,
    int user_int_count = -1,
    const std::string& board_profile_json = ""
);

} // namespace LogicElements

#endif /* LE_DISASM_CORE_H */

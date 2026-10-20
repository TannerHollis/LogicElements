/**
 * @file le_compiler.h
 * @brief Public C Application Programming Interface for the canonical LogicElements circuit compiler.
 *
 * This header defines the stable, portable C ABI for compiling LogicElements schematic JSON
 * documents into deterministic virtual machine bytecode (.lebin v13 LEB2 chunked containers),
 * disassembling binaries, generating embedded C headers, and performing Ed25519 signing.
 *
 * ## Memory Management & Ownership Model
 * All heap buffers returned through pointer-to-pointer arguments or inside the @ref le_compile_result_t
 * structure are dynamically allocated by the compiler runtime.
 * - Call @ref le_compile_result_free on any populated @ref le_compile_result_t instance to release
 *   its internal strings, bytecode buffers, and disassembly text.
 * - Call @ref le_compiler_free_buffer to release standalone string buffers returned by
 *   @ref le_disassemble_bin and @ref le_export_c_header.
 *
 * ## Thread Safety & Reentrancy
 * All functions declared in this header are thread-safe and fully re-entrant when operating on
 * distinct argument structures. Multiple threads may compile separate circuits simultaneously.
 *
 * ## Example Usage
 * ```c
 * const char* circuit_json = "{\"elements\": [...], \"nets\": [...]}";
 * le_compile_result_t result;
 * int rc = le_compile_json(circuit_json, NULL, &result);
 * if (rc == 0 && result.success) {
 *     printf("Compilation succeeded: %zu bytes emitted\n", result.binary_size);
 *     // Access result.binary_data, result.disassembly_text, etc.
 * } else {
 *     fprintf(stderr, "Compilation failed: %s\n", result.error_message);
 * }
 * le_compile_result_free(&result);
 * ```
 */

#ifndef LE_COMPILER_H
#define LE_COMPILER_H

#include <stddef.h>
#include <stdint.h>
#include "le_compiler_options.h"

#ifdef _WIN32
  #if defined(LE_COMPILER_EXPORTS)
    #define LE_COMPILER_API __declspec(dllexport)
  #elif defined(LE_COMPILER_STATIC)
    #define LE_COMPILER_API
  #else
    #define LE_COMPILER_API __declspec(dllimport)
  #endif
#else
  #if defined(LE_COMPILER_EXPORTS)
    #define LE_COMPILER_API __attribute__((visibility("default")))
  #else
    #define LE_COMPILER_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compilation outcome container holding bytecode, C header, disassembly, and metrics.
 *
 * All pointer members (`error_message`, `warnings`, `binary_data`, `c_header_code`,
 * `disassembly_text`, `json_circuit`) are allocated on the heap by the compiler.
 * Call @ref le_compile_result_free to safely deallocate all allocated members.
 */
typedef struct {
    int success;               /**< 1 if compilation succeeded without errors; 0 on failure. */
    char* error_message;       /**< Heap-allocated error message describing failure, or NULL on success. */
    char* warnings;            /**< Heap-allocated newline-separated warning messages, or NULL if none. */

    uint8_t* binary_data;      /**< Heap-allocated buffer containing the compiled .lebin payload, or NULL. */
    size_t binary_size;        /**< Length of @ref binary_data in bytes. */

    char* c_header_code;       /**< Heap-allocated C source code string declaring an embedded static array. */
    char* disassembly_text;    /**< Heap-allocated formatted assembly listing string. */
    char* json_circuit;        /**< Heap-allocated canonical circuit JSON string after code block expansion. */

    /* Resource allocation statistics */
    int instruction_count;     /**< Total number of virtual machine instructions emitted. */
    int din_count;             /**< Total number of physical digital input channels mapped (%IN). */
    int dout_count;            /**< Total number of physical digital output channels mapped (%OUT). */
    int ain_count;             /**< Total number of physical analog input channels mapped (%AIN). */
    int aout_count;            /**< Total number of physical analog output channels mapped (%AOUT). */
    int bool_reg_count;        /**< Total number of boolean memory registers allocated (%B). */
    int float_count;           /**< Total number of 32-bit floating-point registers allocated (%F). */
    int complex_count;         /**< Total number of complex registers allocated (%C, real+imag pairs). */
    int int_count;             /**< Total number of 32-bit integer registers allocated (%N). */
    int timer_count;           /**< Total number of timer function blocks instantiated. */
    int counter_count;         /**< Total number of counter function blocks instantiated. */
    int alias_count;           /**< Total number of user register aliases embedded into the container. */
    uint32_t crc32;            /**< Standard IEEE 802.3 CRC-32 checksum of the emitted bytecode payload. */

    /* Register allocation breakdown */
    int user_bool_count;       /**< Count of explicit named boolean registers allocated. */
    int temp_bool_count;       /**< Count of reused transient intermediate boolean registers. */
    int user_float_count;      /**< Count of explicit named floating-point registers allocated. */
    int temp_float_count;      /**< Count of reused transient intermediate floating-point registers. */
    int user_complex_count;    /**< Count of explicit named complex registers allocated. */
    int temp_complex_count;    /**< Count of reused transient intermediate complex registers. */
    int user_int_count;        /**< Count of explicit named integer registers allocated. */
    int temp_int_count;        /**< Count of reused transient intermediate integer registers. */

    /* Optimization metrics */
    int eliminated_instructions; /**< Total instructions pruned by dead code elimination or constant folding. */
    int eliminated_registers;    /**< Intermediate registers coalesced by direct destination optimization. */

    /* Static worst-case timing analysis */
    uint32_t abstract_cycles;    /**< Worst-case cycle cost according to the target hardware timing model. */
    double estimated_exec_us;    /**< Estimated worst-case scan execution time in microseconds, or 0.0. */
} le_compile_result_t;

/**
 * @brief Compiles circuit JSON and target board profile into bytecode using custom optimization flags.
 *
 * @param[in] circuit_json Null-terminated UTF-8 JSON string declaring circuit elements and nets.
 *                         Must not be NULL.
 * @param[in] board_profile_json Optional null-terminated UTF-8 JSON string declaring board limits,
 *                               pin aliases, and custom nodes. May be NULL.
 * @param[in] options Pointer to compiler optimization flags, or NULL to use default @ref LE_OPT_FULL.
 * @param[out] out_result Pointer to structure receiving the compiled outputs and metrics.
 *                        Must not be NULL. Must be freed with @ref le_compile_result_free.
 *
 * @return Returns `0` on successful compilation.
 *         Returns `-1` if @p circuit_json or @p out_result is NULL.
 *         Returns `1` if the schematic fails validation or contains compilation errors.
 */
LE_COMPILER_API int le_compile_json_ex(
    const char* circuit_json,
    const char* board_profile_json,
    const le_compiler_options_t* options,
    le_compile_result_t* out_result
);

/**
 * @brief Compiles circuit JSON and target board profile into bytecode using standard -O2 optimizations.
 *
 * Equivalent to calling @ref le_compile_json_ex with default @ref le_compiler_options_init_default.
 *
 * @param[in] circuit_json Null-terminated UTF-8 JSON string declaring circuit elements and nets.
 * @param[in] board_profile_json Optional null-terminated UTF-8 JSON board profile string, or NULL.
 * @param[out] out_result Pointer to structure receiving the compilation outputs.
 *
 * @return Returns `0` on success; `-1` on argument error; `1` on compilation error.
 */
LE_COMPILER_API int le_compile_json(
    const char* circuit_json,
    const char* board_profile_json,
    le_compile_result_t* out_result
);

/**
 * @brief Disassembles binary bytecode into a formatted human-readable assembly listing.
 *
 * @param[in] bin_data Pointer to the raw bytecode container payload. Must not be NULL.
 * @param[in] bin_len Length of @p bin_data in bytes. Must be greater than 0.
 * @param[in] user_bool_count Number of named user boolean registers, or `-1` if unknown.
 * @param[in] user_float_count Number of named user float registers, or `-1` if unknown.
 * @param[in] user_int_count Number of named user integer registers, or `-1` if unknown.
 * @param[in] board_profile_json Optional board profile JSON string to annotate custom node calls, or NULL.
 * @param[out] out_disassembly Receives heap-allocated disassembly listing string on success.
 *                             Must be freed with @ref le_compiler_free_buffer.
 * @param[out] out_error_msg Receives heap-allocated error message string on failure, or NULL.
 *                           Must be freed with @ref le_compiler_free_buffer if non-NULL.
 *
 * @return Returns `0` on success; non-zero on parsing or decoding error.
 */
LE_COMPILER_API int le_disassemble_bin(
    const uint8_t* bin_data,
    size_t bin_len,
    int user_bool_count,
    int user_float_count,
    int user_int_count,
    const char* board_profile_json,
    char** out_disassembly,
    char** out_error_msg
);

/**
 * @brief Generates an embedded C static byte array header string from a binary container.
 *
 * @param[in] bin_data Pointer to binary bytecode payload. Must not be NULL.
 * @param[in] bin_len Length of @p bin_data in bytes. Must be greater than 0.
 * @param[in] array_name Identifier for the C array variable, or NULL to use `"le_default_program"`.
 * @param[out] out_c_header Receives heap-allocated C header source code string.
 *                          Must be freed with @ref le_compiler_free_buffer.
 *
 * @return Returns `0` on success; non-zero on failure.
 */
LE_COMPILER_API int le_export_c_header(
    const uint8_t* bin_data,
    size_t bin_len,
    const char* array_name,
    char** out_c_header
);

/**
 * @brief Cryptographically signs an extensible chunked container (.lebin v13 LEB2) using Ed25519.
 *
 * Computes a detached 64-byte Ed25519 signature across all preceding chunks and container headers,
 * appending a `"SIGN"` chunk.
 *
 * @param[in] in_container Pointer to the unsigned container buffer. Must not be NULL.
 * @param[in] in_size Size of @p in_container in bytes.
 * @param[in] secret_key 64-byte private/secret Ed25519 key (expanded seed + public key).
 * @param[out] out_container Destination buffer to receive the signed container.
 * @param[in] max_out_size Maximum capacity of @p out_container in bytes.
 * @param[out] out_size Pointer receiving the total signed bytes written to @p out_container.
 *
 * @return Returns `0` on success; non-zero if signing fails or destination buffer is too small.
 */
LE_COMPILER_API int le_compiler_sign_container(
    const uint8_t* in_container,
    size_t in_size,
    const uint8_t secret_key[64],
    uint8_t* out_container,
    size_t max_out_size,
    size_t* out_size
);

/**
 * @brief Generates an Ed25519 public/private keypair from a 32-byte cryptographic seed.
 *
 * @param[out] out_pk 32-byte destination buffer receiving the generated public key.
 * @param[out] out_sk 64-byte destination buffer receiving the expanded secret key.
 * @param[in] seed 32-byte cryptographic random seed.
 *
 * @return Returns `0` on success; non-zero on failure.
 */
LE_COMPILER_API int le_compiler_keygen(
    uint8_t out_pk[32],
    uint8_t out_sk[64],
    const uint8_t seed[32]
);

/**
 * @brief Deallocates all memory allocated inside a @ref le_compile_result_t container.
 *
 * Safely frees `error_message`, `warnings`, `binary_data`, `c_header_code`, `disassembly_text`,
 * and `json_circuit`, and resets all member fields to zero/NULL.
 *
 * @param[in,out] result Pointer to the result container to release. May be NULL (no-op).
 */
LE_COMPILER_API void le_compile_result_free(le_compile_result_t* result);

/**
 * @brief Deallocates a heap string or binary buffer allocated by compiler API functions.
 *
 * Use this function to release buffers returned by @ref le_disassemble_bin and @ref le_export_c_header.
 *
 * @param[in,out] ptr Pointer to the allocated memory buffer. May be NULL (no-op).
 */
LE_COMPILER_API void le_compiler_free_buffer(void* ptr);

/**
 * @brief Returns the version identifier string of the LogicElements compiler engine.
 *
 * @return Pointer to a static, null-terminated version string (e.g. `"13.0.0"`).
 */
LE_COMPILER_API const char* le_compiler_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPILER_H */

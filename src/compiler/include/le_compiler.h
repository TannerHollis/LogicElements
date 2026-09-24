/**
 * @file le_compiler.h
 * @brief Public C API for the canonical LogicElements circuit compiler and disassembler.
 */

#ifndef LE_COMPILER_H
#define LE_COMPILER_H

#include <stdint.h>
#include <stddef.h>
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
 */
typedef struct {
    int success;               /**< Non-zero if compilation succeeded; otherwise, 0. */
    char* error_message;       /**< Error message describing failure, or `NULL` on success. Free using @ref le_compile_result_free. */
    char* warnings;            /**< Warning messages concatenated by newline characters, or `NULL` if none. */

    uint8_t* binary_data;      /**< Pointer to the compiled .lebin bytecode payload, or `NULL` on error. */
    size_t binary_size;        /**< Size of @p binary_data in bytes. */

    char* c_header_code;       /**< Null-terminated C source code string containing embedded static array. */
    char* disassembly_text;    /**< Null-terminated formatted disassembly listing string. */
    char* json_circuit;        /**< Null-terminated canonical circuit JSON representation string. */

    /* Resource statistics */
    int instruction_count;     /**< Total number of virtual machine instructions generated. */
    int din_count;             /**< Total number of physical digital input pins mapped (%I). */
    int dout_count;            /**< Total number of physical digital output pins mapped (%Q). */
    int ain_count;             /**< Total number of analog input channels mapped. */
    int bool_reg_count;        /**< Total number of boolean memory registers allocated (%M). */
    int float_count;           /**< Total number of 32-bit floating-point registers allocated (%R). */
    int complex_count;         /**< Total number of complex registers allocated (%C, real+imag pairs). */
    int int_count;             /**< Total number of 32-bit integer registers allocated. */
    int timer_count;           /**< Total number of timer function blocks allocated. */
    int counter_count;         /**< Total number of counter function blocks allocated. */
int alias_count;           /**< Total number of user-declared register aliases packed into the binary. */
    uint32_t crc32;            /**< Calculated CRC-32 checksum of the bytecode payload. */

    /* Register allocation breakdown */
    int user_bool_count;       /**< Number of explicit named boolean registers allocated. */
    int temp_bool_count;       /**< Number of transient intermediate boolean registers reused. */
    int user_float_count;      /**< Number of explicit named floating-point registers allocated. */
    int temp_float_count;      /**< Number of transient intermediate floating-point registers reused. */
    int user_complex_count;    /**< Number of explicit named complex registers allocated. */
    int temp_complex_count;    /**< Number of transient intermediate complex registers reused. */
    int user_int_count;        /**< Number of explicit named integer registers allocated. */
    int temp_int_count;        /**< Number of transient intermediate integer registers reused. */

    /* Optimization statistics */
    int eliminated_instructions; /**< Number of instructions removed by optimization passes. */
    int eliminated_registers;    /**< Number of intermediate registers eliminated by optimization passes. */

    /* Timing analysis (deterministic fixed-rate execution) */
    uint32_t abstract_cycles;    /**< Worst-case abstract cost of the emitted program (timing descriptor). */
    double   estimated_exec_us;  /**< Estimated worst-case scan time on the target board (with compiler safety margin), or 0 when the board profile provides no cost model. */
} le_compile_result_t;

/**
 * @brief Compiles circuit JSON and optional board profile JSON into bytecode using specified optimization options.
 *
 * @param circuit_json Null-terminated UTF-8 JSON string declaring circuit elements and nets.
 * @param board_profile_json Optional null-terminated UTF-8 JSON string declaring target board limits and pin aliases (or `NULL`).
 * @param options Pointer to optimization switches and passes, or `NULL` to use default options (-O2).
 * @param out_result Pointer to structure receiving the compilation outputs. Free with @ref le_compile_result_free.
 * @return Returns `0` on success; otherwise, returns a non-zero error code.
 */
LE_COMPILER_API int le_compile_json_ex(
    const char* circuit_json,
    const char* board_profile_json,
    const le_compiler_options_t* options,
    le_compile_result_t* out_result
);

/**
 * @brief Compiles circuit JSON and optional board profile JSON into bytecode using default -O2 optimizations.
 *
 * @param circuit_json Null-terminated UTF-8 JSON string declaring circuit elements and nets.
 * @param board_profile_json Optional null-terminated UTF-8 JSON string declaring target board limits and pin aliases (or `NULL`).
 * @param out_result Pointer to structure receiving the compilation outputs. Free with @ref le_compile_result_free.
 * @return Returns `0` on success; otherwise, returns a non-zero error code.
 */
LE_COMPILER_API int le_compile_json(
    const char* circuit_json,
    const char* board_profile_json,
    le_compile_result_t* out_result
);

/**
 * @brief Disassembles compiled .lebin bytecode into a human-readable assembly listing.
 *
 * @param bin_data Pointer to the raw binary bytecode payload.
 * @param bin_len Length of @p bin_data in bytes.
 * @param user_bool_count Number of named user boolean registers (`-1` if unknown).
 * @param user_float_count Number of named user float registers (`-1` if unknown).
 * @param user_int_count Number of named user integer registers (`-1` if unknown).
 * @param board_profile_json Optional board profile JSON used to annotate custom
 *        board nodes in the output. Pass NULL to omit custom-node info.
 * @param out_disassembly Output pointer receiving heap-allocated disassembly string. Free with @ref le_compiler_free_buffer.
 * @param out_error_msg Output pointer receiving heap-allocated error message on failure. Free with @ref le_compiler_free_buffer.
 * @return Returns `0` on success; otherwise, returns a non-zero error code.
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
 * @brief Exports .lebin bytecode as an embedded C static byte array header string.
 *
 * @param bin_data Pointer to the raw binary bytecode payload.
 * @param bin_len Length of @p bin_data in bytes.
 * @param array_name Variable identifier for the generated static byte array (or `NULL` to use default).
 * @param out_c_header Output pointer receiving heap-allocated C header string. Free with @ref le_compiler_free_buffer.
 * @return Returns `0` on success; otherwise, returns a non-zero error code.
 */
LE_COMPILER_API int le_export_c_header(
    const uint8_t* bin_data,
    size_t bin_len,
    const char* array_name,
    char** out_c_header
);

/**
 * @brief Releases all memory allocated inside a @ref le_compile_result_t container.
 *
 * @param result Pointer to the result container to release.
 */
LE_COMPILER_API void le_compile_result_free(le_compile_result_t* result);

/**
 * @brief Releases a heap buffer allocated by compiler API functions.
 *
 * @param ptr Pointer to the allocated memory buffer.
 */
LE_COMPILER_API void le_compiler_free_buffer(void* ptr);

/**
 * @brief Returns the version identifier string of the LogicElements compiler.
 *
 * @return Pointer to a static, null-terminated version string.
 */
LE_COMPILER_API const char* le_compiler_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPILER_H */

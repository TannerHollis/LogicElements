/**
 * @file le_compiler_core.h
 * @brief Canonical compiler core engine and register allocator for LogicElements.
 */

#ifndef LE_COMPILER_CORE_H
#define LE_COMPILER_CORE_H

#include "le_compiler.h"
#include "le_compiler_types.h"
#include "le_json.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace LogicElements {

/**
 * @brief Core compilation and code generation engine.
 */
class CompilerCore {
public:
    /**
     * @brief Constructs a new CompilerCore instance.
     */
    CompilerCore();

    /**
     * @brief Compiles circuit JSON and board profile strings using default optimizations.
     *
     * @param circuit_json_str Circuit schematic JSON string.
     * @param board_json_str Target board profile JSON string (or empty).
     * @param out_result Output pointer receiving the compilation outputs.
     * @return Returns `0` on success; otherwise, returns a non-zero error code.
     */
    int compile(const std::string& circuit_json_str,
                const std::string& board_json_str,
                le_compile_result_t* out_result);

    /**
     * @brief Compiles circuit JSON and board profile strings using explicit optimization options.
     *
     * @param circuit_json_str Circuit schematic JSON string.
     * @param board_json_str Target board profile JSON string (or empty).
     * @param options Explicit optimization configuration flags.
     * @param out_result Output pointer receiving the compilation outputs.
     * @return Returns `0` on success; otherwise, returns a non-zero error code.
     */
    int compile_ex(const std::string& circuit_json_str,
                   const std::string& board_json_str,
                   const le_compiler_options_t& options,
                   le_compile_result_t* out_result);

    /**
     * @brief Computes standard IEEE 802.3 CRC-32 checksum for bytecode payload verification.
     *
     * @param data Pointer to input data buffer.
     * @param length Number of bytes to hash.
     * @return Computed 32-bit CRC value.
     */
    static uint32_t compute_crc32(const uint8_t* data, size_t length);

    /**
     * @brief Disassembles binary bytecode into a formatted text listing.
     *
     * @param bin_data Pointer to binary bytecode payload.
     * @param bin_len Length of @p bin_data in bytes.
     * @param user_bool_count Number of user boolean registers (`-1` if unknown).
     * @param user_float_count Number of user float registers (`-1` if unknown).
     * @param user_int_count Number of user integer registers (`-1` if unknown).
     * @return Formatted disassembly string.
     */
    static std::string disassemble(const uint8_t* bin_data,
                                   size_t bin_len,
                                   int user_bool_count = -1,
                                   int user_float_count = -1,
                                   int user_int_count = -1);

    /**
     * @brief Generates an embedded C static byte array header string.
     *
     * @param bin_data Pointer to binary bytecode payload.
     * @param bin_len Length of @p bin_data in bytes.
     * @param array_name Variable name for the generated static array.
     * @param custom_headers List of custom C header includes.
     * @param scalers List of configured analog scaler blocks.
     * @return Complete C header source code.
     */
    static std::string export_c_header(const uint8_t* bin_data,
                                       size_t bin_len,
                                       const std::string& array_name = "le_default_program",
                                       const std::vector<std::string>& custom_headers = {},
                                       const std::vector<ScalerInfo>& scalers = {});

    /* ====================================================================== */
    /* Variable resolution (used by property reads and expression evaluation). */
    /* ====================================================================== */

    /* Variable tables (populated by build_variables from the circuit JSON). */
    std::map<std::string, double>  m_var_values;      /* resolved variable values */
    std::map<std::string, std::string> m_var_exprs;   /* raw indirect definitions */
    std::map<std::string, int>     m_var_state;       /* 0=unseen 1=in-progress 2=done */
    std::string m_var_error;                          /* first unresolved/cycle error */
    /* Evaluate a numeric property: number, "%VAR%", or arithmetic expression. */
    double prop(const JsonValue& element, const std::string& key, double default_value);
    /* Populate + resolve the circuit's "variables" table. */
    int build_variables(const JsonValue& circuit_doc);
    /* Internal: parse+eval an expression over variables and literals. */
    static double eval_expr(const std::string& expr, const std::map<std::string,double>& var_values);

private:
    uint16_t allocate_user_bool();
    uint16_t allocate_user_int();
    uint16_t allocate_user_float();
    uint16_t allocate_user_complex();
    uint16_t allocate_timer();
    uint16_t allocate_counter();

    uint16_t acquire_temp_bool(int& out_temp_idx);
    void release_temp_bool(int temp_idx);
    uint16_t acquire_temp_float(int& out_temp_idx);
    void release_temp_float(int temp_idx);
    uint16_t acquire_temp_complex(int& out_temp_idx);
    void release_temp_complex(int temp_idx);
    uint16_t acquire_temp_int(int& out_temp_idx);
    void release_temp_int(int temp_idx);

    int m_user_bool_count = 0;
    int m_user_int_count = 0;
    int m_user_float_count = 0;
    int m_user_complex_count = 0;
    int m_peak_temp_bool = 0;
    int m_peak_temp_float = 0;
    int m_peak_temp_complex = 0;
    int m_peak_temp_int = 0;
    int m_timer_counter = 0;
    int m_counter_counter = 0;

    std::vector<int> m_free_temp_bool_pool;
    std::vector<int> m_free_temp_float_pool;
    std::vector<int> m_free_temp_complex_pool;
    std::vector<int> m_free_temp_int_pool;
};

} // namespace LogicElements

#endif /* LE_COMPILER_CORE_H */

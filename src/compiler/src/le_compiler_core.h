/**
 * @file le_compiler_core.h
 * @brief Canonical compiler core engine and virtual machine register allocator for LogicElements.
 *
 * This module coordinates the entire circuit synthesis pipeline:
 * 1. JSON parsing and schema validation.
 * 2. Symbolic variable table (`%VAR%`) resolution with topological evaluation.
 * 3. Board profile limit checks and pin alias mapping.
 * 4. Code block (`"CODE_BLOCK"`) frontend expansion.
 * 5. Algebraic cycle and feedback loop detection.
 * 6. Multi-pass IR optimization via @ref Optimizer.
 * 7. Liveness analysis and temporary register reuse.
 * 8. Opcode instruction emission and state table generation.
 * 9. LEB2 container serialization and CRC-32 checksum calculation.
 */

#ifndef LE_COMPILER_CORE_H
#define LE_COMPILER_CORE_H

#include "le_compiler.h"
#include "le_compiler_types.h"
#include "le_json.h"
#include "le_register_allocator.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Canonical circuit compilation and code generation engine.
 *
 * Encapsulates the compilation state, including user register counters, transient
 * temporary register pools, variable resolution tables, and output buffers.
 */
class CompilerCore {
public:
    /**
     * @brief Constructs a new CompilerCore instance with zeroed allocation counters.
     */
    CompilerCore();

    /**
     * @brief Compiles circuit JSON and board profile strings using default @ref LE_OPT_FULL optimizations.
     *
     * @param[in] circuit_json_str Circuit schematic JSON string.
     * @param[in] board_json_str Target board profile JSON string, or empty string.
     * @param[out] out_result Output pointer receiving the compilation outputs and metrics.
     * @return `0` on success; non-zero on compilation error.
     */
    int compile(
        const std::string& circuit_json_str,
        const std::string& board_json_str,
        le_compile_result_t* out_result
    );

    /**
     * @brief Compiles circuit JSON and board profile strings using explicit optimization options.
     *
     * @param[in] circuit_json_str Circuit schematic JSON string.
     * @param[in] board_json_str Target board profile JSON string, or empty string.
     * @param[in] options Explicit optimization configuration flags.
     * @param[out] out_result Output pointer receiving the compilation outputs and metrics.
     * @return `0` on success; non-zero on compilation error.
     */
    int compile_ex(
        const std::string& circuit_json_str,
        const std::string& board_json_str,
        const le_compiler_options_t& options,
        le_compile_result_t* out_result
    );

    /**
     * @brief Computes standard IEEE 802.3 CRC-32 checksum for bytecode payload verification.
     *
     * @param[in] data Pointer to input data buffer.
     * @param[in] length Number of bytes to hash.
     * @return Computed 32-bit CRC value.
     */
    [[nodiscard]] static uint32_t compute_crc32(const uint8_t* data, size_t length);

    /**
     * @brief Disassembles binary bytecode into a formatted text listing.
     *
     * @param[in] bin_data Pointer to binary bytecode payload.
     * @param[in] bin_len Length of @p bin_data in bytes.
     * @param[in] user_bool_count Number of user boolean registers, or `-1` if unknown.
     * @param[in] user_float_count Number of user float registers, or `-1` if unknown.
     * @param[in] user_int_count Number of user integer registers, or `-1` if unknown.
     * @return Formatted disassembly string.
     */
    [[nodiscard]] static std::string disassemble(
        const uint8_t* bin_data,
        size_t bin_len,
        int user_bool_count = -1,
        int user_float_count = -1,
        int user_int_count = -1
    );

    /**
     * @brief Generates an embedded C static byte array header string.
     *
     * @param[in] bin_data Pointer to binary bytecode payload.
     * @param[in] bin_len Length of @p bin_data in bytes.
     * @param[in] array_name Variable name for the generated static array.
     * @param[in] custom_headers List of custom C header includes.
     * @param[in] scalers List of configured analog scaler blocks.
     * @return Complete C header source code.
     */
    [[nodiscard]] static std::string export_c_header(
        const uint8_t* bin_data,
        size_t bin_len,
        const std::string& array_name = "le_default_program",
        const std::vector<std::string>& custom_headers = {},
        const std::vector<ScalerInfo>& scalers = {}
    );

    /* ====================================================================== */
    /* Variable resolution (used by property reads and expression evaluation). */
    /* ====================================================================== */

    std::map<std::string, double> m_var_values;     /**< Resolved numeric variable values. */
    std::map<std::string, std::string> m_var_exprs;  /**< Raw unparsed indirect variable formulas. */
    std::map<std::string, int> m_var_state;          /**< Cycle detection state: 0=unseen, 1=in-progress, 2=done. */
    std::string m_var_error;                         /**< Error message describing variable resolution or cycle failure. */

    /**
     * @brief Evaluates a numeric schematic property, resolving numbers, "%VAR%" references, or math expressions.
     *
     * @param[in] element JSON node representing the schematic element.
     * @param[in] key Property name identifier.
     * @param[in] default_value Fallback value if the property is absent.
     * @return Resolved double-precision numeric value.
     */
    double prop(const JsonValue& element, const std::string& key, double default_value);

    /**
     * @brief Populates and topologically resolves the circuit's `"variables"` dictionary.
     *
     * @param[in] circuit_doc Root circuit JSON document.
     * @return `0` on success; non-zero if an unresolved reference or circular cycle is detected.
     */
    int build_variables(const JsonValue& circuit_doc);

    /**
     * @brief Evaluates an arithmetic expression containing variable references and numeric constants.
     *
     * @param[in] expr Expression string to evaluate (e.g. `"%GAIN% * 2.5"`).
     * @param[in] var_values Map of resolved variable identifiers to double values.
     * @return Computed double value.
     */
    [[nodiscard]] static double eval_expr(const std::string& expr, const std::map<std::string, double>& var_values);

    /**
     * @brief Parses an optional board profile JSON document and extracts configuration and pin limits.
     *
     * @param[in] board_json_str Raw JSON string containing board profile, or empty.
     * @param[in] requested_target_version Target container version requested by options.
     * @param[out] out_board_doc Output parsed board JSON document.
     * @return Populated @ref BoardProfile descriptor.
     */
    [[nodiscard]] static BoardProfile parse_board_profile(
        const std::string& board_json_str,
        uint16_t requested_target_version,
        JsonValue& out_board_doc
    );

    /**
     * @brief Deduces the logical data type of a circuit element.
     *
     * @param[in] node_name Name of the target element.
     * @param[in] elem_by_name Lookup map of elements by name.
     * @param[in] custom_nodes Board custom node definitions.
     * @return `0` for Bool, `1` for Float, `2` for Int, `3` for Complex.
     */
    [[nodiscard]] static int deduce_element_type(
        const std::string& node_name,
        const std::map<std::string, JsonValue>& elem_by_name,
        const std::map<std::string, CustomNodeInfo>& custom_nodes
    );

    /**
     * @brief Detects feedback loops, algebraic cycles, and combinatorial ring oscillators in the circuit netlist.
     *
     * @param[in] elements Array of circuit element JSON objects.
     * @param[in] nets Array of circuit net JSON objects.
     * @param[in] elem_by_name Map of elements indexed by name.
     * @param[in] custom_nodes Board custom node definitions.
     * @param[in,out] element_outputs Map from element name to canonical allocated register address.
     * @param[out] feedback_state_nodes Set of node names requiring persistent state registers.
     * @param[out] oscillator_warnings List of generated hazard warning messages.
     */
    void detect_cycles_and_oscillators(
        const std::vector<JsonValue>& elements,
        const std::vector<JsonValue>& nets,
        const std::map<std::string, JsonValue>& elem_by_name,
        const std::map<std::string, CustomNodeInfo>& custom_nodes,
        std::map<std::string, uint16_t>& element_outputs,
        std::set<std::string>& feedback_state_nodes,
        std::vector<std::string>& oscillator_warnings
    );

    /**
     * @brief Resolves schematic and board aliases to process-image register addresses.
     *
     * @param[in] circuit_doc Circuit JSON document.
     * @param[in] elements Circuit elements array.
     * @param[in] element_outputs Map from element name to allocated register address.
     * @param[in] board_alias_to_addr Board pin alias mapping.
     * @param[out] out_aliases Vector of resolved `le_alias_t` descriptors.
     * @param[out] out_error Error message if validation fails.
     * @return `0` on success; non-zero on validation error.
     */
    [[nodiscard]] static int resolve_circuit_aliases(
        const JsonValue& circuit_doc,
        const std::vector<JsonValue>& elements,
        const std::map<std::string, uint16_t>& element_outputs,
        const std::map<std::string, std::string>& board_alias_to_addr,
        std::vector<le_alias_t>& out_aliases,
        std::string& out_error
    );

    /**
     * @brief Builds non-volatile metadata (NVMD) descriptor and coalesces retentive register ranges.
     *
     * @param[in] circuit_doc Circuit JSON document.
     * @param[in] board_doc Board profile JSON document.
     * @param[in] elements Circuit elements array.
     * @param[in] element_outputs Canonical element output addresses.
     * @param[in] element_outputs_port Multi-port element output addresses.
     * @param[in] alias_to_addr Board pin alias mapping.
     * @param[out] out_ranges Vector of coalesced `le_nvram_range_t` range descriptors.
     * @param[out] out_nvmd Populated runtime NVRAM descriptor.
     * @return `true` if NVRAM retentive configuration was present; `false` otherwise.
     */
    [[nodiscard]] static bool build_nvram_descriptor(
        const JsonValue& circuit_doc,
        const JsonValue& board_doc,
        const std::vector<JsonValue>& elements,
        const std::map<std::string, uint16_t>& element_outputs,
        const std::map<std::string, std::map<std::string, uint16_t>>& element_outputs_port,
        const std::map<std::string, std::string>& alias_to_addr,
        std::vector<le_nvram_range_t>& out_ranges,
        le_nvram_desc_t& out_nvmd
    );

    /**
     * @brief Validates compiled circuit metrics and register counts against target board limits.
     *
     * @param[in] board Parsed board profile descriptor.
     * @param[in] board_doc Parsed board profile JSON document.
     * @param[in] options Compiler options.
     * @param[in] has_nvmd True if circuit uses NVRAM manifest.
     * @param[in] num_coalesced_ranges Number of disjoint retentive ranges.
     * @param[in] din_count Number of digital input channels used.
     * @param[in] max_din_idx Maximum digital input channel index used.
     * @param[in] dout_count Number of digital output channels used.
     * @param[in] max_dout_idx Maximum digital output channel index used.
     * @param[in] ain_count Number of analog input channels used.
     * @param[in] max_ain_idx Maximum analog input channel index used.
     * @param[in] aout_count Number of analog output channels used.
     * @param[in] max_aout_idx Maximum analog output channel index used.
     * @param[in] total_bool_regs Total boolean registers allocated.
     * @param[in] total_float_regs Total float registers allocated.
     * @param[in] total_int_regs Total integer registers allocated.
     * @param[in] uses_protection True if ANSI protection functions are used.
     * @param[in] uses_serial_bus True if I2C/SPI serial buses are used.
     * @param[in] uses_dsp True if DSP filter/math blocks are used.
     * @param[in] total_bin_size Size of binary bytecode payload in bytes.
     * @param[out] out_errors List of validation error messages.
     * @return `true` if all constraints are satisfied; `false` on limit violations.
     */
    [[nodiscard]] static bool validate_board_limits(
        const BoardProfile& board,
        const JsonValue& board_doc,
        const le_compiler_options_t& options,
        const le_header_t& header,
        bool has_nvmd,
        const le_nvram_desc_t& nvmd,
        size_t num_coalesced_ranges,
        size_t din_count, int max_din_idx,
        size_t dout_count, int max_dout_idx,
        size_t ain_count, int max_ain_idx,
        size_t aout_count, int max_aout_idx,
        int total_bool_regs,
        int total_float_regs,
        int total_int_regs,
        int total_complex_regs,
        const std::map<uint8_t, int>& state_descs,
        bool uses_protection,
        bool uses_serial_bus,
        bool uses_dsp,
        size_t total_bin_size,
        std::vector<std::string>& out_errors
    );

private:
    /* Register Allocator */
    RegisterAllocator m_reg_alloc;

    /* Register Allocator: User Registers */
    uint16_t allocate_user_bool();
    uint16_t allocate_user_int();
    uint16_t allocate_user_float();
    uint16_t allocate_user_complex();

    /* Register Allocator: Transient Temporaries with Pool Reuse */
    uint16_t acquire_temp_bool(int& out_temp_idx);
    void release_temp_bool(int temp_idx);
    uint16_t acquire_temp_float(int& out_temp_idx);
    void release_temp_float(int temp_idx);
    uint16_t acquire_temp_complex(int& out_temp_idx);
    void release_temp_complex(int temp_idx);
    uint16_t acquire_temp_int(int& out_temp_idx);
    void release_temp_int(int temp_idx);
};

} // namespace LogicElements

#endif /* LE_COMPILER_CORE_H */

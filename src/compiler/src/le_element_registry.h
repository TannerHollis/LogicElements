/**
 * @file le_element_registry.h
 * @brief Central element metadata registry and traits database for the LogicElements compiler.
 *
 * Provides a single source of truth for element type identifiers, aliases, opcodes, operand
 * data types, input/output port definitions, and stateful block kinds. Replaces duplicated
 * element categorization logic across CompilerCore, Optimizer, and Disassembler.
 */

#ifndef LE_ELEMENT_REGISTRY_H
#define LE_ELEMENT_REGISTRY_H

#include "le_opcodes.h"
#include "le_types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace LogicElements {

/**
 * @brief Primary data type representation produced or consumed by a schematic element.
 */
enum class ElemDataType {
    Bool = 0,     /**< 1-bit discrete boolean. */
    Float = 1,    /**< 32-bit single-precision float. */
    Int = 2,      /**< 32-bit signed integer. */
    Complex = 3,  /**< Complex float pair (real + imag). */
    Void = 4      /**< Untyped, multi-output, or custom node. */
};

/**
 * @brief High-level classification governing dead code elimination and execution scheduling.
 */
enum class ElemCategory {
    Input,         /**< External input pin (%IN, %AIN). */
    Output,        /**< External output pin (%OUT, %AOUT); always treated as an execution root. */
    Combinatorial, /**< Pure stateless logic gate or math operation. */
    StatefulBlock, /**< Stateful DSP, protection, timer, or counter block with persistent RAM state. */
    Register,      /**< Explicit named memory register (%B, %N, %F, %C). */
    Tag,           /**< Cross-schematic signal transmitter or receiver. */
    Custom         /**< Target board-specific BSP external function call (LE_OP_EXT_CALL). */
};

/**
 * @brief Metadata descriptor defining an element's hardware traits and compilation semantics.
 */
struct ElementTraits {
    std::string canonical_type;      /**< Canonical type identifier (e.g. "AND", "ADD", "TON"). */
    std::vector<std::string> aliases;/**< Accepted type aliases (e.g. "LE_AND", "MULTIPLY"). */
    uint8_t opcode = LE_OP_NOP;      /**< Default target virtual machine opcode. */
    ElemDataType data_type = ElemDataType::Bool; /**< Default primary output data type. */
    ElemCategory category = ElemCategory::Combinatorial; /**< Functional classification. */
    bool is_stateful = false;        /**< True if the element requires persistent state across scan cycles. */
    uint8_t state_kind = LE_BLK_NONE;/**< Runtime state descriptor kind (e.g. @ref LE_BLK_TIMER). */
    uint32_t default_cycles = 1;     /**< Nominal abstract execution cycle cost for timing analysis. */
};

/**
 * @brief Static query registry mapping type names and aliases to @ref ElementTraits.
 */
class ElementRegistry {
public:
    /**
     * @brief Looks up traits by element type name or alias.
     *
     * Lookup is case-insensitive and trims leading/trailing whitespace.
     *
     * @param[in] type_name Element type string (e.g. "AND", "le_and", "ADD_F").
     * @return Pointer to matching @ref ElementTraits, or `nullptr` if unmapped.
     */
    [[nodiscard]] static const ElementTraits* lookup(std::string_view type_name) noexcept;

    /**
     * @brief Determines the primary output data type for an element type.
     *
     * @param[in] type_name Element type identifier.
     * @return Resolved @ref ElemDataType (defaults to @ref ElemDataType::Bool if unknown).
     */
    [[nodiscard]] static ElemDataType get_data_type(std::string_view type_name) noexcept;

    /**
     * @brief Determines if an element maintains persistent state across scan cycles.
     *
     * @param[in] type_name Element type identifier.
     * @return True if stateful; false otherwise.
     */
    [[nodiscard]] static bool is_stateful(std::string_view type_name) noexcept;

    /**
     * @brief Determines whether a node must be treated as an execution root during DCE.
     *
     * Outputs, stateful blocks, registers, and tag senders are execution roots.
     *
     * @param[in] type_name Element type identifier.
     * @param[in] tag_direction Optional direction ("send" or "recv") if the element is a TAG.
     * @return True if the node is a root sink that must not be pruned.
     */
    [[nodiscard]] static bool is_root(std::string_view type_name, std::string_view tag_direction = "") noexcept;

    /**
     * @brief Returns the default opcode for an element type.
     *
     * @param[in] type_name Element type identifier.
     * @return Opcode identifier (e.g. `LE_OP_AND`), or `LE_OP_NOP` if unmapped.
     */
    [[nodiscard]] static uint8_t get_opcode(std::string_view type_name) noexcept;

    /**
     * @brief Returns the optimizer opcode used during IR construction in Optimizer::build_ir.
     *
     * Maps combinatorial logic gates, math operations, and single-output DSP blocks to their
     * corresponding VM opcodes. Timers, counters, latches, registers, and external I/O terminals
     * return LE_OP_NOP so they are not treated as basic combinatorial producers.
     *
     * @param[in] type_name Element type identifier.
     * @return Opcode identifier for optimization, or LE_OP_NOP if not a simple combinatorial producer.
     */
    [[nodiscard]] static uint8_t get_optimizer_opcode(std::string_view type_name) noexcept;

    /**
     * @brief Returns the runtime block kind identifier for stateful elements.
     *
     * @param[in] type_name Element type identifier.
     * @return State block kind (e.g. `LE_BLK_TIMER`, `LE_BLK_PID`), or `LE_BLK_NONE`.
     */
    [[nodiscard]] static uint8_t get_state_kind(std::string_view type_name) noexcept;

private:
    static std::string normalize_type(std::string_view s);
};

} // namespace LogicElements

#endif /* LE_ELEMENT_REGISTRY_H */

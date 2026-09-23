/**
 * @file le_optimizer.h
 * @brief Multi-pass optimization pipeline for the canonical LogicElements compiler.
 */

#ifndef LE_OPTIMIZER_H
#define LE_OPTIMIZER_H

#include "le_compiler_options.h"
#include "le_compiler_types.h"
#include "le_json.h"
#include "le_types.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

namespace LogicElements {

/**
 * @brief Intermediate representation node representing an individual logic block or wire.
 */
struct OptNode {
    std::string name;                          /**< Unique node identifier. */
    std::string type;                          /**< Element type identifier (e.g. "AND", "NOT", "DIGITALINPUT"). */
    uint8_t opcode = LE_OP_NOP;                /**< Target virtual machine opcode. */
    uint8_t modifier = 0;                      /**< Opcode modifier flags or custom function identifier. */

    std::map<std::string, std::string> inputs; /**< Map of input terminal name to driving source node name. */
    std::vector<std::string> consumers;        /**< List of downstream node names consuming this node's output. */

    /* Constant values */
    bool is_const = false;                     /**< Indicates whether this node evaluates to a compile-time constant. */
    std::string const_type;                    /**< Constant data type ("bool", "float", "int"). */
    bool const_bool = false;                   /**< Boolean constant value. */
    float const_float = 0.0f;                  /**< Floating-point constant value. */
    int const_int = 0;                         /**< Integer constant value. */

    /* Forwarding */
    std::string forwarded_to;                  /**< Source node name to forward to if this node was folded. */
    uint8_t forward_modifier_invert = 0;       /**< Inversion modifier bit to carry when forwarded. */

    /* Reachability and optimization state */
    bool is_dead = false;                      /**< True if this node was determined to be unreachable. */
    bool is_root = false;                      /**< True if this node is an anchor (e.g. DOUT, stateful block). */
    bool is_stateful = false;                  /**< True if this node contains internal state across cycles. */
    bool direct_dest_eliminated = false;       /**< True if the consumer MOVE instruction was coalesced. */
    std::string direct_dest_node;              /**< Target destination pin or register name. */

    JsonValue raw_json;                        /**< Original JSON element payload. */
};

/**
 * @brief Metrics recorded during optimization passes.
 */
struct OptimizationStats {
    int eliminated_instructions = 0;           /**< Total number of instructions removed. */
    int eliminated_registers = 0;              /**< Total number of temporary registers eliminated. */
    int folded_inversions = 0;                 /**< Total number of NOT gates folded into consumer operands. */
    int folded_constants = 0;                  /**< Total number of constant expressions evaluated. */
    int eliminated_dead_nodes = 0;             /**< Total number of unreferenced dead nodes pruned. */
    int eliminated_moves = 0;                  /**< Total number of redundant MOVE instructions eliminated. */
    int eliminated_subexprs = 0;               /**< Total number of duplicate common subexpressions merged. */
};

/**
 * @brief Multi-pass optimization pipeline.
 */
class Optimizer {
public:
    /**
     * @brief Constructs an Optimizer with the specified optimization options.
     *
     * @param options Optimization switches and iteration limits.
     */
    Optimizer(const le_compiler_options_t& options);

    /**
     * @brief Builds intermediate representation from schematic elements and nets.
     *
     * @param elements List of parsed element JSON objects.
     * @param nets List of parsed net JSON objects.
     * @param alias_to_addr Mapping of pin aliases to physical address strings.
     * @param custom_nodes Mapping of custom node type identifiers to node definitions.
     */
    void build_ir(const std::vector<JsonValue>& elements,
                  const std::vector<JsonValue>& nets,
                  const std::map<std::string, std::string>& alias_to_addr,
                  const std::map<std::string, CustomNodeInfo>& custom_nodes);

    /**
     * @brief Executes all configured optimization passes until a fixed point or iteration limit is reached.
     *
     * @return Accumulated metrics describing the optimization outcome.
     */
    OptimizationStats run_passes();

    /**
     * @brief Returns the optimized topological execution order of nodes.
     *
     * @return Reference to the vector of node names in execution order.
     */
    const std::vector<std::string>& get_optimized_order() const { return m_execution_order; }

    /**
     * @brief Returns the complete map of intermediate representation nodes.
     *
     * @return Reference to the node dictionary.
     */
    const std::map<std::string, OptNode>& get_nodes() const { return m_nodes; }

    /**
     * @brief Retrieves a mutable reference to an individual intermediate representation node.
     *
     * @param name Node identifier string.
     * @return Reference to the requested node.
     */
    OptNode& get_node(const std::string& name) { return m_nodes[name]; }

private:
    void pass_constant_folding();
    void pass_dead_code_elimination();
    void pass_common_subexpression_elimination();
    void pass_inversion_folding();
    void pass_direct_destination_coalescing();
    void update_consumer_lists();
    void compute_topological_order();

    le_compiler_options_t m_options;
    std::map<std::string, OptNode> m_nodes;
    std::vector<std::string> m_execution_order;
    OptimizationStats m_stats;
};

} // namespace LogicElements

#endif /* LE_OPTIMIZER_H */

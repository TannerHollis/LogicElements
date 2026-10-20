/**
 * @file le_optimizer.h
 * @brief Multi-pass optimization pipeline and intermediate representation for the LogicElements compiler.
 *
 * This module transforms the raw schematic netlist into an optimized Directed Acyclic Graph (DAG)
 * of @ref OptNode structures. It applies algebraic rewrites, constant propagation, dead code pruning,
 * hardware modifier folding, and direct destination coalescing to minimize bytecode size and execution cycles.
 *
 * ## Optimization Invariants
 * - **Deterministic Fixed-Point**: Optimization passes run iteratively until no further transformations
 *   occur, or until reaching @ref le_compiler_options_t::max_pass_iterations.
 * - **Semantic Equivalence**: Optimized bytecode preserves identical observable outputs on all physical
 *   output pins and state registers across every execution scan cycle.
 * - **Feedback Cycle Preservation**: Nodes marked with @ref OptNode::is_feedback_state are treated as
 *   roots to prevent pruning memory state required by unit-delay feedback loops.
 */

#ifndef LE_OPTIMIZER_H
#define LE_OPTIMIZER_H

#include "le_compiler_options.h"
#include "le_compiler_types.h"
#include "le_json.h"
#include "le_types.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Intermediate representation node representing an individual logic block, math gate, or wire.
 */
struct OptNode {
    std::string name;                          /**< Unique schematic node identifier. */
    std::string type;                          /**< Canonical element type identifier (e.g. "AND", "NOT", "DIGITALINPUT"). */
    uint8_t opcode = LE_OP_NOP;                /**< Target virtual machine opcode mapped to this node. */
    uint8_t modifier = 0;                      /**< Opcode modifier flags (inversion, rate domain, or function ID). */

    std::map<std::string, std::string> inputs; /**< Map of input port identifier to driving source node name. */
    std::vector<std::string> consumers;        /**< List of downstream consumer node names dependent on this node. */

    /* Constant evaluation attributes */
    bool is_const = false;                     /**< True if this node evaluates to a compile-time constant. */
    std::string const_type;                    /**< Constant data type descriptor: "bool", "float", or "int". */
    bool const_bool = false;                   /**< Boolean constant value if @ref const_type is "bool". */
    float const_float = 0.0f;                  /**< Floating-point constant value if @ref const_type is "float". */
    int const_int = 0;                         /**< Integer constant value if @ref const_type is "int". */

    /* Forwarding and substitution state */
    std::string forwarded_to;                  /**< Replacement source node name if this node was folded or alias-forwarded. */
    uint8_t forward_modifier_invert = 0;       /**< Inversion modifier bit carried forward when folded. */

    /* Reachability and lifetime state */
    bool is_dead = false;                      /**< True if pruned by Dead Code Elimination. */
    bool is_root = false;                      /**< True if designated as an execution root (e.g. physical outputs, stateful blocks). */
    bool is_stateful = false;                  /**< True if the node maintains persistent internal state across scan cycles. */
    bool is_feedback_state = false;            /**< True if allocated to break a cyclic feedback loop with unit delay. */
    bool direct_dest_eliminated = false;       /**< True if consumer MOVE instruction was coalesced directly into this node. */
    std::string direct_dest_node;              /**< Target destination pin or register identifier if coalesced. */

    JsonValue raw_json;                        /**< Original JSON element specification. */
};

/**
 * @brief Metrics recorded during execution of the optimization pipeline.
 */
struct OptimizationStats {
    int eliminated_instructions = 0;           /**< Total number of instructions eliminated across all passes. */
    int eliminated_registers = 0;              /**< Total number of temporary registers saved by coalescing. */
    int folded_inversions = 0;                 /**< Number of inverter (NOT) gates folded into operand modifier bits. */
    int folded_constants = 0;                  /**< Number of constant expressions evaluated at compile time. */
    int eliminated_dead_nodes = 0;             /**< Number of unreachable or unconsumed nodes pruned. */
    int eliminated_moves = 0;                  /**< Number of redundant intermediate MOVE operations removed. */
    int eliminated_subexprs = 0;               /**< Number of common subexpressions unified. */
    int simplified_boolean_rules = 0;          /**< Number of boolean algebraic reduction rules applied. */
    int fused_instructions = 0;                /**< Number of arithmetic instructions fused into hardware FMA/FMS. */
};

/**
 * @brief Multi-pass optimization pipeline transforming circuit netlists into minimal bytecode IR.
 */
class Optimizer {
public:
    /**
     * @brief Constructs an Optimizer configured with the specified optimization options.
     *
     * @param[in] options Compiler optimization configuration and iteration switches.
     */
    explicit Optimizer(const le_compiler_options_t& options);

    /**
     * @brief Ingests parsed schematic elements and nets to build the initial Intermediate Representation (IR).
     *
     * @param[in] elements List of parsed element JSON objects.
     * @param[in] nets List of parsed net JSON objects declaring wire connectivity.
     * @param[in] alias_to_addr Mapping of symbolic pin aliases to physical address strings.
     * @param[in] custom_nodes Map of custom node type identifiers to node definitions.
     * @param[in] feedback_state_nodes Set of node names designated to hold persistent state for feedback cycles.
     */
    void build_ir(
        const std::vector<JsonValue>& elements,
        const std::vector<JsonValue>& nets,
        const std::map<std::string, std::string>& alias_to_addr,
        const std::map<std::string, CustomNodeInfo>& custom_nodes,
        const std::set<std::string>& feedback_state_nodes = {}
    );

    /**
     * @brief Executes all enabled optimization passes iteratively until reaching a fixed point.
     *
     * Executes constant folding, boolean simplification, inversion folding, common subexpression
     * elimination, instruction fusion, and direct destination coalescing, followed by topological
     * sort of the active nodes.
     *
     * @return Accumulated metrics describing transformations performed during the passes.
     */
    OptimizationStats run_passes();

    /**
     * @brief Returns the optimized topological execution order of active nodes.
     *
     * @return Const reference to the ordered list of node names.
     */
    const std::vector<std::string>& get_optimized_order() const noexcept { return m_execution_order; }

    /**
     * @brief Returns the complete map of intermediate representation nodes.
     *
     * @return Const reference to node dictionary mapping node names to @ref OptNode.
     */
    const std::map<std::string, OptNode>& get_nodes() const noexcept { return m_nodes; }

    /**
     * @brief Retrieves a mutable reference to an individual intermediate representation node.
     *
     * @param[in] name Node identifier string.
     * @return Mutable reference to the requested node.
     */
    OptNode& get_node(const std::string& name) { return m_nodes[name]; }

private:
    /** @brief Propagates constant inputs and evaluates static expressions at compile time. */
    void pass_constant_folding();

    /** @brief Applies boolean reduction identities (e.g. A AND TRUE = A, A AND FALSE = FALSE). */
    void pass_boolean_simplification();

    /** @brief Marks unreferenced nodes as dead through reverse reachability traversal from root sinks. */
    void pass_dead_code_elimination();

    /** @brief Identifies identical computations with matching inputs and combines them into a single node. */
    void pass_common_subexpression_elimination();

    /** @brief Folds single-fanout NOT gates into consumer operand inversion modifiers (~A, ~B). */
    void pass_inversion_folding();

    /** @brief Fuses consecutive MUL and ADD/SUB operations into hardware FMA/FMS opcodes. */
    void pass_instruction_fusion();

    /** @brief Coalesces intermediate assignments directly into output pins or registers. */
    void pass_direct_destination_coalescing();

    /** @brief Recomputes downstream consumer lists for all active nodes in the IR. */
    void update_consumer_lists();

    /** @brief Computes a valid topological execution schedule for all reachable nodes. */
    void compute_topological_order();

    le_compiler_options_t m_options;           /**< Active compiler optimization options. */
    std::map<std::string, OptNode> m_nodes;    /**< Primary intermediate representation node table. */
    std::vector<std::string> m_execution_order;/**< Topologically sorted execution schedule. */
    OptimizationStats m_stats;                 /**< Optimization statistics collector. */
};

} // namespace LogicElements

#endif /* LE_OPTIMIZER_H */

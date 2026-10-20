/**
 * @file le_compiler_options.h
 * @brief Configuration and optimization switches for the canonical LogicElements compiler.
 *
 * This header defines high-level optimization presets (@ref le_opt_level_t), fine-grained
 * pass configuration switches (@ref le_compiler_options_t), and initialization factory functions.
 *
 * ## Optimization Pipeline Architecture
 * When enabled, the multi-pass optimizer runs up to @ref le_compiler_options_t::max_pass_iterations
 * iterations until reaching a fixed point:
 * 1. **Constant Propagation & Folding**: Evaluates static boolean, integer, and floating-point subtrees.
 * 2. **Boolean Algebraic Simplification**: Applies identity, idempotence, annihilation, and De Morgan's laws.
 * 3. **Hardware Inversion Folding**: Folds inverter (`NOT`) gates into consumer operand modifier bits
 *    (`LE_MOD_INVERT_A`, `LE_MOD_INVERT_B`), executing inversion at zero runtime cycle cost.
 * 4. **Common Subexpression Elimination (CSE)**: Unifies duplicate expressions driving independent consumers.
 * 5. **Direct Destination Coalescing**: Collapses redundant `MOVE` instructions by targeting output pins directly.
 * 6. **Dead Code Elimination (DCE)**: Prunes gates and networks that do not transitively reach output pins or stateful blocks.
 */

#ifndef LE_COMPILER_OPTIONS_H
#define LE_COMPILER_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-level optimization level presets for circuit compilation.
 */
typedef enum {
    LE_OPT_NONE    = 0, /**< `-O0`: Disables optimizations to preserve a 1:1 schematic-to-instruction mapping for debugging. */
    LE_OPT_BASIC   = 1, /**< `-O1`: Performs safe basic optimizations (dead code pruning and direct destination writes). */
    LE_OPT_FULL    = 2, /**< `-O2`: Enables full optimization (constant folding, inversion folding, CSE, boolean rules). */
    LE_OPT_SIZE    = 3  /**< `-Os`: Aggressive size optimization targeting minimum bytecode container footprint. */
} le_opt_level_t;

/**
 * @brief Fine-grained optimization pass configuration and target container settings.
 */
typedef struct {
    le_opt_level_t opt_level;     /**< Base optimization level preset (defaults to @ref LE_OPT_FULL). */
    int eliminate_dead_code;      /**< 1 to prune unreferenced logic elements; 0 to preserve all elements. */
    int fold_constants;           /**< 1 to evaluate constant expressions at compile time; 0 to emit runtime loads. */
    int fold_inversions;          /**< 1 to fold NOT gates into consumer operand modifiers; 0 to emit explicit NOT instructions. */
    int direct_destination;       /**< 1 to write directly to output pins and registers, eliminating redundant MOVE instructions; 0 otherwise. */
    int eliminate_common_subexpr; /**< 1 to eliminate duplicate logic computations; 0 otherwise. */
    int simplify_boolean;         /**< 1 to apply boolean algebraic simplification rules; 0 otherwise. */
    int fuse_instructions;        /**< 1 to fuse MUL+ADD/SUB pairs into hardware FMA/FMS instructions; 0 otherwise. */
    int inline_user_nodes;        /**< 1 to inline custom user macros; 0 to emit external calls. */
    int max_pass_iterations;      /**< Maximum fixed-point pass iterations before halting (defaults to 5). */
    int emit_chunked_container;   /**< 1 to emit v13 LEB2 chunked container format; 0 for legacy flat format (default 1). */
    int enable_multi_rate;        /**< 1 to partition instructions into fast/medium/slow execution rate domains; 0 otherwise. */
    int rate_med_div;             /**< Medium rate scan prescaler divisor (defaults to 48). */
    int rate_slow_div;            /**< Slow rate scan prescaler divisor (defaults to 480). */
    int target_version;           /**< Target container format version (defaults to 13). */
} le_compiler_options_t;

/**
 * @brief Initializes a compiler options structure with presets for a specific optimization level.
 *
 * @param[in] level Desired optimization level preset.
 * @return Fully initialized @ref le_compiler_options_t structure.
 */
static inline le_compiler_options_t le_compiler_options_init(le_opt_level_t level) {
    le_compiler_options_t options;
    options.opt_level = level;
    options.max_pass_iterations = 5;
    options.inline_user_nodes = (level == LE_OPT_FULL) ? 1 : 0;
    options.emit_chunked_container = 1;
    options.enable_multi_rate = 0;
    options.rate_med_div = 48;
    options.rate_slow_div = 480;
    options.target_version = 13;
    switch (level) {
        case LE_OPT_NONE:
            options.eliminate_dead_code = 0;
            options.fold_constants = 0;
            options.fold_inversions = 0;
            options.direct_destination = 0;
            options.eliminate_common_subexpr = 0;
            options.simplify_boolean = 0;
            options.fuse_instructions = 0;
            break;
        case LE_OPT_BASIC:
            options.eliminate_dead_code = 1;
            options.fold_constants = 0;
            options.fold_inversions = 0;
            options.direct_destination = 1;
            options.eliminate_common_subexpr = 0;
            options.simplify_boolean = 0;
            options.fuse_instructions = 0;
            break;
        case LE_OPT_FULL:
        case LE_OPT_SIZE:
        default:
            options.eliminate_dead_code = 1;
            options.fold_constants = 1;
            options.fold_inversions = 1;
            options.direct_destination = 1;
            options.eliminate_common_subexpr = 1;
            options.simplify_boolean = 1;
            options.fuse_instructions = 1;
            break;
    }
    return options;
}

/**
 * @brief Returns default compiler options configured for full optimization (`-O2`).
 *
 * @return Initialized @ref le_compiler_options_t structure with standard default settings.
 */
static inline le_compiler_options_t le_compiler_options_init_default(void) {
    return le_compiler_options_init(LE_OPT_FULL);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPILER_OPTIONS_H */

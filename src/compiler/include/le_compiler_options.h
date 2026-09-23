/**
 * @file le_compiler_options.h
 * @brief Configuration and optimization flags for the canonical LogicElements compiler.
 */

#ifndef LE_COMPILER_OPTIONS_H
#define LE_COMPILER_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-level optimization levels for bytecode generation.
 */
typedef enum {
    LE_OPT_NONE    = 0, /**< `-O0`: Disables optimizations to maintain 1:1 schematic-to-instruction mapping for debugging. */
    LE_OPT_BASIC   = 1, /**< `-O1`: Performs safe basic optimizations including direct destination write and dead code pruning. */
    LE_OPT_FULL    = 2, /**< `-O2`: Enables full optimization passes including inversion folding, constant propagation, and common subexpression elimination. */
    LE_OPT_SIZE    = 3  /**< `-Os`: Aggressive size optimization targeting minimum bytecode binary footprint. */
} le_opt_level_t;

/**
 * @brief Fine-grained optimization pass configuration flags.
 */
typedef struct {
    le_opt_level_t opt_level;     /**< Base optimization level preset (defaults to @ref LE_OPT_FULL). */
    int eliminate_dead_code;      /**< Set to 1 to prune unreferenced logic elements; otherwise, 0. */
    int fold_constants;           /**< Set to 1 to evaluate constant expressions at compile time; otherwise, 0. */
    int fold_inversions;          /**< Set to 1 to fold NOT gates into consumer operand modifiers (@ref LE_MOD_INVERT_A, @ref LE_MOD_INVERT_B); otherwise, 0. */
    int direct_destination;       /**< Set to 1 to write directly to output pins and registers, eliminating redundant MOVE instructions; otherwise, 0. */
    int eliminate_common_subexpr; /**< Set to 1 to eliminate duplicate logic computations; otherwise, 0. */
    int inline_user_nodes;        /**< Set to 1 to inline custom user macros; otherwise, 0. */
    int max_pass_iterations;      /**< Maximum fixed-point pass iterations (defaults to 5). */
} le_compiler_options_t;

/**
 * @brief Initializes a compiler options structure with presets for a specific optimization level.
 *
 * @param level Optimization level presets to apply.
 * @return Returns an initialized @ref le_compiler_options_t structure.
 */
static inline le_compiler_options_t le_compiler_options_init(le_opt_level_t level) {
    le_compiler_options_t options;
    options.opt_level = level;
    options.max_pass_iterations = 5;
    options.inline_user_nodes = (level == LE_OPT_FULL) ? 1 : 0;
    switch (level) {
        case LE_OPT_NONE:
            options.eliminate_dead_code = 0;
            options.fold_constants = 0;
            options.fold_inversions = 0;
            options.direct_destination = 0;
            options.eliminate_common_subexpr = 0;
            break;
        case LE_OPT_BASIC:
            options.eliminate_dead_code = 1;
            options.fold_constants = 0;
            options.fold_inversions = 0;
            options.direct_destination = 1;
            options.eliminate_common_subexpr = 0;
            break;
        case LE_OPT_FULL:
        case LE_OPT_SIZE:
        default:
            options.eliminate_dead_code = 1;
            options.fold_constants = 1;
            options.fold_inversions = 1;
            options.direct_destination = 1;
            options.eliminate_common_subexpr = 1;
            break;
    }
    return options;
}

/**
 * @brief Returns a default compiler options structure configured for full optimization (`-O2`).
 *
 * @return Returns an initialized @ref le_compiler_options_t structure with default settings.
 */
static inline le_compiler_options_t le_compiler_options_init_default(void) {
    return le_compiler_options_init(LE_OPT_FULL);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPILER_OPTIONS_H */

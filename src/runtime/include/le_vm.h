/**
 * @file le_vm.h
 * @brief Virtual Machine runtime for executing LogicElements bytecode programs.
 */

#ifndef LE_VM_H
#define LE_VM_H

#include "le_types.h"
#include "le_process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Virtual machine instance managing process image state and bytecode execution.
 */
typedef struct {
    le_process_image_t        image;             /**< Arena-backed process image descriptor (see le_process_image_bind). */

    /* Unified RAM workspace: one board-tunable pool (LE_RAM_WORKSPACE_BYTES)
     * carved at load time into the packed register arena (front) and the state
     * workspace slice (back). A union forces 4-byte alignment so the float /
     * complex buckets and state structs are naturally aligned. */
    union {
        uint8_t                ram_workspace[LE_RAM_WORKSPACE_BYTES];
        uint32_t               _align4[(LE_RAM_WORKSPACE_BYTES + 3) / 4];
    };

    const le_instruction_t*   instructions;      /**< Pointer to active instruction array in flash or RAM. */
    uint16_t                  instruction_count; /**< Number of valid instructions in the active program. */
    const le_block_desc_t*    blocks;            /**< Pointer to active variable-arity block table (or NULL). */
    uint16_t                  block_count;       /**< Number of valid block descriptors. */
    const le_alias_t*         aliases;           /**< Zero-copy program alias table (from the .lebin) or NULL. */
    uint16_t                  alias_count;       /**< Number of valid program aliases. */
    const le_alias_t*         board_aliases;     /**< Host/board-supplied alias table (NOT carried in the .lebin) or NULL. */
    uint16_t                  board_alias_count; /**< Number of valid board aliases. */
    le_pulse_t                pulses[LE_MAX_PULSES]; /**< Pending runtime pulses (alias/register pulse commands). */
    uint8_t                   pulse_count;       /**< Number of active pulse slots. */
    bool                      running;           /**< True if the execution loop is actively stepping. */
    uint32_t                  cycle_count;       /**< Monotonically increasing execution scan count. */

    /* Fixed-rate scan clock (deterministic execution). Set with
     * le_vm_set_scan_period_us(); when le_vm_step_us is called and
     * enforce_fixed_rate is set, now_us must advance by exactly scan_period_us
     * each scan or LE_ERR_SCAN_JITTER is returned. */
    uint32_t                  scan_period_us;    /**< Fixed scan period in microseconds (0 = host-driven). */
    uint32_t                  last_scan_us;      /**< Timestamp of the previous scan (0 = never stepped). */
    uint8_t                   enforce_fixed_rate;/**< 1 = reject off-cadence scans. */
    uint32_t                  observed_worst_us; /**< Largest measured scan duration (needs HAL time). */
    uint32_t                  scan_overruns;     /**< Scans whose measured duration exceeded the period. */

    /* Program timing budget (from the .lebin timing descriptor + board cost). */
    uint32_t                  timing_abstract_cycles; /**< Compiler cost units (0 = none). */
    uint16_t                  timing_margin_pct;  /**< Compiler safety multiplier (150 = 1.5x). */
    uint32_t                  timed_worst_us;    /**< Estimated worst-case scan duration (board cost). */
    uint8_t                   timing_feasible;   /**< 1 when timed_worst_us <= scan_period_us. */
} le_vm_t;

/**
 * @brief Initializes a virtual machine instance and clears its process image.
 *
 * @param vm Pointer to the virtual machine structure to initialize.
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if @p vm is NULL.
 */
le_status_t le_vm_init(le_vm_t* vm);

/**
 * @brief Points the virtual machine to an instruction sequence in flash ROM or RAM.
 *
 * Configures the instruction pointer without copying instruction data, supporting
 * zero-copy Execute-In-Place (XIP).
 *
 * @param vm Pointer to the virtual machine instance.
 * @param instructions Pointer to the contiguous array of bytecode instructions.
 * @param count Number of instructions in the array.
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if pointers are NULL.
 */
le_status_t le_vm_load_program(le_vm_t* vm, const le_instruction_t* instructions, uint16_t count);

/**
 * @brief Points the virtual machine to a variable-arity block table.
 *
 * Configures the pointer to the block descriptors without copying, supporting
 * zero-copy Execute-In-Place (XIP) alongside the instruction array.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param blocks Pointer to the contiguous array of block descriptors (or NULL if none).
 * @param count Number of block descriptors.
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if @p vm is NULL.
 */
le_status_t le_vm_load_blocks(le_vm_t* vm, const le_block_desc_t* blocks, uint16_t count);

/**
 * @brief Executes one complete logic scan cycle through all loaded instructions.
 *
 * Evaluates instructions sequentially in topological order. If an instruction
 * returns an error code, execution halts and returns that error.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param now_ms Current system timestamp in milliseconds for time-based blocks.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t if an instruction fails.
 */
le_status_t le_vm_step(le_vm_t* vm, uint32_t now_ms);

/**
 * @brief Enables instruction execution in the virtual machine.
 *
 * @param vm Pointer to the virtual machine instance.
 */
void le_vm_start(le_vm_t* vm);

/**
 * @brief Pauses instruction execution in the virtual machine.
 *
 * @param vm Pointer to the virtual machine instance.
 */
void le_vm_stop(le_vm_t* vm);

/**
 * @brief Clears the process image and resets internal state machines.
 *
 * Sets all digital inputs, outputs, boolean registers, floats, and stateful blocks
 * (timers, counters, PID, protection) back to their default initialized states.
 *
 * @param vm Pointer to the virtual machine instance to reset.
 */
void le_vm_reset(le_vm_t* vm);

/**
 * @brief Attaches a zero-copy program alias table (from the .lebin) to the VM.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param aliases Pointer to the alias table array, or NULL to clear.
 * @param count Number of alias entries.
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if @p vm is NULL.
 */
le_status_t le_vm_load_aliases(le_vm_t* vm, const le_alias_t* aliases, uint16_t count);

/**
 * @brief Attaches a host/board-supplied alias table.
 *
 * Board pin aliases are resolved by the host/firmware (from the board profile)
 * and supplied here so the runtime can target them by name WITHOUT carrying
 * them in the program's .lebin. Program aliases take precedence on collision.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param aliases Pointer to the board alias table array, or NULL to clear.
 * @param count Number of alias entries.
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if @p vm is NULL.
 */
le_status_t le_vm_load_board_aliases(le_vm_t* vm, const le_alias_t* aliases, uint16_t count);

/**
 * @brief Resolves an alias name to a process-image address.
 *
 * Searches the program alias table first, then the board alias table. Aliases
 * may optionally be prefixed with '%'.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param name Alias name (<= LE_ALIAS_NAME_MAX chars; optional leading '%').
 * @param out_addr Pointer receiving the resolved address (must be non-NULL).
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NOT_FOUND / @ref LE_ERR_NULL_PTR otherwise.
 */
le_status_t le_alias_lookup(const le_vm_t* vm, const char* name, uint16_t* out_addr);

/**
 * @brief Writes a boolean to the register referenced by an alias.
 */
le_status_t le_alias_set_bool(le_vm_t* vm, const char* name, bool val);

/**
 * @brief Writes a float to the register referenced by an alias.
 */
le_status_t le_alias_set_float(le_vm_t* vm, const char* name, float val);

/**
 * @brief Writes an integer to the register referenced by an alias.
 */
le_status_t le_alias_set_int(le_vm_t* vm, const char* name, int32_t val);

/**
 * @brief Toggles the boolean register referenced by an alias.
 */
le_status_t le_alias_toggle(le_vm_t* vm, const char* name);

/**
 * @brief Pulses an alias for the default duration (1000 ms = 1 second).
 *
 * Sets the register to its active (non-zero) state and clears it back to zero
 * once `now_ms` advances past the duration. See @ref le_alias_pulse_for.
 */
le_status_t le_alias_pulse(le_vm_t* vm, const char* name);

/**
 * @brief Pulses an alias for an explicit duration in seconds.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param name Alias name.
 * @param seconds Positive duration in seconds (e.g. 2.5); <= 0 defaults to 1 s.
 * @return Returns @ref LE_OK on success, @ref LE_ERR_NOT_FOUND if the alias is
 *         unknown, or @ref LE_ERR_CAPACITY if all pulse slots are busy.
 */
le_status_t le_alias_pulse_for(le_vm_t* vm, const char* name, float seconds);

/**
 * @brief Arms a raw process-image address as a pulse for a duration in ms.
 *
 * Used by the CLI / comms pulse command. Clears the register to zero when the
 * VM's `now_ms` crosses the expiry (checked inside @ref le_vm_step).
 */
le_status_t le_vm_pulse(le_vm_t* vm, uint16_t addr, uint32_t duration_ms);

/**
 * @brief Configures the fixed scan period for deterministic execution.
 *
 * Sets scan_period_us, resets the scan anchor, and applies the period (in
 * seconds) to the runtime dt used by time-dependent opcodes (PID, overcurrent).
 *
 * @param vm Pointer to the virtual machine instance.
 * @param period_us Fixed scan period in microseconds (e.g. 1042 for 960 Hz).
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if @p vm is NULL.
 */
le_status_t le_vm_set_scan_period_us(le_vm_t* vm, uint32_t period_us);

/**
 * @brief Enables or disables strict fixed-rate cadence enforcement.
 *
 * When enabled, @ref le_vm_step_us rejects timestamps that do not advance by
 * exactly scan_period_us with @ref LE_ERR_SCAN_JITTER.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param enable True to enforce, false for host-driven (legacy) stepping.
 */
void le_vm_set_enforce_fixed_rate(le_vm_t* vm, bool enable);

/**
 * @brief Executes one scan at the given microsecond timestamp.
 *
 * The deterministic entry point for fixed-rate operation: when enforcement is
 * enabled, @p now_us must equal last_scan_us + scan_period_us. Also measures
 * the actual scan duration (when the HAL provides a microsecond clock) and
 * accrues observed_worst_us / scan_overruns.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param now_us Monotonic microsecond timestamp of this scan.
 * @return Returns @ref LE_OK on success, @ref LE_ERR_SCAN_JITTER on an
 *         off-cadence timestamp, or the underlying execution error.
 */
le_status_t le_vm_step_us(le_vm_t* vm, uint32_t now_us);

/**
 * @brief Blocks until the next fixed-rate boundary, then executes one scan.
 *
 * Scheduler entry point for firmware main loops: waits (via the HAL microsecond
 * clock) until last_scan_us + scan_period_us, then calls @ref le_vm_step_us with
 * the canonical boundary timestamp.
 *
 * @param vm Pointer to the virtual machine instance (scan period must be set).
 * @return Returns @ref LE_OK on success, @ref LE_ERR_TIMING_BUDGET if no period
 *         is configured, or an execution error.
 */
le_status_t le_vm_run_scan(le_vm_t* vm);

#ifdef __cplusplus
}
#endif

#endif /* LE_VM_H */

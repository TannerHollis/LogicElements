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
    le_process_image_t        image;             /**< Statically allocated process image storage. */
    const le_instruction_t*   instructions;      /**< Pointer to active instruction array in flash or RAM. */
    uint16_t                  instruction_count; /**< Number of valid instructions in the active program. */
    const le_block_desc_t*    blocks;            /**< Pointer to active variable-arity block table (or NULL). */
    uint16_t                  block_count;       /**< Number of valid block descriptors. */
    bool                      running;           /**< True if the execution loop is actively stepping. */
    uint32_t                  cycle_count;       /**< Monotonically increasing execution scan count. */
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

#ifdef __cplusplus
}
#endif

#endif /* LE_VM_H */

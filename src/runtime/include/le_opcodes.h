/**
 * @file le_opcodes.h
 * @brief Opcode execution handlers for LogicElements C Runtime.
 */

#ifndef LE_OPCODES_H
#define LE_OPCODES_H

#include "le_types.h"
#include "le_process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Executes a single instruction against the process image, with no
 * variable-arity block table (equivalent to @ref le_exec_instruction_ex with
 * `blocks = NULL`, `block_count = 0`). An @ref LE_OP_BLOCK instruction will
 * therefore fail with @ref LE_ERR_OUT_OF_BOUNDS.
 *
 * @param inst Pointer to the instruction to execute.
 * @param img Pointer to the process image storage.
 * @param now_ms Current system timestamp in milliseconds for timer/counter updates.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t on failure.
 */
le_status_t le_exec_instruction(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms);

/**
 * @brief Executes a single instruction against the process image, consulting
 * the variable-arity block table for @ref LE_OP_BLOCK opcodes.
 *
 * @param inst Pointer to the instruction to execute.
 * @param img Pointer to the process image storage.
 * @param now_ms Current system timestamp in milliseconds for timer/counter updates.
 * @param blocks Pointer to the block descriptor table (or NULL if @p block_count is 0).
 * @param block_count Number of block descriptors in @p blocks.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t on failure.
 */
le_status_t le_exec_instruction_ex(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms,
                                   const le_block_desc_t* blocks, uint16_t block_count);

/**
 * @brief Batch executes a sequence of instructions against the process image.
 *
 * Loops internally over the instruction stream, keeping register allocations
 * and stack frame setup amortized once per scan instead of per instruction.
 *
 * @param instructions Pointer to array of instructions to execute.
 * @param count Number of instructions to execute.
 * @param img Pointer to the process image storage.
 * @param now_ms Current system timestamp in milliseconds.
 * @param blocks Pointer to the block descriptor table.
 * @param block_count Number of block descriptors.
 * @return Returns LE_OK on success, or an error code on failure.
 */
le_status_t le_exec_program(const le_instruction_t* instructions, uint16_t count,
                            le_process_image_t* img, uint32_t now_ms,
                            const le_block_desc_t* blocks, uint16_t block_count);

#ifdef __cplusplus
}
#endif

#endif /* LE_OPCODES_H */

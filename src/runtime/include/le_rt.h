/**
 * @file le_rt.h
 * @brief Static zero-heap state arena and runtime block table.
 *
 * Stateful "complex" elements (timers, counters, PID, phasors, DSP filters,
 * protection, scalers) store their state in a fixed static byte arena. At
 * runtime start the loader/VM "reserves" one heap slice per active state block
 * and records a pointer to it in the runtime block table. Each scan an
 * instruction dereferences that pointer instead of indexing a fixed array.
 *
 * The arena is a static array - there is never a malloc after boot.
 */
#ifndef LE_RT_H
#define LE_RT_H

#include "le_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief (Re)initializes the state arena and runtime block table.
 * Idempotent; safe to call at every VM init.
 */
void le_rt_init(void);

/**
 * @brief Reserves a slice of the state arena and claims a runtime-block row.
 * @param kind State struct kind (le_block_kind_t); LE_BLK_NONE is rejected.
 * @param size Bytes of state to allocate.
 * @param align Alignment of the slice (0 or 1 == byte aligned).
 * @return row index into the runtime block table, or -1 if the arena or the row
 *         table is exhausted.
 */
int le_rt_reserve(uint8_t kind, uint16_t size, uint16_t align);

/** @brief Binds a caller-supplied state pointer to a runtime-block row. */
void le_rt_bind(int row, uint8_t kind, uint16_t size, uint8_t* state);

/** @brief Returns the descriptor for a row, or NULL if unused/out of range. */
le_rt_block_t* le_rt_get(int row);

/** @brief Returns the current arena pointer (for low-level use / diagnostics). */
uint8_t* le_rt_heap(void);

/** @brief Number of bytes of the arena used so far. */
uint32_t le_rt_used(void);

/** @brief Total arena capacity in bytes. */
uint32_t le_rt_capacity(void);

/** @brief Number of rows currently claimed. */
int le_rt_row_count(void);

/** @brief Records the base runtime-block row for a kind group. */
void le_rt_set_group_base(uint8_t kind, int base);

/**
 * @brief Returns the base runtime-block row for @p kind, or -1 if no state of
 * that kind has been bound (e.g. a program without that element type, or
 * manual instructions executed without the loader).
 */
int le_rt_group_base(uint8_t kind);

#ifdef __cplusplus
}
#endif

#endif /* LE_RT_H */
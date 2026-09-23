/**
 * @file le_rt.h
 * @brief Runtime state workspace derived from the on-disk state image.
 *
 * Stateful "complex" elements (timers, counters, PID, phasors, DSP filters,
 * protection, scalers, I2C/SPI devices) live in a single contiguous RAM
 * workspace. The compiler bakes a preconfigured state image into the `.lebin`;
 * the loader memcpy's that image directly into the workspace at load time and
 * records the byte offset (kind base) of each kind group. An instruction then
 * reaches block `idx` of `kind` via:
 *
 *     block = le_rt_workspace() + kind_base[kind] + idx * sizeof(kind-state)
 *
 * Offset arithmetic (not a heap/allocator) replaces any runtime allocation, and
 * the workspace is sized by the platform (LE_STATE_WORKSPACE_BYTES), not by a
 * per-program cap. There is never a malloc after boot.
 */
#ifndef LE_RT_H
#define LE_RT_H

#include "le_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief (Re)initializes the state workspace: zeroes it and clears the kind-base
 * table. Idempotent; safe to call at every VM init.
 */
void le_rt_reset(void);

/** @brief Returns the base of the state workspace (RAM the image is copied into). */
uint8_t* le_rt_workspace(void);

/** @brief Total workspace capacity in bytes (LE_STATE_WORKSPACE_BYTES). */
uint32_t le_rt_workspace_bytes(void);

/**
 * @brief Returns the byte size of one state struct for @p kind (0 if the kind
 * has no state). Used to index within a kind group.
 */
uint16_t le_rt_kind_size(uint8_t kind);

/**
 * @brief Records the byte offset of the @p kind group within the workspace.
 * Set by the loader (and by tests) after the state image is placed.
 */
void le_rt_set_kind_base(uint8_t kind, int32_t base);

/**
 * @brief Returns the workspace byte offset of the @p kind group, or -1 if no
 * state of that kind is present.
 */
int32_t le_rt_kind_base(uint8_t kind);

/**
 * @brief Resolves block @p idx of @p kind to its state pointer in the workspace,
 * or NULL if the kind is absent or @p idx falls outside the workspace.
 */
uint8_t* le_rt_state(uint8_t kind, uint16_t idx);

#ifdef __cplusplus
}
#endif

#endif /* LE_RT_H */
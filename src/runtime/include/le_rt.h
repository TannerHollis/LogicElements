/**
 * @file le_rt.h
 * @brief Runtime state workspace: a slice of the board's unified RAM workspace.
 *
 * Stateful "complex" elements (timers, counters, PID, phasors, DSP filters,
 * protection, scalers, I2C/SPI devices) live in a contiguous RAM slice carved
 * out of the board's shared LE_RAM_WORKSPACE_BYTES pool (after the packed
 * register arena). The compiler bakes a preconfigured state image into the
 * `.lebin`; the loader memcpy's that image directly into the slice at load time
 * and records the byte offset (kind base) of each kind group. An instruction
 * then reaches block `idx` of `kind` via:
 *
 *     block = le_rt_workspace() + kind_base[kind] + idx * sizeof(kind-state)
 *
 * Offset arithmetic (not a heap/allocator) replaces any runtime allocation, and
 * the slice is bounded by the part of the pool a program's state image consumes
 * (loader-validated against LE_RAM_WORKSPACE_BYTES). No malloc ever after boot.
 */
#ifndef LE_RT_H
#define LE_RT_H

#include "le_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Binds the state workspace to a slice of the unified RAM workspace.
 *
 * Called by the loader (and by test harnesses) after the register arena is
 * carved out; @p base must be the slice start and @p len its size. Before any
 * bind all workspace accessors return NULL / 0.
 *
 * @param base Caller-owned byte buffer the preconfigured state image is copied into (may be NULL to unbind).
 * @param len  Number of bytes available at @p base.
 */
void le_rt_bind(uint8_t* base, uint32_t len);

/**
 * @brief (Re)initializes the state workspace: zeroes the bound slice and clears
 * the kind-base table. Idempotent; safe to call at every VM init.
 */
void le_rt_reset(void);

/** @brief Returns the base of the state workspace (RAM the image is copied into). */
uint8_t* le_rt_workspace(void);

/** @brief Byte size of the currently bound state slice (0 before a bind). */
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
 * @brief Records how many instances of @p kind were declared in the loaded
 * program. `le_rt_state` rejects indices at or past this count, hardening the
 * resolver against malformed binaries that reference blocks the program never
 * declared.
 *
 * @param kind Kind tag (see @ref le_status_t).
 * @param count Declared instance count for the kind group (0 = none).
 */
void le_rt_set_kind_count(uint8_t kind, uint16_t count);

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

/**
 * @brief Sets the fixed scan period (in seconds) used by time-dependent
 * opcodes (PID, overcurrent, totalizers) when no scan rate is configured.
 * Called by the scheduler / host alongside le_vm_set_scan_period_us().
 */
void le_rt_set_scan_dt(float seconds);

/**
 * @brief Returns the current fixed scan period in seconds (defaults to
 * LE_DEFAULT_SCAN_DT_SEC when no scan rate has been configured).
 */
float le_rt_scan_dt(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_RT_H */
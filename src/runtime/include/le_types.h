/**
 * @file le_types.h
 * @brief Umbrella header for core types, opcodes, memory layout, binary format, and element state.
 *
 * For modularity and fast compilation, types are decomposed into:
 *   - le_addr.h: Memory layout, region masks, and address encoding macros.
 *   - le_opcodes.h: Opcode enumeration, modifiers, and block function IDs.
 *   - le_binary.h: Binary file headers, chunk descriptors, and instruction formats.
 *   - le_elements.h: Element block state structs (timers, DSP, protection, serial).
 *
 * Including le_types.h continues to provide all definitions with zero code changes required.
 */

#ifndef LE_TYPES_H
#define LE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "le_config.h"
#include "le_complex.h"
#include "le_addr.h"
#include "le_opcode_defs.h"
#include "le_binary.h"
#include "le_elements.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Static zero-heap state arena for stateful "complex" elements.              */
/* A fixed byte arena (no malloc); blocks are bound to heap pointers at       */
/* runtime start and instructions dereference those pointers each scan.       */
/* ========================================================================== */

/** @brief Kind tag naming which state struct a runtime state block holds.
 * Values are explicit so the on-disk `.lebin` state-desc table is stable
 * regardless of which optional subsystems are compiled in. */
typedef enum {
    LE_BLK_NONE      = 0,   /* stateless (variable-arity block call) */
#define LE_BLOCK_DEF(id, val, name, type, gate) id = val,
#include "le_blocks.def"
#undef LE_BLOCK_DEF
    LE_BLK_LAST             /* sentinel (not a real block kind) */
} le_block_kind_t;

/** @brief One active state-block slot in the runtime block table (RAM). */
typedef struct {
    uint8_t  kind;         /**< @ref le_block_kind_t. */
    uint16_t state_size;   /**< Bytes at @p state (for bookkeeping/reset). */
    uint8_t* state;        /**< Pointer into the zero-heap state arena. */
} le_rt_block_t;

/**
 * @brief A single runtime pulse slot used to implement alias/register pulse
 * commands. A pulse sets a writable register to its active (non-zero) state,
 * then clears it to zero once `now_ms` reaches @p clear_after_ms.
 */
typedef struct {
    uint16_t addr;              /**< Process-image address being pulsed. */
    bool     active;            /**< Set while the pulse slot is in use. */
    uint32_t duration_ms;       /**< Requested pulse duration in milliseconds. */
    uint32_t clear_after_ms;    /**< Absolute VM timestamp when the pulse clears (0 = not yet anchored to a scan). */
} le_pulse_t;

/**
 * @brief Status and return codes for runtime API operations.
 */
typedef enum {
    LE_OK                   = 0,   /**< Operation completed successfully. */
    LE_ERR_NULL_PTR         = -1,  /**< A required pointer parameter was NULL. */
    LE_ERR_INVALID_MAGIC    = -2,  /**< Magic header does not match expected identifier. */
    LE_ERR_INVALID_VERSION  = -3,  /**< Bytecode version is unsupported. */
    LE_ERR_CRC_MISMATCH     = -4,  /**< CRC32 checksum validation failed. */
    LE_ERR_CAPACITY         = -5,  /**< Required resources exceed target capacity limits. */
    LE_ERR_OUT_OF_BOUNDS    = -6,  /**< Index or address exceeds allocated bounds. */
    LE_ERR_UNKNOWN_OPCODE   = -7,  /**< Instruction contains an unrecognized opcode. */
    LE_ERR_NOT_FOUND        = -8,  /**< Named alias/register not found. */
    LE_ERR_SCAN_JITTER      = -9,  /**< Fixed-rate scan boundary missed (non-uniform cadence). */
    LE_ERR_TIMING_BUDGET    = -10, /**< Program worst-case scan exceeds the configured scan period. */
    LE_ERR_SCAN_OVERFLOW    = -11, /**< A measured scan overran the fixed period. */
    LE_ERR_ACTIVE_SLOT      = -12, /**< Upload/catalog operation targeted the currently active slot. */
    LE_ERR_STORAGE          = -13, /**< Non-volatile storage read/write failed. */
    LE_ERR_INVALID_HEADER   = -14, /**< Program or container header format is invalid or missing required sections. */
    LE_ERR_SIGNATURE_MISSING = -15, /**< Program container lacks required cryptographic signature chunk. */
    LE_ERR_SIGNATURE_INVALID = -16  /**< Cryptographic signature verification failed (tampered binary or wrong public key). */
} le_status_t;

#ifdef __cplusplus
}
#endif

#endif /* LE_TYPES_H */
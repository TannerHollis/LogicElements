/**
 * @file le_event_rec.h
 * @brief Multi-level Event Recorder (Oscillography / Waveform Capture) engine.
 */

#ifndef LE_EVENT_REC_H
#define LE_EVENT_REC_H

#include "le_types.h"
#include "le_process_image.h"
#include "le_nvram.h"
#include "le_ser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LE_EVENT_REC_MAX_LEVELS     4
#define LE_EVENT_REC_MAX_CHANNELS   16
#define LE_EVENT_REC_MAGIC          0x45565231UL /* ASCII 'EVR1' */

typedef struct {
    bool     enabled;                                 /**< Whether level is active */
    uint16_t rate_div;                                /**< Scan rate decimation factor (1 = every scan) */
    uint16_t pre_trigger;                             /**< Pre-trigger sample depth */
    uint16_t capacity;                                /**< Total sample rows in ring */
    uint16_t sample_size;                             /**< Byte width of one sample row */
    uint16_t channel_count;                           /**< Number of channels recorded in this level */
    uint16_t channels[LE_EVENT_REC_MAX_CHANNELS];     /**< Process image virtual addresses */
} le_event_level_cfg_t;

typedef struct {
    uint8_t  triggered;        /**< 1 if triggered, 0 if armed/idle */
    uint8_t  completed;        /**< 1 if post-trigger capture filled */
    uint16_t post_remaining;   /**< Samples remaining until capture freeze */
    uint32_t trigger_seq;      /**< Scan sequence when trigger fired */
    uint64_t trigger_time_us;  /**< Microsecond timestamp of trigger */
    uint32_t sample_count;     /**< Total samples collected */
    uint32_t write_index;      /**< Circular sample buffer write index */
} le_event_level_state_t;

typedef struct {
    uint16_t             trigger_address;             /**< Digital signal address causing trigger */
    uint8_t              trigger_edge;                /**< LE_SER_EDGE_RISING, FALLING, BOTH */
    uint8_t              active_levels;               /**< Number of levels configured */
    le_event_level_cfg_t levels[LE_EVENT_REC_MAX_LEVELS];
} le_event_rec_cfg_t;

typedef struct {
    uint32_t               magic;                     /**< LE_EVENT_REC_MAGIC */
    uint8_t                active_levels;             /**< Number of valid level states */
    uint8_t                reserved[3];
    le_event_level_state_t levels[LE_EVENT_REC_MAX_LEVELS];
} le_event_rec_nv_header_t;

/**
 * @brief Runtime level tracking for Event Recorder.
 */
typedef struct {
    le_event_level_cfg_t   cfg;
    le_event_level_state_t state;
    uint8_t*               sample_buffer; /**< Pointer to allocated sample RAM */
    uint32_t               buffer_bytes;  /**< cfg.capacity * row_size */
} le_event_rec_level_t;

/**
 * @brief Multi-Level Event Recorder instance.
 */
typedef struct {
    uint16_t             trigger_address;
    uint8_t              trigger_edge;
    uint8_t              prev_trigger_state;
    uint8_t              active_levels;
    le_event_rec_level_t levels[LE_EVENT_REC_MAX_LEVELS];

    /* Non-volatile backing & timing */
    uint32_t             nv_offset; /**< Logical NVRAM offset (e.g. LE_NVRAM_OFFSET_EVENT_REC) */
    bool                 is_dirty;
    uint64_t             last_activity_time_us; /**< Timestamp of last trigger or sample capture */
} le_event_rec_t;

/**
 * @brief Initializes the Multi-Level Event Recorder (Oscillography) instance.
 *
 * Partitions the provided RAM pool among active recording levels according to their
 * configured capacities and sample row widths. Configures the trigger source address
 * and edge mode, sets up the NVRAM persistence offset, and restores existing captures
 * if persistent data is present in NVRAM.
 *
 * @param rec Pointer to the Event Recorder instance to initialize.
 * @param cfg Configuration structure specifying trigger criteria and per-level settings.
 * @param ram_pool Pointer to caller-allocated contiguous RAM storage for sample circular buffers.
 * @param pool_size Total byte capacity of @p ram_pool.
 * @param nv_offset Logical NVRAM byte offset for persistence (defaults to `LE_NVRAM_OFFSET_EVENT_REC` if 0).
 */
void le_event_rec_init(le_event_rec_t* rec, const le_event_rec_cfg_t* cfg,
                       uint8_t* ram_pool, size_t pool_size, uint32_t nv_offset);

/**
 * @brief Restores Event Recorder state and recorded samples from non-volatile RAM.
 *
 * Reads the persistent header at `rec->nv_offset`, verifies `LE_EVENT_REC_MAGIC`
 * and active level definitions, and restores sample circular buffer contents from NVRAM.
 *
 * @param rec Pointer to the initialized Event Recorder instance.
 * @return Returns `true` if valid capture records were restored; otherwise, `false`.
 */
bool le_event_rec_restore_nvram(le_event_rec_t* rec);

/**
 * @brief Flushes a completed Event Recorder capture to non-volatile RAM.
 *
 * Serializes the header metadata and all recorded sample circular buffers across
 * active levels into persistent NVRAM storage, clearing the dirty flag on success.
 *
 * @param rec Pointer to the Event Recorder instance to flush.
 * @return Returns `true` if NVRAM write succeeded; otherwise, `false`.
 */
bool le_event_rec_flush_nvram(le_event_rec_t* rec);

/**
 * @brief Executes one periodic scan step of the Event Recorder.
 *
 * Evaluates trigger conditions against the process image @p img. If triggered,
 * transitions levels into post-trigger countdown; otherwise, buffers pre-trigger
 * historical samples at each level's configured decimation rate (`rate_div`).
 *
 * @param rec Pointer to the Event Recorder instance.
 * @param img Pointer to the active process image containing live channel values.
 * @param scan_seq Monotonic execution scan counter.
 * @param now_us Monotonic microsecond timestamp of the current scan.
 */
void le_event_rec_scan(le_event_rec_t* rec, const le_process_image_t* img, uint32_t scan_seq, uint64_t now_us);

/**
 * @brief Checks if any level is currently actively capturing post-trigger samples.
 *
 * @param rec Pointer to the Event Recorder instance.
 * @return Returns `true` if at least one level is capturing post-trigger samples; otherwise, `false`.
 */
bool le_event_rec_is_capturing(const le_event_rec_t* rec);

/**
 * @brief Re-arms the Event Recorder for another fault capture.
 *
 * Resets trigger flags and sample counters on all configured levels, preparing
 * the engine for subsequent pre-trigger buffering and edge detection.
 *
 * @param rec Pointer to the Event Recorder instance to re-arm.
 */
void le_event_rec_arm(le_event_rec_t* rec);

/**
 * @brief Calculates the total byte size required to buffer a given recording level.
 *
 * Computes `capacity * (8 + sum(channel_sizes))` based on each channel's process-image region width.
 *
 * @param lvl Pointer to the level configuration structure.
 * @return Total required buffer size in bytes, or 0 if disabled or invalid.
 */
uint32_t le_event_rec_calc_level_bytes(const le_event_level_cfg_t* lvl);

#ifdef __cplusplus
}
#endif

#endif /* LE_EVENT_REC_H */

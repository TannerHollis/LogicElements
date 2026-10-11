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
 * @brief Initializes the Event Recorder instance.
 */
void le_event_rec_init(le_event_rec_t* rec, const le_event_rec_cfg_t* cfg,
                       uint8_t* ram_pool, size_t pool_size, uint32_t nv_offset);

/**
 * @brief Restores Event Recorder state and recorded samples from NVRAM.
 */
bool le_event_rec_restore_nvram(le_event_rec_t* rec);

/**
 * @brief Flushes completed Event Recorder capture to NVRAM.
 */
bool le_event_rec_flush_nvram(le_event_rec_t* rec);

/**
 * @brief Executes one scan step of the Event Recorder.
 */
void le_event_rec_scan(le_event_rec_t* rec, const le_process_image_t* img, uint32_t scan_seq, uint64_t now_us);

/**
 * @brief Checks if any level is currently actively capturing post-trigger samples.
 */
bool le_event_rec_is_capturing(const le_event_rec_t* rec);

/**
 * @brief Re-arms the Event Recorder for another capture.
 */
void le_event_rec_arm(le_event_rec_t* rec);

/**
 * @brief Returns the total byte size required to buffer a given level.
 */
uint32_t le_event_rec_calc_level_bytes(const le_event_level_cfg_t* lvl);

#ifdef __cplusplus
}
#endif

#endif /* LE_EVENT_REC_H */

/**
 * @file le_ser.h
 * @brief Sequence of Events Recorder (SER) engine for digital transitions.
 */

#ifndef LE_SER_H
#define LE_SER_H

#include "le_types.h"
#include "le_process_image.h"
#include "le_nvram.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LE_SER_MAX_CHANNELS     64
#define LE_SER_STATE_WORDS      ((LE_SER_MAX_CHANNELS + 31) / 32)
#define LE_SER_MAGIC            0x53455231UL /* ASCII 'SER1' */

#define LE_SER_EDGE_RISING      1
#define LE_SER_EDGE_FALLING     2
#define LE_SER_EDGE_BOTH        3

typedef struct {
    uint16_t address;      /**< Process-image virtual address (%DIN, %DOUT, %B) */
    uint8_t  trigger_mode;  /**< LE_SER_EDGE_RISING, LE_SER_EDGE_FALLING, or LE_SER_EDGE_BOTH */
    uint8_t  reserved;
} le_ser_channel_t;

typedef struct {
    uint64_t timestamp_us;  /**< Microsecond timestamp of edge transition */
    uint32_t sequence_id;   /**< Monotonic sequence identifier */
    uint16_t address;       /**< Process-image address of transitioned channel */
    uint8_t  edge;          /**< LE_SER_EDGE_RISING or LE_SER_EDGE_FALLING */
    uint8_t  value;         /**< New logical state (1 or 0) */
} le_ser_event_t;

typedef struct {
    uint32_t magic;
    uint32_t sequence_counter;
    uint32_t overflow_count;
    uint16_t capacity;
    uint16_t count;
    uint16_t head;
    uint16_t tail;
} le_ser_nv_header_t;

/**
 * @brief Sequence of Events Recorder (SER) runtime state.
 */
typedef struct {
    uint16_t                channel_count;
    const le_ser_channel_t* channels;          /**< Monitored channel list */
    uint32_t                prev_state_words[LE_SER_STATE_WORDS]; /**< Bitfield of channel states on prior scan */
    uint16_t                capacity;          /**< Maximum events in ring buffer */
    uint16_t                count;             /**< Valid events in buffer */
    uint16_t                head;              /**< Write index */
    uint16_t                tail;              /**< Read index */
    uint32_t                sequence_counter;  /**< Monotonically increasing sequence ID */
    uint32_t                overflow_count;    /**< Events overwritten when buffer was full */

    le_ser_event_t*         events;            /**< Pointer to event buffer in RAM */

    /* Non-volatile persistence & timing */
    uint32_t                nv_offset;         /**< Logical NVRAM offset (e.g. LE_NVRAM_OFFSET_SER) */
    bool                    is_dirty;
    uint64_t                last_event_time_us; /**< Timestamp of most recent recorded event */
} le_ser_t;

/**
 * @brief Initializes the Sequence of Events Recorder (SER) instance.
 *
 * Configures the monitored digital channels, attaches the caller-provided event
 * ring buffer, and links the logical NVRAM partition. If @p nv_offset is non-zero,
 * attempts to restore existing event records and sequence numbers from NVRAM.
 *
 * @param ser Pointer to the SER structure to initialize.
 * @param channels Contiguous array of digital channel descriptors to monitor.
 * @param channel_count Number of channels in @p channels (clamped to `LE_SER_MAX_CHANNELS`).
 * @param event_buffer Pointer to pre-allocated RAM storage for the event ring buffer.
 * @param capacity Maximum number of @ref le_ser_event_t records @p event_buffer can hold.
 * @param nv_offset Logical NVRAM byte offset for persistence (defaults to `LE_NVRAM_OFFSET_SER` if 0).
 */
void le_ser_init(le_ser_t* ser, const le_ser_channel_t* channels, uint16_t channel_count,
                 le_ser_event_t* event_buffer, uint16_t capacity, uint32_t nv_offset);

/**
 * @brief Restores SER state and buffered events from non-volatile RAM.
 *
 * Reads the persistent header at `ser->nv_offset`, verifies `LE_SER_MAGIC` and
 * matching buffer capacity, and populates the in-memory ring buffer with historical
 * event records.
 *
 * @param ser Pointer to the initialized SER instance.
 * @return Returns `true` if valid SER records were restored; otherwise, `false`.
 */
bool le_ser_restore_nvram(le_ser_t* ser);

/**
 * @brief Flushes pending SER events and header metadata to non-volatile RAM.
 *
 * Serializes the header metadata (sequence counters, head/tail indices, overflow count)
 * and all active events into persistent NVRAM storage, clearing the dirty flag on success.
 *
 * @param ser Pointer to the SER instance to flush.
 * @return Returns `true` if NVRAM write succeeded; otherwise, `false`.
 */
bool le_ser_flush_nvram(le_ser_t* ser);

/**
 * @brief Executes an edge detection scan across all monitored channels.
 *
 * Scans the current boolean values of monitored channels in @p img, detects
 * rising or falling transitions relative to previous scan state, and pushes
 * transition events into the ring buffer with microsecond timestamps and monotonic IDs.
 *
 * @param ser Pointer to the SER instance.
 * @param img Pointer to the active process image containing live channel states.
 * @param now_us Monotonic microsecond timestamp of the current scan.
 */
void le_ser_scan(le_ser_t* ser, const le_process_image_t* img, uint64_t now_us);

/**
 * @brief Resets the SER event ring buffer.
 *
 * Clears all buffered events, resets head/tail indices and overflow counters, and
 * flushes the cleared state to NVRAM if persistence is enabled.
 *
 * @param ser Pointer to the SER instance to clear.
 */
void le_ser_clear(le_ser_t* ser);

/**
 * @brief Reads a range of chronological events from the ring buffer.
 *
 * Copies up to @p max_count events starting at offset @p start_idx from the current
 * ring tail (oldest event) into the caller's @p out_events buffer without modifying the ring.
 *
 * @param ser Pointer to the SER instance to read from.
 * @param start_idx Zero-based logical event offset from the oldest valid event.
 * @param out_events Destination array to receive copied @ref le_ser_event_t records.
 * @param max_count Maximum number of events to copy into @p out_events.
 * @return Total number of events successfully copied.
 */
uint16_t le_ser_read_events(const le_ser_t* ser, uint16_t start_idx, le_ser_event_t* out_events, uint16_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* LE_SER_H */

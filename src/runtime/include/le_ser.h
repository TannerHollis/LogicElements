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
 * @brief Initializes the SER instance with channel definitions and buffer.
 */
void le_ser_init(le_ser_t* ser, const le_ser_channel_t* channels, uint16_t channel_count,
                 le_ser_event_t* event_buffer, uint16_t capacity, uint32_t nv_offset);

/**
 * @brief Restores SER state and buffered events from NVRAM.
 */
bool le_ser_restore_nvram(le_ser_t* ser);

/**
 * @brief Flushes dirty SER events to NVRAM.
 */
bool le_ser_flush_nvram(le_ser_t* ser);

/**
 * @brief Executes edge detection scan across monitored channels at end of logic scan.
 */
void le_ser_scan(le_ser_t* ser, const le_process_image_t* img, uint64_t now_us);

/**
 * @brief Resets the SER event ring buffer.
 */
void le_ser_clear(le_ser_t* ser);

/**
 * @brief Reads up to max_count events from the ring buffer.
 */
uint16_t le_ser_read_events(const le_ser_t* ser, uint16_t start_idx, le_ser_event_t* out_events, uint16_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* LE_SER_H */

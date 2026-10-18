/**
 * @file le_ringbuf.h
 * @brief Reusable Single-Producer Single-Consumer (SPSC) Circular Byte Ring Buffer.
 *
 * Provides a lightweight, non-allocating, lock-free ring buffer designed for
 * asynchronous UART/CDC serial transmit and receive pipelines.
 *
 * Buffer capacity and defaults can be configured per board in @c le_board_config.h
 * via @ref LE_RINGBUF_DEFAULT_SIZE (defaults to 2048 bytes).
 */

#ifndef LE_RINGBUF_H
#define LE_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "le_types.h"
#include "le_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LE_RINGBUF_DEFAULT_SIZE
/** @brief Default buffer capacity in bytes for async serial ring buffers. */
#define LE_RINGBUF_DEFAULT_SIZE 2048u
#endif

/**
 * @brief Circular byte ring buffer structure.
 */
typedef struct {
    uint8_t*        buffer;    /**< Pointer to underlying contiguous byte array storage. */
    size_t          capacity;  /**< Total allocated capacity in bytes. */
    volatile size_t head;      /**< Write cursor index (modified by producer). */
    volatile size_t tail;      /**< Read cursor index (modified by consumer). */
} le_ringbuf_t;

/**
 * @brief Convenience macro declaring a static ring buffer and backing storage array.
 *
 * @param var_name Identifier name for the @ref le_ringbuf_t instance.
 * @param size_bytes Buffer capacity in bytes.
 */
#define LE_RINGBUF_DEFINE(var_name, size_bytes) \
    static uint8_t var_name##_storage[(size_bytes)]; \
    static le_ringbuf_t var_name = { \
        (var_name##_storage), \
        (size_bytes), \
        0, \
        0 \
    }

/**
 * @brief Initializes a ring buffer structure with caller-supplied storage.
 *
 * @param[out] rb Pointer to the @ref le_ringbuf_t instance to initialize.
 * @param[in] buffer Pointer to the contiguous byte storage array.
 * @param[in] capacity Total capacity of the storage array in bytes.
 */
void le_ringbuf_init(le_ringbuf_t* rb, uint8_t* buffer, size_t capacity);

/**
 * @brief Clears all queued bytes, resetting read and write cursors to zero.
 *
 * @param[in,out] rb Pointer to the ring buffer instance.
 */
void le_ringbuf_clear(le_ringbuf_t* rb);

/**
 * @brief Returns the total allocated byte capacity of the ring buffer.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @return Total storage capacity in bytes.
 */
size_t le_ringbuf_capacity(const le_ringbuf_t* rb);

/**
 * @brief Returns the number of unread bytes currently stored in the ring buffer.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @return Number of readable bytes.
 */
size_t le_ringbuf_available(const le_ringbuf_t* rb);

/**
 * @brief Returns the remaining free write space available in bytes.
 *
 * Note: One byte is reserved to distinguish between completely full and empty states.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @return Number of bytes that can be written before the buffer is full.
 */
size_t le_ringbuf_space(const le_ringbuf_t* rb);

/**
 * @brief Checks if the ring buffer contains zero readable bytes.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @return True if empty; false otherwise.
 */
bool le_ringbuf_is_empty(const le_ringbuf_t* rb);

/**
 * @brief Checks if the ring buffer cannot accept additional bytes without overflowing.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @return True if full; false otherwise.
 */
bool le_ringbuf_is_full(const le_ringbuf_t* rb);

/**
 * @brief Enqueues a single byte into the ring buffer.
 *
 * @param[in,out] rb Pointer to the ring buffer instance.
 * @param[in] byte Byte value to append.
 * @return True if byte was enqueued; false if buffer was full.
 */
bool le_ringbuf_put(le_ringbuf_t* rb, uint8_t byte);

/**
 * @brief Dequeues a single byte from the ring buffer.
 *
 * @param[in,out] rb Pointer to the ring buffer instance.
 * @param[out] byte Pointer to receive the read byte (may be NULL to discard).
 * @return True if byte was retrieved; false if buffer was empty.
 */
bool le_ringbuf_get(le_ringbuf_t* rb, uint8_t* byte);

/**
 * @brief Peeks at a byte at a specific offset from the current read cursor without advancing it.
 *
 * @param[in] rb Pointer to the ring buffer instance.
 * @param[in] offset Zero-based offset from the current read cursor (0 <= offset < available).
 * @param[out] byte Pointer to receive the inspected byte.
 * @return True if byte was successfully read; false if offset is out of bounds.
 */
bool le_ringbuf_peek(const le_ringbuf_t* rb, size_t offset, uint8_t* byte);

/**
 * @brief Writes a block of bytes into the ring buffer.
 *
 * If the write length exceeds remaining space, writes up to @ref le_ringbuf_space bytes.
 *
 * @param[in,out] rb Pointer to the ring buffer instance.
 * @param[in] data Pointer to the source data buffer.
 * @param[in] len Number of bytes to write.
 * @return Actual number of bytes written.
 */
size_t le_ringbuf_write(le_ringbuf_t* rb, const uint8_t* data, size_t len);

/**
 * @brief Reads a block of bytes from the ring buffer.
 *
 * If requested bytes exceed available data, reads all currently available bytes.
 *
 * @param[in,out] rb Pointer to the ring buffer instance.
 * @param[out] data Destination buffer (may be NULL to skip/discard bytes).
 * @param[in] max_len Maximum number of bytes to retrieve.
 * @return Actual number of bytes read or discarded.
 */
size_t le_ringbuf_read(le_ringbuf_t* rb, uint8_t* data, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* LE_RINGBUF_H */

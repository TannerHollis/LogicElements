/**
 * @file le_ringbuf.c
 * @brief Implementation of reusable circular byte ring buffer.
 */

#include "le_ringbuf.h"
#include <string.h>

void le_ringbuf_init(le_ringbuf_t* rb, uint8_t* buffer, size_t capacity)
{
    if (!rb) return;
    rb->buffer = buffer;
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
}

void le_ringbuf_clear(le_ringbuf_t* rb)
{
    if (!rb) return;
    rb->head = 0;
    rb->tail = 0;
}

size_t le_ringbuf_capacity(const le_ringbuf_t* rb)
{
    return rb ? rb->capacity : 0;
}

size_t le_ringbuf_available(const le_ringbuf_t* rb)
{
    if (!rb || !rb->buffer || rb->capacity == 0) return 0;

    size_t h = rb->head;
    size_t t = rb->tail;
    if (h >= t) {
        return h - t;
    }
    return rb->capacity - (t - h);
}

size_t le_ringbuf_space(const le_ringbuf_t* rb)
{
    if (!rb || !rb->buffer || rb->capacity <= 1) return 0;
    size_t avail = le_ringbuf_available(rb);
    return rb->capacity - 1 - avail;
}

bool le_ringbuf_is_empty(const le_ringbuf_t* rb)
{
    return !rb || (rb->head == rb->tail);
}

bool le_ringbuf_is_full(const le_ringbuf_t* rb)
{
    if (!rb || !rb->buffer || rb->capacity <= 1) return true;
    size_t next_h = rb->head + 1;
    if (next_h >= rb->capacity) next_h = 0;
    return (next_h == rb->tail);
}

bool le_ringbuf_put(le_ringbuf_t* rb, uint8_t byte)
{
    if (!rb || !rb->buffer || rb->capacity <= 1) return false;

    size_t next_h = rb->head + 1;
    if (next_h >= rb->capacity) next_h = 0;
    if (next_h == rb->tail) {
        return false; /* Buffer is full */
    }

    rb->buffer[rb->head] = byte;
    rb->head = next_h;
    return true;
}

bool le_ringbuf_get(le_ringbuf_t* rb, uint8_t* byte)
{
    if (!rb || !rb->buffer || rb->head == rb->tail) {
        return false; /* Buffer is empty */
    }

    if (byte) {
        *byte = rb->buffer[rb->tail];
    }
    size_t next_t = rb->tail + 1;
    if (next_t >= rb->capacity) next_t = 0;
    rb->tail = next_t;
    return true;
}

bool le_ringbuf_peek(const le_ringbuf_t* rb, size_t offset, uint8_t* byte)
{
    if (!rb || !rb->buffer || !byte) return false;
    if (offset >= le_ringbuf_available(rb)) return false;

    size_t idx = (rb->tail + offset) % rb->capacity;
    *byte = rb->buffer[idx];
    return true;
}

size_t le_ringbuf_write(le_ringbuf_t* rb, const uint8_t* data, size_t len)
{
    if (!rb || !rb->buffer || !data || len == 0 || rb->capacity <= 1) return 0;

    size_t space = le_ringbuf_space(rb);
    if (len > space) len = space;
    if (len == 0) return 0;

    size_t h = rb->head;
    size_t cap = rb->capacity;
    size_t chunk1 = cap - h;
    if (chunk1 > len) chunk1 = len;

    memcpy(&rb->buffer[h], data, chunk1);
    if (len > chunk1) {
        memcpy(&rb->buffer[0], data + chunk1, len - chunk1);
        rb->head = len - chunk1;
    } else {
        size_t next_h = h + chunk1;
        if (next_h >= cap) next_h = 0;
        rb->head = next_h;
    }

    return len;
}

size_t le_ringbuf_read(le_ringbuf_t* rb, uint8_t* data, size_t max_len)
{
    if (!rb || !rb->buffer || rb->capacity <= 1) return 0;

    size_t avail = le_ringbuf_available(rb);
    if (max_len > avail) max_len = avail;
    if (max_len == 0) return 0;

    size_t t = rb->tail;
    size_t cap = rb->capacity;
    size_t chunk1 = cap - t;
    if (chunk1 > max_len) chunk1 = max_len;

    if (data) {
        memcpy(data, &rb->buffer[t], chunk1);
        if (max_len > chunk1) {
            memcpy(data + chunk1, &rb->buffer[0], max_len - chunk1);
        }
    }

    if (max_len > chunk1) {
        rb->tail = max_len - chunk1;
    } else {
        size_t next_t = t + chunk1;
        if (next_t >= cap) next_t = 0;
        rb->tail = next_t;
    }

    return max_len;
}

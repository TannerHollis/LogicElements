/**
 * @file le_ser.c
 * @brief Implementation of Sequence of Events Recorder (SER).
 */

#include "le_ser.h"
#include <string.h>

void le_ser_init(le_ser_t* ser, const le_ser_channel_t* channels, uint16_t channel_count,
                 le_ser_event_t* event_buffer, uint16_t capacity, uint32_t nv_offset)
{
    if (!ser) return;
    memset(ser, 0, sizeof(*ser));

    if (channel_count > LE_SER_MAX_CHANNELS) channel_count = LE_SER_MAX_CHANNELS;
    ser->channel_count = channel_count;
    ser->channels = channels;

    ser->events = event_buffer;
    ser->capacity = capacity;
    ser->nv_offset = nv_offset ? nv_offset : LE_NVRAM_OFFSET_SER;
    ser->is_dirty = false;
    ser->last_event_time_us = 0;

    if (ser->nv_offset > 0) {
        le_ser_restore_nvram(ser);
    }
}

bool le_ser_restore_nvram(le_ser_t* ser)
{
    if (!ser || ser->nv_offset == 0 || !ser->events) return false;

    le_ser_nv_header_t hdr;
    if (!le_nvram_read(ser->nv_offset, &hdr, sizeof(hdr))) {
        return false;
    }
    if (hdr.magic != LE_SER_MAGIC || hdr.capacity != ser->capacity) {
        return false;
    }

    ser->count = hdr.count;
    ser->head = hdr.head;
    ser->tail = hdr.tail;
    ser->sequence_counter = hdr.sequence_counter;
    ser->overflow_count = hdr.overflow_count;

    if (ser->count > 0 && ser->count <= ser->capacity) {
        uint32_t ev_off = ser->nv_offset + (uint32_t)sizeof(le_ser_nv_header_t);
        le_nvram_read(ev_off, ser->events, (size_t)ser->capacity * sizeof(le_ser_event_t));
    }
    ser->is_dirty = false;
    return true;
}

bool le_ser_flush_nvram(le_ser_t* ser)
{
    if (!ser || ser->nv_offset == 0 || !ser->events) return false;

    le_ser_nv_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = LE_SER_MAGIC;
    hdr.capacity = ser->capacity;
    hdr.count = ser->count;
    hdr.head = ser->head;
    hdr.tail = ser->tail;
    hdr.sequence_counter = ser->sequence_counter;
    hdr.overflow_count = ser->overflow_count;

    if (!le_nvram_write(ser->nv_offset, &hdr, sizeof(hdr))) {
        return false;
    }

    if (ser->count > 0) {
        uint32_t ev_off = ser->nv_offset + (uint32_t)sizeof(le_ser_nv_header_t);
        le_nvram_write(ev_off, ser->events, (size_t)ser->capacity * sizeof(le_ser_event_t));
    }
    ser->is_dirty = false;
    return true;
}

void le_ser_scan(le_ser_t* ser, const le_process_image_t* img, uint64_t now_us)
{
    if (!ser || !img || !ser->channels || ser->channel_count == 0) return;

    uint32_t num_words = (ser->channel_count + 31) / 32;
    for (uint32_t w = 0; w < num_words; w++) {
        uint32_t start_ch = w * 32;
        uint32_t end_ch = start_ch + 32;
        if (end_ch > ser->channel_count) end_ch = ser->channel_count;

        uint32_t curr_bits = 0;
        for (uint32_t i = start_ch; i < end_ch; i++) {
            if (le_process_image_get_bool(img, ser->channels[i].address)) {
                curr_bits |= (1UL << (i - start_ch));
            }
        }

        uint32_t changed = curr_bits ^ ser->prev_state_words[w];
        if (changed != 0) {
            for (uint32_t i = start_ch; i < end_ch; i++) {
                uint32_t bit_pos = 1UL << (i - start_ch);
                if (changed & bit_pos) {
                    bool new_val = (curr_bits & bit_pos) != 0;
                    uint8_t edge = new_val ? (uint8_t)LE_SER_EDGE_RISING : (uint8_t)LE_SER_EDGE_FALLING;
                    uint8_t mode = ser->channels[i].trigger_mode;
                    if (mode == LE_SER_EDGE_BOTH || mode == edge) {
                        if (ser->capacity > 0 && ser->events) {
                            le_ser_event_t ev;
                            ev.timestamp_us = now_us;
                            ev.address = ser->channels[i].address;
                            ev.edge = edge;
                            ev.value = new_val ? 1 : 0;
                            ev.sequence_id = ++ser->sequence_counter;

                            ser->events[ser->head] = ev;
                            ser->head = (ser->head + 1) % ser->capacity;
                            if (ser->count < ser->capacity) {
                                ser->count++;
                            } else {
                                ser->tail = (ser->tail + 1) % ser->capacity;
                                ser->overflow_count++;
                            }
                            ser->is_dirty = true;
                            ser->last_event_time_us = now_us;
                        }
                    }
                }
            }
            ser->prev_state_words[w] = curr_bits;
        }
    }
}

void le_ser_clear(le_ser_t* ser)
{
    if (!ser) return;
    ser->count = 0;
    ser->head = 0;
    ser->tail = 0;
    ser->overflow_count = 0;
    ser->is_dirty = true;
    if (ser->nv_offset > 0) {
        le_ser_flush_nvram(ser);
    }
}

uint16_t le_ser_read_events(const le_ser_t* ser, uint16_t start_idx, le_ser_event_t* out_events, uint16_t max_count)
{
    if (!ser || !out_events || max_count == 0 || ser->count == 0 || !ser->events) return 0;
    if (start_idx >= ser->count) return 0;

    uint16_t to_read = ser->count - start_idx;
    if (to_read > max_count) to_read = max_count;

    for (uint16_t i = 0; i < to_read; i++) {
        uint16_t idx = (ser->tail + start_idx + i) % ser->capacity;
        out_events[i] = ser->events[idx];
    }
    return to_read;
}

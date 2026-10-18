/**
 * @file le_event_rec.c
 * @brief Implementation of Multi-Level Event Recorder (Oscillography).
 */

#include "le_event_rec.h"
#include <string.h>

static uint16_t le_event_rec_addr_size(uint16_t addr)
{
    uint16_t region = addr & LE_ADDR_REGION_MASK;
    switch (region) {
        case LE_REGION_DIN:
        case LE_REGION_DOUT:
        case LE_REGION_BOOL_REG:
        case LE_REGION_BOOL_REG_EXT:
            return 1;
        case LE_REGION_FLOAT:
        case LE_REGION_FLOAT_EXT:
        case LE_REGION_INT_REG:
        case LE_REGION_INT_REG_EXT:
        case LE_REGION_AIN:
        case LE_REGION_AOUT:
            return 4;
#if LE_ENABLE_COMPLEX
        case LE_REGION_CMPLX:
            return 8;
#endif
        default:
            return 4;
    }
}

uint32_t le_event_rec_calc_level_bytes(const le_event_level_cfg_t* lvl)
{
    if (!lvl || !lvl->enabled || lvl->capacity == 0) return 0;
    uint32_t row_size = 8; /* 8-byte timestamp_us */
    for (uint16_t c = 0; c < lvl->channel_count; c++) {
        row_size += (uint32_t)le_event_rec_addr_size(lvl->channels[c]);
    }
    return (uint32_t)lvl->capacity * row_size;
}

static void le_event_rec_pack_sample(uint8_t* dst, const le_event_level_cfg_t* cfg, const le_process_image_t* img, uint64_t now_us)
{
    memcpy(dst, &now_us, sizeof(now_us));
    uint32_t offset = 8;
    for (uint16_t c = 0; c < cfg->channel_count; c++) {
        uint16_t addr = cfg->channels[c];
        uint16_t region = addr & LE_ADDR_REGION_MASK;
        if (region == LE_REGION_DIN || region == LE_REGION_DOUT ||
            region == LE_REGION_BOOL_REG || region == LE_REGION_BOOL_REG_EXT) {
            uint8_t b = le_process_image_get_bool(img, addr) ? 1 : 0;
            dst[offset] = b;
            offset += 1;
        } else if (region == LE_REGION_INT_REG || region == LE_REGION_INT_REG_EXT) {
            int32_t v = le_process_image_get_int(img, addr);
            memcpy(dst + offset, &v, 4);
            offset += 4;
        }
#if LE_ENABLE_COMPLEX
        else if (region == LE_REGION_CMPLX) {
            le_complex_t cplx = le_process_image_get_complex(img, addr);
            memcpy(dst + offset, &cplx, 8);
            offset += 8;
        }
#endif
        else {
            float f = le_process_image_get_float(img, addr);
            memcpy(dst + offset, &f, 4);
            offset += 4;
        }
    }
}

void le_event_rec_init(le_event_rec_t* rec, const le_event_rec_cfg_t* cfg,
                       uint8_t* ram_pool, size_t pool_size, uint32_t nv_offset)
{
    if (!rec) return;
    memset(rec, 0, sizeof(*rec));

    if (!cfg || cfg->active_levels == 0) return;

    rec->trigger_address = cfg->trigger_address;
    rec->trigger_edge = cfg->trigger_edge ? cfg->trigger_edge : (uint8_t)LE_SER_EDGE_RISING;
    rec->active_levels = cfg->active_levels;
    rec->nv_offset = nv_offset ? nv_offset : le_nvram_get_event_rec_offset();

    size_t pool_used = 0;
    for (uint8_t i = 0; i < cfg->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        rec->levels[i].cfg = cfg->levels[i];
        if (rec->levels[i].cfg.rate_div == 0) rec->levels[i].cfg.rate_div = 1;
        uint32_t row_size = 8;
        for (uint16_t c = 0; c < rec->levels[i].cfg.channel_count; c++) {
            row_size += (uint32_t)le_event_rec_addr_size(rec->levels[i].cfg.channels[c]);
        }
        rec->levels[i].cfg.sample_size = (uint16_t)row_size;

        uint32_t needed = (uint32_t)rec->levels[i].cfg.capacity * row_size;
        rec->levels[i].buffer_bytes = needed;

        if (ram_pool && (pool_used + needed <= pool_size)) {
            rec->levels[i].sample_buffer = ram_pool + pool_used;
            memset(rec->levels[i].sample_buffer, 0, needed);
            pool_used += needed;
        } else {
            rec->levels[i].sample_buffer = NULL;
        }
    }

    if (rec->nv_offset > 0) {
        le_event_rec_restore_nvram(rec);
    }
}

bool le_event_rec_restore_nvram(le_event_rec_t* rec)
{
    if (!rec || rec->nv_offset == 0) return false;

    le_event_rec_nv_header_t hdr;
    if (!le_nvram_read(rec->nv_offset, &hdr, sizeof(hdr))) {
        return false;
    }
    if (hdr.magic != LE_EVENT_REC_MAGIC) {
        return false;
    }

    uint32_t buf_offs = rec->nv_offset + (uint32_t)sizeof(le_event_rec_nv_header_t);
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        rec->levels[i].state = hdr.levels[i];
        if (rec->levels[i].sample_buffer && rec->levels[i].buffer_bytes > 0) {
            le_nvram_read(buf_offs, rec->levels[i].sample_buffer, rec->levels[i].buffer_bytes);
        }
        buf_offs += rec->levels[i].buffer_bytes;
    }
    rec->is_dirty = false;
    return true;
}

bool le_event_rec_flush_nvram(le_event_rec_t* rec)
{
    if (!rec || rec->nv_offset == 0) return false;

    le_event_rec_nv_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = LE_EVENT_REC_MAGIC;
    hdr.active_levels = rec->active_levels;
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        hdr.levels[i] = rec->levels[i].state;
    }

    if (!le_nvram_write(rec->nv_offset, &hdr, sizeof(hdr))) {
        return false;
    }

    uint32_t buf_offs = rec->nv_offset + (uint32_t)sizeof(le_event_rec_nv_header_t);
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        if (rec->levels[i].sample_buffer && rec->levels[i].buffer_bytes > 0) {
            le_nvram_write(buf_offs, rec->levels[i].sample_buffer, rec->levels[i].buffer_bytes);
        }
        buf_offs += rec->levels[i].buffer_bytes;
    }
    rec->is_dirty = false;
    return true;
}

void le_event_rec_scan(le_event_rec_t* rec, const le_process_image_t* img, uint32_t scan_seq, uint64_t now_us)
{
    if (!rec || !img || rec->active_levels == 0) return;

    /* 1. Check trigger condition */
    bool trig_curr = le_process_image_get_bool(img, rec->trigger_address);
    bool trig_prev = (rec->prev_trigger_state != 0);
    bool trig_fired = false;

    if (rec->trigger_edge == LE_SER_EDGE_RISING && (!trig_prev && trig_curr)) {
        trig_fired = true;
    } else if (rec->trigger_edge == LE_SER_EDGE_FALLING && (trig_prev && !trig_curr)) {
        trig_fired = true;
    } else if (rec->trigger_edge == LE_SER_EDGE_BOTH && (trig_prev != trig_curr)) {
        trig_fired = true;
    }
    rec->prev_trigger_state = trig_curr ? 1 : 0;

    /* 2. Process each active level */
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        le_event_rec_level_t* lvl = &rec->levels[i];
        if (!lvl->cfg.enabled || !lvl->sample_buffer || lvl->cfg.capacity == 0) continue;

        if (trig_fired && !lvl->state.triggered && !lvl->state.completed) {
            lvl->state.triggered = 1;
            lvl->state.trigger_seq = scan_seq;
            lvl->state.trigger_time_us = now_us;
            uint16_t post = (lvl->cfg.capacity > lvl->cfg.pre_trigger)
                            ? (lvl->cfg.capacity - lvl->cfg.pre_trigger)
                            : 1;
            lvl->state.post_remaining = post;
            rec->is_dirty = true;
            rec->last_activity_time_us = now_us;
        }

        if (lvl->state.completed) continue;

        if ((scan_seq % lvl->cfg.rate_div) == 0) {
            uint32_t row_bytes = lvl->cfg.sample_size;
            uint32_t row_offset = lvl->state.write_index * row_bytes;
            if (row_offset + row_bytes <= lvl->buffer_bytes) {
                le_event_rec_pack_sample(lvl->sample_buffer + row_offset, &lvl->cfg, img, now_us);
            }

            lvl->state.write_index = (lvl->state.write_index + 1) % lvl->cfg.capacity;
            if (lvl->state.sample_count < lvl->cfg.capacity) {
                lvl->state.sample_count++;
            }

            if (lvl->state.triggered) {
                if (lvl->state.post_remaining > 0) {
                    lvl->state.post_remaining--;
                    if (lvl->state.post_remaining == 0) {
                        lvl->state.completed = 1;
                        rec->is_dirty = true;
                        if (rec->nv_offset > 0) {
                            le_event_rec_flush_nvram(rec);
                        }
                    }
                }
            }
        }
    }
}

bool le_event_rec_is_capturing(const le_event_rec_t* rec)
{
    if (!rec) return false;
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        if (rec->levels[i].state.triggered && !rec->levels[i].state.completed) {
            return true;
        }
    }
    return false;
}

void le_event_rec_arm(le_event_rec_t* rec)
{
    if (!rec) return;
    for (uint8_t i = 0; i < rec->active_levels && i < LE_EVENT_REC_MAX_LEVELS; i++) {
        rec->levels[i].state.triggered = 0;
        rec->levels[i].state.completed = 0;
        rec->levels[i].state.post_remaining = 0;
        rec->levels[i].state.write_index = 0;
        rec->levels[i].state.sample_count = 0;
    }
    rec->is_dirty = true;
    if (rec->nv_offset > 0) {
        le_event_rec_flush_nvram(rec);
    }
}

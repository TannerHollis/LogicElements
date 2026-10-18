/**
 * @file le_nvram.c
 * @brief Implementation of Non-Volatile RAM (NVRAM) and wear-leveling management.
 */

#include "le_nvram.h"
#include "le_storage.h"
#include "le_loader.h"
#include "le_hal.h"
#include "le_ser.h"
#include "le_event_rec.h"
#include <string.h>

/* ========================================================================== */
/* Internal State & Modes                                                     */
/* ========================================================================== */

typedef enum {
    NVRAM_MODE_DIRECT = 0, /**< Direct byte NVRAM (FRAM, EEPROM, battery SRAM, or HAL) */
    NVRAM_MODE_FLASH  = 1  /**< Wear-leveled Flash journal across circular sectors */
} le_nvram_mode_t;

static le_nvram_mode_t      s_nvram_mode = NVRAM_MODE_DIRECT;
static le_flash_wl_driver_t s_flash_driver;
static bool                 s_nvram_initialized = false;

/* Logical NVRAM working image cache:
 * Dedicated cache for Manifest & Retentive Registers (%B, %I, %F, %C).
 * Streaming recording buffers (SER log, Event Recorder) route directly through HAL / Flash
 * via the circular queue architecture without requiring multi-megabyte RAM mirrors. */
#ifndef LE_NVRAM_CACHE_SIZE
#define LE_NVRAM_CACHE_SIZE         12288UL
#endif

static uint8_t                      s_nvram_cache[LE_NVRAM_CACHE_SIZE] = {0};

/* Flash journal active tracking */
static uint32_t             s_active_sector_idx = 0;
static uint32_t             s_active_sector_seq = 0;
static uint32_t             s_active_sector_erase_count = 0;
static uint32_t             s_active_sector_write_pos = 0; /**< Byte offset within active sector */

/* Dynamic manifest tracking */
static le_nvram_manifest_t  s_manifest;
static bool                 s_manifest_valid = false;

/* ========================================================================== */
/* Manifest Helper                                                            */
/* ========================================================================== */

static void nvram_populate_default_manifest(le_nvram_manifest_t* m)
{
    memset(m, 0, sizeof(*m));
    m->magic = LE_NVRAM_MAGIC;
    m->version = LE_NVRAM_VERSION;
    m->flags = 0;
    m->total_bytes = LE_NVRAM_TOTAL_BYTES;
    m->config_slots = (LE_MAX_CONFIG_SLOTS > 0) ? LE_MAX_CONFIG_SLOTS : 1;

    /* Base manifest size aligned to 64 bytes */
    uint32_t manifest_size = ((sizeof(le_nvram_manifest_t) + 63) / 64) * 64;
    uint32_t avail = (LE_NVRAM_TOTAL_BYTES > manifest_size) ? (LE_NVRAM_TOTAL_BYTES - manifest_size) : 0;

    /* Default fallback: balanced partition if unconfigured by a circuit */
    uint32_t regs_total = m->config_slots * LE_NVRAM_DEFAULT_REGS_BYTES;
    if (regs_total > avail / 4) regs_total = avail / 4;
    m->regs_per_slot = (m->config_slots > 0) ? (regs_total / m->config_slots) : 0;
    m->regs_offset = manifest_size;
    m->regs_bytes = regs_total;

    uint32_t ser_total = LE_NVRAM_DEFAULT_SER_BYTES;
    if (regs_total + ser_total > avail) {
        ser_total = (avail > regs_total) ? (avail - regs_total) : 0;
    }
    m->ser_offset = m->regs_offset + m->regs_bytes;
    m->ser_bytes = ser_total;

    m->event_rec_offset = m->ser_offset + m->ser_bytes;
    m->event_rec_bytes = (avail > (m->regs_bytes + m->ser_bytes)) ?
                         (avail - (m->regs_bytes + m->ser_bytes)) : 0;
    m->crc32 = le_crc32((const uint8_t*)m, offsetof(le_nvram_manifest_t, crc32));
}

static void nvram_ensure_manifest(void)
{
    if (s_manifest_valid) return;

    /* Try reading existing manifest from offset 0 */
    le_nvram_manifest_t m;
    if (le_nvram_read(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m))) {
        uint32_t calc_crc = le_crc32((const uint8_t*)&m, offsetof(le_nvram_manifest_t, crc32));
        if (m.magic == LE_NVRAM_MAGIC && m.crc32 == calc_crc) {
            s_manifest = m;
            s_manifest_valid = true;
            return;
        }
    }

    /* Fallback default manifest */
    nvram_populate_default_manifest(&s_manifest);
    s_manifest_valid = true;
}

/* ========================================================================== */
/* Flash Wear-Leveling Internal Implementation                                */
/* ========================================================================== */

static uint32_t wl_sector_phys_addr(uint32_t sector_idx)
{
    return s_flash_driver.flash_base_offset + (sector_idx * s_flash_driver.sector_size);
}

static bool wl_format_sector(uint32_t sector_idx, uint32_t seq_num, uint32_t erase_count)
{
    uint32_t phys = wl_sector_phys_addr(sector_idx);
    if (!s_flash_driver.erase_sector(sector_idx, phys)) {
        return false;
    }

    le_wl_sector_header_t hdr;
    hdr.magic = LE_WL_SECTOR_MAGIC;
    hdr.seq_num = seq_num;
    hdr.erase_count = erase_count;
    hdr.crc32 = le_crc32((const uint8_t*)&hdr, offsetof(le_wl_sector_header_t, crc32));

    if (!s_flash_driver.program(phys, (const uint8_t*)&hdr, sizeof(hdr))) {
        return false;
    }

    return true;
}

static bool wl_rollover_and_compact(void)
{
    uint32_t next_sector_idx = (s_active_sector_idx + 1) % s_flash_driver.sector_count;
    uint32_t next_seq = s_active_sector_seq + 1;
    uint32_t next_erase = s_active_sector_erase_count + 1;

    /* Format next sector */
    if (!wl_format_sector(next_sector_idx, next_seq, next_erase)) {
        return false;
    }

    uint32_t next_phys = wl_sector_phys_addr(next_sector_idx);
    uint32_t write_pos = sizeof(le_wl_sector_header_t);

    /* Write non-zero blocks of current logical NVRAM cache into new active sector */
    const uint32_t chunk_size = 128;
    const uint32_t max_cache_bytes = (LE_NVRAM_TOTAL_BYTES < LE_NVRAM_CACHE_SIZE) ? LE_NVRAM_TOTAL_BYTES : LE_NVRAM_CACHE_SIZE;
    for (uint32_t off = 0; off < max_cache_bytes; off += chunk_size) {
        uint32_t cur = (max_cache_bytes - off < chunk_size) ? (max_cache_bytes - off) : chunk_size;

        /* Check if chunk has any non-zero bytes */
        bool is_nonzero = false;
        for (uint32_t k = 0; k < cur; k++) {
            if (s_nvram_cache[off + k] != 0) {
                is_nonzero = true;
                break;
            }
        }
        if (!is_nonzero) continue;

        if (write_pos + sizeof(le_wl_record_header_t) + cur > s_flash_driver.sector_size) {
            break;
        }

        le_wl_record_header_t rhdr;
        rhdr.marker = LE_WL_RECORD_MARKER;
        rhdr.len = (uint16_t)cur;
        rhdr.nv_offset = off;
        rhdr.crc32 = le_crc32(&s_nvram_cache[off], cur);

        if (!s_flash_driver.program(next_phys + write_pos, (const uint8_t*)&rhdr, sizeof(rhdr))) {
            return false;
        }
        write_pos += sizeof(rhdr);

        if (!s_flash_driver.program(next_phys + write_pos, &s_nvram_cache[off], cur)) {
            return false;
        }
        write_pos += cur;
    }

    /* Switch active sector */
    s_active_sector_idx = next_sector_idx;
    s_active_sector_seq = next_seq;
    s_active_sector_erase_count = next_erase;
    s_active_sector_write_pos = write_pos;

    return true;
}

static bool wl_mount_scan(void)
{
    bool found_valid_sector = false;
    uint32_t best_seq = 0;
    uint32_t best_idx = 0;
    uint32_t best_erase = 0;

    for (uint32_t i = 0; i < s_flash_driver.sector_count; i++) {
        uint32_t phys = wl_sector_phys_addr(i);
        le_wl_sector_header_t shdr;
        if (!s_flash_driver.read(phys, (uint8_t*)&shdr, sizeof(shdr))) {
            continue;
        }

        if (shdr.magic == LE_WL_SECTOR_MAGIC) {
            uint32_t expected_crc = le_crc32((const uint8_t*)&shdr, offsetof(le_wl_sector_header_t, crc32));
            if (expected_crc == shdr.crc32) {
                if (!found_valid_sector || shdr.seq_num > best_seq) {
                    found_valid_sector = true;
                    best_seq = shdr.seq_num;
                    best_idx = i;
                    best_erase = shdr.erase_count;
                }
            }
        }
    }

    if (!found_valid_sector) {
        /* Unformatted storage: format sector 0 and write default manifest */
        s_active_sector_idx = 0;
        s_active_sector_seq = 1;
        s_active_sector_erase_count = 1;
        s_active_sector_write_pos = sizeof(le_wl_sector_header_t);

        if (!wl_format_sector(0, 1, 1)) {
            return false;
        }

        le_nvram_manifest_t m;
        nvram_populate_default_manifest(&m);
        memcpy(&s_nvram_cache[LE_NVRAM_OFFSET_MANIFEST], &m, sizeof(m));

        /* Write manifest record */
        le_wl_record_header_t rhdr;
        rhdr.marker = LE_WL_RECORD_MARKER;
        rhdr.len = (uint16_t)sizeof(m);
        rhdr.nv_offset = LE_NVRAM_OFFSET_MANIFEST;
        rhdr.crc32 = le_crc32((const uint8_t*)&m, sizeof(m));

        uint32_t phys = wl_sector_phys_addr(0);
        s_flash_driver.program(phys + s_active_sector_write_pos, (const uint8_t*)&rhdr, sizeof(rhdr));
        s_active_sector_write_pos += sizeof(rhdr);
        s_flash_driver.program(phys + s_active_sector_write_pos, (const uint8_t*)&m, sizeof(m));
        s_active_sector_write_pos += sizeof(m);

        return true;
    }

    /* Valid sector found: mount and replay valid log records into cache */
    s_active_sector_idx = best_idx;
    s_active_sector_seq = best_seq;
    s_active_sector_erase_count = best_erase;

    uint32_t phys = wl_sector_phys_addr(best_idx);
    uint32_t pos = sizeof(le_wl_sector_header_t);
    uint8_t temp_buf[512];

    while (pos + sizeof(le_wl_record_header_t) <= s_flash_driver.sector_size) {
        le_wl_record_header_t rhdr;
        if (!s_flash_driver.read(phys + pos, (uint8_t*)&rhdr, sizeof(rhdr))) {
            break;
        }

        if (rhdr.marker != LE_WL_RECORD_MARKER || rhdr.len == 0 || rhdr.len > sizeof(temp_buf)) {
            break; /* Reached end of valid written records in sector */
        }

        if (pos + sizeof(rhdr) + rhdr.len > s_flash_driver.sector_size) {
            break;
        }

        if (rhdr.nv_offset + rhdr.len > LE_NVRAM_TOTAL_BYTES) {
            break;
        }

        if (!s_flash_driver.read(phys + pos + sizeof(rhdr), temp_buf, rhdr.len)) {
            break;
        }

        uint32_t crc = le_crc32(temp_buf, rhdr.len);
        if (crc != rhdr.crc32) {
            break; /* Corrupted or interrupted write, stop replay */
        }

        /* Apply valid record to logical cache if within cache bounds */
        if (rhdr.nv_offset < LE_NVRAM_CACHE_SIZE) {
            size_t copy_len = rhdr.len;
            if (rhdr.nv_offset + copy_len > LE_NVRAM_CACHE_SIZE) {
                copy_len = LE_NVRAM_CACHE_SIZE - rhdr.nv_offset;
            }
            memcpy(&s_nvram_cache[rhdr.nv_offset], temp_buf, copy_len);
        }
        pos += sizeof(rhdr) + rhdr.len;
    }

    s_active_sector_write_pos = pos;
    return true;
}

/* ========================================================================== */
/* Public API Functions                                                       */
/* ========================================================================== */

bool le_nvram_init_direct(void)
{
    s_nvram_mode = NVRAM_MODE_DIRECT;
    s_nvram_initialized = true;
    s_manifest_valid = false;

    /* Pre-load cache from physical NVRAM if HAL read callback is provided */
    if (g_le_hal && g_le_hal->nvram_read &&
        (void*)g_le_hal->nvram_read != (void*)le_nvram_read) {
        g_le_hal->nvram_read(0, s_nvram_cache, LE_NVRAM_CACHE_SIZE);
    }

    nvram_ensure_manifest();
    return true;
}

bool le_nvram_init_flash(const le_flash_wl_driver_t* driver)
{
    static le_flash_wl_driver_t s_auto_driver;
    if (!driver) {
        if (g_le_hal && g_le_hal->nvram_erase_sector && g_le_hal->nvram_write_page && g_le_hal->nvram_read) {
            uint32_t sec_sz = (g_board_nvram_sector.erase_size > 0) ? g_board_nvram_sector.erase_size : LE_NVRAM_BLOCK_SIZE;
            uint32_t total_sz = (g_board_nvram_sector.size_bytes > 0) ? g_board_nvram_sector.size_bytes : LE_NVRAM_TOTAL_BYTES;
            uint32_t cnt = (sec_sz > 0) ? (total_sz / sec_sz) : 4;
            if (cnt < 2) cnt = 2;

            s_auto_driver.flash_base_offset = g_board_nvram_sector.phys_addr;
            s_auto_driver.sector_size = sec_sz;
            s_auto_driver.page_size = (g_board_nvram_sector.page_size > 0) ? g_board_nvram_sector.page_size : LE_NVRAM_PAGE_SIZE;
            s_auto_driver.sector_count = cnt;
            s_auto_driver.max_erase_cycles = LE_NVRAM_MAX_ERASE_CYCLES;
            s_auto_driver.erase_sector = g_le_hal->nvram_erase_sector;
            s_auto_driver.program = g_le_hal->nvram_write_page;
            s_auto_driver.read = (bool (*)(uint32_t, uint8_t*, size_t))g_le_hal->nvram_read;
            driver = &s_auto_driver;
        } else {
            return false;
        }
    }

    if (driver->sector_size == 0 || driver->sector_count < 2 ||
        !driver->erase_sector || !driver->program || !driver->read) {
        return false;
    }

    s_flash_driver = *driver;
    if (s_flash_driver.page_size == 0) {
        s_flash_driver.page_size = LE_NVRAM_PAGE_SIZE;
    }
    if (s_flash_driver.max_erase_cycles == 0) {
        s_flash_driver.max_erase_cycles = LE_NVRAM_MAX_ERASE_CYCLES;
    }
    s_nvram_mode = NVRAM_MODE_FLASH;
    memset(s_nvram_cache, 0, sizeof(s_nvram_cache));

    if (!wl_mount_scan()) {
        return false;
    }

    s_nvram_initialized = true;
    return true;
}

bool le_nvram_read(uint32_t offset, void* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) {
        return false;
    }

    /* 1. If read is within the active retentive cache, serve instantly from RAM */
    if (offset + len <= LE_NVRAM_CACHE_SIZE) {
        memcpy(buf, &s_nvram_cache[offset], len);
        return true;
    }

    /* 2. Streaming / large recording area: Stream directly through HAL (QSPI Flash) */
    if (g_le_hal && g_le_hal->nvram_read) {
        return g_le_hal->nvram_read(offset, (uint8_t*)buf, len);
    }

    return false;
}

bool le_nvram_write(uint32_t offset, const void* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) {
        return false;
    }

    /* Update RAM cache image if within retentive cache bounds */
    if (offset < LE_NVRAM_CACHE_SIZE) {
        size_t copy_len = len;
        if (offset + copy_len > LE_NVRAM_CACHE_SIZE) {
            copy_len = LE_NVRAM_CACHE_SIZE - offset;
        }
        memcpy(&s_nvram_cache[offset], buf, copy_len);
    }

    if (s_nvram_mode == NVRAM_MODE_DIRECT) {
        /* NVRAM_MODE_DIRECT: Write through HAL callback */
        if (g_le_hal && g_le_hal->nvram_write &&
            (void*)g_le_hal->nvram_write != (void*)le_nvram_write) {
            return g_le_hal->nvram_write(offset, (const uint8_t*)buf, len);
        }
        return true;
    }

    /* NVRAM_MODE_FLASH: Wear-leveled append-only log record */
    uint32_t record_total = (uint32_t)sizeof(le_wl_record_header_t) + (uint32_t)len;

    /* Check if active sector has room for record; if not, rollover and compact */
    if (s_active_sector_write_pos + record_total > s_flash_driver.sector_size) {
        if (!wl_rollover_and_compact()) {
            return false;
        }
    }

    le_wl_record_header_t rhdr;
    rhdr.marker = LE_WL_RECORD_MARKER;
    rhdr.len = (uint16_t)len;
    rhdr.nv_offset = offset;
    rhdr.crc32 = le_crc32((const uint8_t*)buf, len);

    uint32_t phys = wl_sector_phys_addr(s_active_sector_idx);

    if (!s_flash_driver.program(phys + s_active_sector_write_pos, (const uint8_t*)&rhdr, sizeof(rhdr))) {
        return false;
    }
    s_active_sector_write_pos += sizeof(rhdr);

    if (!s_flash_driver.program(phys + s_active_sector_write_pos, (const uint8_t*)buf, len)) {
        return false;
    }
    s_active_sector_write_pos += len;

    return true;
}

bool le_nvram_flush(void)
{
    if (s_nvram_mode == NVRAM_MODE_FLASH) {
        return wl_rollover_and_compact();
    }
    return true;
}

bool le_nvram_format(void)
{
    if (s_nvram_mode == NVRAM_MODE_FLASH) {
        if (!s_flash_driver.erase_sector) {
            return false;
        }
        /* Physically erase all allocated wear-leveling flash sectors */
        for (uint32_t i = 0; i < s_flash_driver.sector_count; i++) {
            uint32_t phys = wl_sector_phys_addr(i);
            if (!s_flash_driver.erase_sector(i, phys)) {
                return false;
            }
        }
        /* Format sector 0 as active with initial sequence */
        s_active_sector_idx = 0;
        s_active_sector_seq = 1;
        s_active_sector_erase_count = 1;
        if (!wl_format_sector(0, 1, 1)) {
            return false;
        }
        s_active_sector_write_pos = sizeof(le_wl_sector_header_t);
        memset(s_nvram_cache, 0, sizeof(s_nvram_cache));

        /* Populate and write fresh manifest */
        le_nvram_manifest_t m;
        nvram_populate_default_manifest(&m);
        s_manifest = m;
        s_manifest_valid = true;
        return le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m));
    } else {
        /* Direct NVRAM: Erase full partition */
        memset(s_nvram_cache, 0, sizeof(s_nvram_cache));
        if (g_le_hal && g_le_hal->nvram_erase_sector) {
            if (!g_le_hal->nvram_erase_sector(0, 0)) {
                return false;
            }
        } else if (g_le_hal && g_le_hal->nvram_write &&
                   (void*)g_le_hal->nvram_write != (void*)le_nvram_write) {
            uint8_t zero_block[128];
            memset(zero_block, 0, sizeof(zero_block));
            for (uint32_t off = 0; off < LE_NVRAM_TOTAL_BYTES; off += sizeof(zero_block)) {
                size_t n = (LE_NVRAM_TOTAL_BYTES - off < sizeof(zero_block)) ? (LE_NVRAM_TOTAL_BYTES - off) : sizeof(zero_block);
                if (!g_le_hal->nvram_write(off, zero_block, n)) {
                    return false;
                }
            }
        }
        /* Re-write fresh manifest */
        le_nvram_manifest_t m;
        nvram_populate_default_manifest(&m);
        s_manifest = m;
        s_manifest_valid = true;
        return le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m));
    }
}

bool le_nvram_erase(uint32_t offset, size_t len)
{
    if (offset + len > LE_NVRAM_TOTAL_BYTES) {
        return false;
    }
    if (len == 0) {
        return true;
    }

    if (offset == 0 && len >= LE_NVRAM_TOTAL_BYTES) {
        return le_nvram_format();
    }

    if (s_nvram_mode == NVRAM_MODE_DIRECT && g_le_hal && g_le_hal->nvram_erase_sector) {
        if (offset < LE_NVRAM_CACHE_SIZE) {
            size_t clen = (offset + len > LE_NVRAM_CACHE_SIZE) ? (LE_NVRAM_CACHE_SIZE - offset) : len;
            memset(&s_nvram_cache[offset], 0, clen);
        }
        return g_le_hal->nvram_erase_sector(0, offset);
    }

    /* Zero the logical range */
    uint8_t zeros[128];
    memset(zeros, 0, sizeof(zeros));
    uint32_t remaining = (uint32_t)len;
    uint32_t cur_offset = offset;

    while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(zeros)) ? (uint32_t)sizeof(zeros) : remaining;
        if (!le_nvram_write(cur_offset, zeros, chunk)) {
            return false;
        }
        cur_offset += chunk;
        remaining -= chunk;
    }

    return true;
}

/* ========================================================================== */
/* Retentive Registers Serialization                                         */
/* ========================================================================== */

static size_t reg_element_size(uint16_t addr)
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

bool le_nvram_save_regs_slot(uint8_t slot, const le_nvram_reg_cfg_t* cfg, const le_process_image_t* img)
{
    if (slot >= LE_MAX_CONFIG_SLOTS || !cfg || !img || cfg->range_count == 0 || cfg->range_count > LE_NVRAM_MAX_RANGES) {
        return false;
    }

    uint32_t slot_offset = le_nvram_get_regs_offset(slot);
    uint32_t max_regs_bytes = le_nvram_get_regs_bytes(slot);
    if (slot_offset == 0 || max_regs_bytes == 0) {
        return false;
    }

    uint8_t payload[2048];
    size_t max_payload = (max_regs_bytes > sizeof(le_nvram_regs_header_t)) ?
                         (max_regs_bytes - sizeof(le_nvram_regs_header_t)) : 0;
    if (max_payload > sizeof(payload)) max_payload = sizeof(payload);

    size_t payload_pos = 0;

    for (uint16_t r = 0; r < cfg->range_count; r++) {
        uint16_t base = cfg->ranges[r].start_addr;
        uint16_t count = cfg->ranges[r].count;
        uint16_t region = base & LE_ADDR_REGION_MASK;

        for (uint16_t i = 0; i < count; i++) {
            uint16_t addr = base + i;
            size_t sz = reg_element_size(addr);

            if (payload_pos + sz > max_payload) {
                return false; /* Exceeds allocated retentive partition limit */
            }

            if (region == LE_REGION_BOOL_REG || region == LE_REGION_BOOL_REG_EXT ||
                region == LE_REGION_DIN || region == LE_REGION_DOUT) {
                payload[payload_pos] = le_process_image_get_bool(img, addr) ? 1 : 0;
            } else if (region == LE_REGION_INT_REG || region == LE_REGION_INT_REG_EXT) {
                int32_t val = le_process_image_get_int(img, addr);
                memcpy(&payload[payload_pos], &val, 4);
            }
#if LE_ENABLE_COMPLEX
            else if (region == LE_REGION_CMPLX) {
                le_complex_t c = le_process_image_get_complex(img, addr);
                memcpy(&payload[payload_pos], &c, 8);
            }
#endif
            else {
                float f = le_process_image_get_float(img, addr);
                memcpy(&payload[payload_pos], &f, 4);
            }

            payload_pos += sz;
        }
    }

    le_nvram_regs_header_t hdr;
    hdr.magic = LE_NVRAM_REGS_MAGIC;
    hdr.range_count = cfg->range_count;
    hdr.total_payload_bytes = (uint16_t)payload_pos;
    hdr.crc32 = le_crc32(payload, payload_pos);

    if (!le_nvram_write(slot_offset, &hdr, sizeof(hdr))) {
        return false;
    }

    if (payload_pos > 0) {
        if (!le_nvram_write(slot_offset + sizeof(hdr), payload, payload_pos)) {
            return false;
        }
    }

    return true;
}

bool le_nvram_restore_regs_slot(uint8_t slot, const le_nvram_reg_cfg_t* cfg, le_process_image_t* img)
{
    if (slot >= LE_MAX_CONFIG_SLOTS || !cfg || !img || cfg->range_count == 0 || cfg->range_count > LE_NVRAM_MAX_RANGES) {
        return false;
    }

    uint32_t slot_offset = le_nvram_get_regs_offset(slot);
    uint32_t max_regs_bytes = le_nvram_get_regs_bytes(slot);
    if (slot_offset == 0 || max_regs_bytes == 0) {
        return false;
    }

    le_nvram_regs_header_t hdr;
    if (!le_nvram_read(slot_offset, &hdr, sizeof(hdr))) {
        return false;
    }

    if (hdr.magic != LE_NVRAM_REGS_MAGIC || hdr.range_count != cfg->range_count) {
        return false;
    }

    if (hdr.total_payload_bytes > (max_regs_bytes - sizeof(hdr))) {
        return false;
    }

    uint8_t payload[2048];
    if (hdr.total_payload_bytes > sizeof(payload)) {
        return false;
    }

    if (hdr.total_payload_bytes > 0) {
        if (!le_nvram_read(slot_offset + sizeof(hdr), payload, hdr.total_payload_bytes)) {
            return false;
        }

        uint32_t calc_crc = le_crc32(payload, hdr.total_payload_bytes);
        if (calc_crc != hdr.crc32) {
            return false;
        }
    }

    size_t payload_pos = 0;
    for (uint16_t r = 0; r < cfg->range_count; r++) {
        uint16_t base = cfg->ranges[r].start_addr;
        uint16_t count = cfg->ranges[r].count;
        uint16_t region = base & LE_ADDR_REGION_MASK;

        for (uint16_t i = 0; i < count; i++) {
            uint16_t addr = base + i;
            size_t sz = reg_element_size(addr);

            if (payload_pos + sz > hdr.total_payload_bytes) {
                return false;
            }

            if (region == LE_REGION_BOOL_REG || region == LE_REGION_BOOL_REG_EXT ||
                region == LE_REGION_DIN || region == LE_REGION_DOUT) {
                le_process_image_set_bool(img, addr, payload[payload_pos] != 0);
            } else if (region == LE_REGION_INT_REG || region == LE_REGION_INT_REG_EXT) {
                int32_t val;
                memcpy(&val, &payload[payload_pos], 4);
                le_process_image_set_int(img, addr, val);
            }
#if LE_ENABLE_COMPLEX
            else if (region == LE_REGION_CMPLX) {
                le_complex_t c;
                memcpy(&c, &payload[payload_pos], 8);
                le_process_image_set_complex(img, addr, c);
            }
#endif
            else {
                float f;
                memcpy(&f, &payload[payload_pos], 4);
                le_process_image_set_float(img, addr, f);
            }

            payload_pos += sz;
        }
    }

    return true;
}

bool le_nvram_save_regs(const le_nvram_reg_cfg_t* cfg, const le_process_image_t* img)
{
    return le_nvram_save_regs_slot(0, cfg, img);
}

bool le_nvram_restore_regs(const le_nvram_reg_cfg_t* cfg, le_process_image_t* img)
{
    return le_nvram_restore_regs_slot(0, cfg, img);
}

bool le_nvram_get_stats(le_nvram_stats_t* stats)
{
    if (!stats) return false;
    memset(stats, 0, sizeof(*stats));

    if (!s_nvram_initialized) {
        return false;
    }

    if (s_nvram_mode == NVRAM_MODE_DIRECT) {
        stats->is_flash = false;
        stats->health_pct = 100.0f;
        stats->wear_pct = 0.0f;
        stats->sector_size = LE_NVRAM_TOTAL_BYTES;
        stats->sector_bytes_used = LE_NVRAM_TOTAL_BYTES;
        stats->sector_bytes_free = 0;
        stats->sector_count = 1;
        return true;
    }

    /* Flash wear-leveling mode: collect metrics across all physical sectors */
    stats->is_flash = true;
    stats->active_sector_idx = s_active_sector_idx;
    stats->active_seq = s_active_sector_seq;
    stats->active_erase_count = s_active_sector_erase_count;
    stats->sector_count = s_flash_driver.sector_count;
    stats->sector_size = s_flash_driver.sector_size;
    stats->rated_erase_cycles = (s_flash_driver.max_erase_cycles > 0) ? s_flash_driver.max_erase_cycles : LE_NVRAM_MAX_ERASE_CYCLES;

    stats->sector_bytes_used = s_active_sector_write_pos;
    stats->sector_bytes_free = (s_flash_driver.sector_size > s_active_sector_write_pos) ?
                               (s_flash_driver.sector_size - s_active_sector_write_pos) : 0;

    uint32_t min_erase = 0xFFFFFFFFUL;
    uint32_t max_erase = 0;
    uint64_t sum_erase = 0;
    uint32_t valid_sectors = 0;

    for (uint32_t i = 0; i < s_flash_driver.sector_count; i++) {
        uint32_t erase_cnt = 0;
        if (i == s_active_sector_idx) {
            erase_cnt = s_active_sector_erase_count;
            valid_sectors++;
        } else {
            uint32_t phys = wl_sector_phys_addr(i);
            le_wl_sector_header_t shdr;
            if (s_flash_driver.read(phys, (uint8_t*)&shdr, sizeof(shdr))) {
                if (shdr.magic == LE_WL_SECTOR_MAGIC) {
                    uint32_t expected_crc = le_crc32((const uint8_t*)&shdr, offsetof(le_wl_sector_header_t, crc32));
                    if (expected_crc == shdr.crc32) {
                        erase_cnt = shdr.erase_count;
                        valid_sectors++;
                    }
                }
            }
        }

        if (erase_cnt < min_erase) min_erase = erase_cnt;
        if (erase_cnt > max_erase) max_erase = erase_cnt;
        sum_erase += erase_cnt;
    }

    if (valid_sectors == 0 || min_erase == 0xFFFFFFFFUL) {
        min_erase = s_active_sector_erase_count;
        max_erase = s_active_sector_erase_count;
        sum_erase = s_active_sector_erase_count;
        valid_sectors = 1;
    }

    stats->min_erase_count = min_erase;
    stats->max_erase_count = max_erase;
    stats->avg_erase_count = (uint32_t)(sum_erase / (valid_sectors ? valid_sectors : 1));

    /* Calculate wear percentage against rated endurance */
    if (stats->rated_erase_cycles > 0) {
        float wear = ((float)stats->max_erase_count / (float)stats->rated_erase_cycles) * 100.0f;
        if (wear > 100.0f) wear = 100.0f;
        stats->wear_pct = wear;
        stats->health_pct = 100.0f - wear;
    } else {
        stats->wear_pct = 0.0f;
        stats->health_pct = 100.0f;
    }

    return true;
}

/* ========================================================================== */
/* Dynamic Circuit-Driven Partitioning Functions                              */
/* ========================================================================== */

le_status_t le_nvram_configure_layout(uint32_t regs_per_slot, uint32_t ser_records, uint32_t event_bytes)
{
    uint32_t manifest_size = ((sizeof(le_nvram_manifest_t) + 63) / 64) * 64;
    uint32_t total_capacity = LE_NVRAM_TOTAL_BYTES;

    uint32_t config_slots = (LE_MAX_CONFIG_SLOTS > 0) ? LE_MAX_CONFIG_SLOTS : 1;
    uint32_t regs_bytes = config_slots * regs_per_slot;

    uint32_t ser_bytes = 0;
    if (ser_records > 0) {
        ser_bytes = (uint32_t)sizeof(le_ser_nv_header_t) + ser_records * (uint32_t)sizeof(le_ser_event_t);
    }

    uint32_t total_needed = manifest_size + regs_bytes + ser_bytes + event_bytes;
    if (total_needed > total_capacity) {
        return LE_ERR_CAPACITY;
    }

    memset(&s_manifest, 0, sizeof(s_manifest));
    s_manifest.magic = LE_NVRAM_MAGIC;
    s_manifest.version = LE_NVRAM_VERSION;
    s_manifest.flags = 0;
    s_manifest.total_bytes = total_capacity;
    s_manifest.config_slots = config_slots;
    s_manifest.regs_per_slot = regs_per_slot;
    s_manifest.regs_offset = manifest_size;
    s_manifest.regs_bytes = regs_bytes;

    s_manifest.ser_offset = (ser_bytes > 0) ? (s_manifest.regs_offset + s_manifest.regs_bytes) : 0;
    s_manifest.ser_bytes = ser_bytes;

    s_manifest.event_rec_offset = (event_bytes > 0) ?
        ((s_manifest.ser_offset > 0) ? (s_manifest.ser_offset + s_manifest.ser_bytes)
                                     : (s_manifest.regs_offset + s_manifest.regs_bytes)) : 0;
    s_manifest.event_rec_bytes = event_bytes;
    s_manifest.crc32 = le_crc32((const uint8_t*)&s_manifest, offsetof(le_nvram_manifest_t, crc32));

    s_manifest_valid = true;

    /* Write updated manifest to offset 0 */
    (void)le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &s_manifest, sizeof(s_manifest));
    return LE_OK;
}

uint32_t le_nvram_get_total_bytes(void)
{
    return LE_NVRAM_TOTAL_BYTES;
}

uint32_t le_nvram_get_regs_offset(uint8_t slot)
{
    nvram_ensure_manifest();
    if (slot >= s_manifest.config_slots || s_manifest.regs_per_slot == 0) {
        return 0;
    }
    return s_manifest.regs_offset + ((uint32_t)slot * s_manifest.regs_per_slot);
}

uint32_t le_nvram_get_regs_bytes(uint8_t slot)
{
    nvram_ensure_manifest();
    if (slot >= s_manifest.config_slots) {
        return 0;
    }
    return s_manifest.regs_per_slot;
}

uint32_t le_nvram_get_ser_offset(void)
{
    nvram_ensure_manifest();
    return s_manifest.ser_offset;
}

uint32_t le_nvram_get_ser_bytes(void)
{
    nvram_ensure_manifest();
    return s_manifest.ser_bytes;
}

uint32_t le_nvram_get_event_rec_offset(void)
{
    nvram_ensure_manifest();
    return s_manifest.event_rec_offset;
}

uint32_t le_nvram_get_event_rec_bytes(void)
{
    nvram_ensure_manifest();
    return s_manifest.event_rec_bytes;
}

const le_nvram_manifest_t* le_nvram_get_manifest(void)
{
    nvram_ensure_manifest();
    return &s_manifest;
}

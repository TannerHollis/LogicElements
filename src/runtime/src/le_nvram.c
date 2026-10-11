/**
 * @file le_nvram.c
 * @brief Implementation of Non-Volatile RAM (NVRAM) and wear-leveling management.
 */

#include "le_nvram.h"
#include "le_loader.h"
#include "le_hal.h"
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

/* Logical NVRAM working image cache */
static uint8_t              s_nvram_cache[LE_NVRAM_TOTAL_BYTES] = {0};

/* Flash journal active tracking */
static uint32_t             s_active_sector_idx = 0;
static uint32_t             s_active_sector_seq = 0;
static uint32_t             s_active_sector_erase_count = 0;
static uint32_t             s_active_sector_write_pos = 0; /**< Byte offset within active sector */

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
    m->config_slots = LE_MAX_CONFIG_SLOTS;
    m->regs_per_slot = LE_NVRAM_REGS_BYTES;
    m->regs_offset = LE_NVRAM_OFFSET_REGS;
    m->regs_bytes = (uint32_t)LE_MAX_CONFIG_SLOTS * LE_NVRAM_REGS_BYTES;
    m->ser_offset = LE_NVRAM_OFFSET_SER;
    m->ser_bytes = LE_NVRAM_SER_BYTES;
    m->event_rec_offset = LE_NVRAM_OFFSET_EVENT_REC;
    m->event_rec_bytes = LE_NVRAM_EVENT_REC_BYTES;
    m->crc32 = le_crc32((const uint8_t*)m, offsetof(le_nvram_manifest_t, crc32));
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
    for (uint32_t off = 0; off < LE_NVRAM_TOTAL_BYTES; off += chunk_size) {
        uint32_t cur = (LE_NVRAM_TOTAL_BYTES - off < chunk_size) ? (LE_NVRAM_TOTAL_BYTES - off) : chunk_size;

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

        /* Apply valid record to logical cache */
        memcpy(&s_nvram_cache[rhdr.nv_offset], temp_buf, rhdr.len);
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

    /* Pre-load cache from physical NVRAM if HAL read callback is provided */
    if (g_le_hal && g_le_hal->nvram_read &&
        (void*)g_le_hal->nvram_read != (void*)le_nvram_read) {
        g_le_hal->nvram_read(0, s_nvram_cache, sizeof(s_nvram_cache));
    }

    /* Populate default manifest if currently uninitialized */
    le_nvram_manifest_t m;
    if (le_nvram_read(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m))) {
        if (m.magic != LE_NVRAM_MAGIC) {
            nvram_populate_default_manifest(&m);
            le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m));
        }
    } else {
        nvram_populate_default_manifest(&m);
        le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m));
    }

    return true;
}

bool le_nvram_init_flash(const le_flash_wl_driver_t* driver)
{
    if (!driver || driver->sector_size == 0 || driver->sector_count < 2 ||
        !driver->erase_sector || !driver->program || !driver->read) {
        return false;
    }

    s_flash_driver = *driver;
    if (s_flash_driver.page_size == 0) {
        s_flash_driver.page_size = LE_NVRAM_PAGE_SIZE;
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

    if (s_nvram_mode == NVRAM_MODE_FLASH) {
        /* Instant zero-jitter read from cache image */
        memcpy(buf, &s_nvram_cache[offset], len);
        return true;
    }

    /* NVRAM_MODE_DIRECT: Try HAL callback if present */
    if (g_le_hal && g_le_hal->nvram_read) {
        return g_le_hal->nvram_read(offset, (uint8_t*)buf, len);
    }

    /* Direct mode fallback mirror */
    memcpy(buf, &s_nvram_cache[offset], len);
    return true;
}

bool le_nvram_write(uint32_t offset, const void* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) {
        return false;
    }

    /* Update RAM cache image */
    memcpy(&s_nvram_cache[offset], buf, len);

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
        return le_nvram_write(LE_NVRAM_OFFSET_MANIFEST, &m, sizeof(m));
    } else {
        /* Direct NVRAM: Erase full partition */
        memset(s_nvram_cache, 0, sizeof(s_nvram_cache));
        if (g_le_hal && g_le_hal->nvram_erase &&
            (void*)g_le_hal->nvram_erase != (void*)le_nvram_erase) {
            if (!g_le_hal->nvram_erase(0, LE_NVRAM_TOTAL_BYTES)) {
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

    if (s_nvram_mode == NVRAM_MODE_DIRECT && g_le_hal && g_le_hal->nvram_erase &&
        (void*)g_le_hal->nvram_erase != (void*)le_nvram_erase) {
        memset(&s_nvram_cache[offset], 0, len);
        return g_le_hal->nvram_erase(offset, len);
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

    uint8_t payload[LE_NVRAM_REGS_BYTES - sizeof(le_nvram_regs_header_t)];
    size_t payload_pos = 0;

    for (uint16_t r = 0; r < cfg->range_count; r++) {
        uint16_t base = cfg->ranges[r].start_addr;
        uint16_t count = cfg->ranges[r].count;
        uint16_t region = base & LE_ADDR_REGION_MASK;

        for (uint16_t i = 0; i < count; i++) {
            uint16_t addr = base + i;
            size_t sz = reg_element_size(addr);

            if (payload_pos + sz > sizeof(payload)) {
                return false; /* Exceeds retentive partition limit */
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

    uint32_t slot_offset = LE_NVRAM_SLOT_REGS_OFFSET(slot);

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

    uint32_t slot_offset = LE_NVRAM_SLOT_REGS_OFFSET(slot);

    le_nvram_regs_header_t hdr;
    if (!le_nvram_read(slot_offset, &hdr, sizeof(hdr))) {
        return false;
    }

    if (hdr.magic != LE_NVRAM_REGS_MAGIC || hdr.range_count != cfg->range_count) {
        return false;
    }

    if (hdr.total_payload_bytes > (sizeof(s_nvram_cache) - sizeof(hdr))) {
        return false;
    }

    uint8_t payload[LE_NVRAM_REGS_BYTES - sizeof(le_nvram_regs_header_t)];
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

/**
 * @file le_loader.c
 * @brief Implementation of binary loader and CRC32 verifier.
 */

#include "le_loader.h"
#include "le_rt.h"
#include <string.h>

/* Standard CRC32 IEEE 802.3 polynomial: 0xEDB88320 (reversed) */
uint32_t le_crc32(const uint8_t* data, size_t length)
{
    if (!data || length == 0) return 0;

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

le_status_t le_loader_validate(const uint8_t* buffer, size_t size, le_header_t* out_header)
{
    if (!buffer || size < sizeof(le_header_t)) {
        return LE_ERR_NULL_PTR;
    }

    const le_header_t* header = (const le_header_t*)buffer;

    if (header->magic != LE_BIN_MAGIC) {
        return LE_ERR_INVALID_MAGIC;
    }

    if (header->version != LE_BIN_VERSION) {
        return LE_ERR_INVALID_VERSION;
    }

    /* Check expected payload size */
    size_t expected_payload_size = (size_t)header->instruction_count * sizeof(le_instruction_t);

    /* Walk the block table (appended after instructions) to compute its size. */
    size_t block_table_size = 0;
    {
        size_t off = sizeof(le_header_t) + expected_payload_size;
        for (uint16_t b = 0; b < header->block_count; b++) {
            if (off + (size_t)LE_BLOCK_DESC_HEADER_BYTES > size) {
                return LE_ERR_OUT_OF_BOUNDS;
            }
            const le_block_desc_t* d = (const le_block_desc_t*)(buffer + off);
            size_t dsize = (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                           ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
            if (off + dsize > size) {
                return LE_ERR_OUT_OF_BOUNDS;
            }
            off += dsize;
            block_table_size += dsize;
        }
    }
    size_t state_table_off = sizeof(le_header_t) + expected_payload_size + block_table_size;
    size_t state_table_size = (size_t)header->state_desc_count * LE_STATE_DESC_BYTES;
    size_t config_table_size = (size_t)header->config_count * LE_CONFIG_BYTES;
    if (state_table_off + state_table_size + config_table_size > size) {
        return LE_ERR_OUT_OF_BOUNDS;
    }
    size_t total_payload = expected_payload_size + block_table_size + state_table_size + config_table_size;

    if (size < sizeof(le_header_t) + total_payload) {
        return LE_ERR_OUT_OF_BOUNDS;
    }

    /* Check MCU capacity limits */
    if (header->digital_in_count > LE_MAX_DIGITAL_IN ||
        header->digital_out_count > LE_MAX_DIGITAL_OUT ||
        header->bool_reg_count > LE_MAX_BOOL_REGS ||
        header->float_reg_count > LE_MAX_FLOATS ||
        header->timer_count > LE_MAX_TIMERS ||
        header->counter_count > LE_MAX_COUNTERS)
    {
        return LE_ERR_CAPACITY;
    }

    /* Verify CRC32 over the whole payload (instructions + block table + state table) */
    const uint8_t* payload = buffer + sizeof(le_header_t);
    uint32_t computed_crc = le_crc32(payload, total_payload);
    if (computed_crc != header->crc32) {
        return LE_ERR_CRC_MISMATCH;
    }

    if (out_header) {
        memcpy(out_header, header, sizeof(le_header_t));
    }

    return LE_OK;
}

le_status_t le_loader_load(le_vm_t* vm, const uint8_t* buffer, size_t size)
{
    if (!vm || !buffer) return LE_ERR_NULL_PTR;

    le_header_t header;
    le_status_t status = le_loader_validate(buffer, size, &header);
    if (status != LE_OK) {
        return status;
    }

    /* Zero-copy reference to instructions */
    const le_instruction_t* instructions = (const le_instruction_t*)(buffer + sizeof(le_header_t));
    status = le_vm_load_program(vm, instructions, header.instruction_count);
    if (status != LE_OK) {
        return status;
    }

    /* Zero-copy reference to the block table (immediately after instructions). */
    if (header.block_count > 0) {
        const uint8_t* bt_start = buffer + sizeof(le_header_t) +
                                  (size_t)header.instruction_count * sizeof(le_instruction_t);
        status = le_vm_load_blocks(vm, (const le_block_desc_t*)bt_start, header.block_count);
        if (status != LE_OK) {
            return status;
        }
    } else {
        status = le_vm_load_blocks(vm, NULL, 0);
        if (status != LE_OK) {
            return status;
        }
    }

    /* Bind stateful blocks: walk the state-directive table (after the block
     * table) and reserve heap slices + record per-kind base rows. */
    if (header.state_desc_count > 0) {
        size_t bt_size = 0;
        {
            size_t off = sizeof(le_header_t) + (size_t)header.instruction_count * sizeof(le_instruction_t);
            for (uint16_t b = 0; b < header.block_count; b++) {
                const le_block_desc_t* d = (const le_block_desc_t*)(buffer + off);
                off += (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                       ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
                bt_size = off - (sizeof(le_header_t) + (size_t)header.instruction_count * sizeof(le_instruction_t));
            }
        }
        const uint8_t* st = buffer + sizeof(le_header_t) +
                            (size_t)header.instruction_count * sizeof(le_instruction_t) + bt_size;
        for (uint16_t s = 0; s < header.state_desc_count; s++) {
            const le_state_desc_t* sd = (const le_state_desc_t*)(st + (size_t)s * LE_STATE_DESC_BYTES);
            if (sd->kind == LE_BLK_NONE || sd->count == 0) continue;
            int base = -1;
            for (uint16_t i = 0; i < sd->count; i++) {
                int row = le_rt_reserve(sd->kind, sd->size, 4);
                if (row < 0) return LE_ERR_CAPACITY;
                le_rt_block_t* rr = le_rt_get(row);
                if (rr) le_rt_apply_defaults(sd->kind, rr->state);
                if (base < 0) base = row;
            }
            le_rt_set_group_base(sd->kind, base);
        }
    }

    /* Apply per-element config directives to the bound heap blocks. */
    if (header.config_count > 0) {
        size_t cfg_off = sizeof(le_header_t) + (size_t)header.instruction_count * sizeof(le_instruction_t);
        for (uint16_t b = 0; b < header.block_count; b++) {
            const le_block_desc_t* d = (const le_block_desc_t*)(buffer + cfg_off);
            cfg_off += (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                       ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
        }
        cfg_off += (size_t)header.state_desc_count * LE_STATE_DESC_BYTES;
        for (uint16_t k = 0; k < header.config_count; k++) {
            const le_config_t* cfg = (const le_config_t*)(buffer + cfg_off + (size_t)k * LE_CONFIG_BYTES);
            le_rt_configure(&vm->image, cfg->kind, cfg->idx, cfg->slot, cfg->value);
        }
    }

    if (header.flags & LE_FLAG_AUTOSTART) {
        le_vm_start(vm);
    }

    return LE_OK;
}

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
    size_t alias_bytes = (size_t)header->alias_count * LE_ALIAS_BYTES;
    size_t timing_bytes = ((size_t)header->timing_count != 0) ? (size_t)LE_TIMING_DESC_BYTES : 0;
    if (state_table_off + state_table_size + (size_t)header->state_img_len + alias_bytes + timing_bytes > size) {
        return LE_ERR_OUT_OF_BOUNDS;
    }
    size_t total_payload = expected_payload_size + block_table_size + state_table_size +
                           (size_t)header->state_img_len + alias_bytes + timing_bytes;

    if (size < sizeof(le_header_t) + total_payload) {
        return LE_ERR_OUT_OF_BOUNDS;
    }

    /* Check MCU capacity limits: per-region maxima (board budgets) plus the
     * unified RAM pool — the packed register arena (sized from the header's
     * actual register counts) and the preconfigured state image must BOTH fit
     * inside LE_RAM_WORKSPACE_BYTES, so RAM tracks what the program declares. */
    if (header->digital_in_count > LE_MAX_DIGITAL_IN ||
        header->digital_out_count > LE_MAX_DIGITAL_OUT ||
        header->bool_reg_count > LE_MAX_BOOL_REGS ||
        header->float_reg_count > LE_MAX_FLOATS ||
        header->int_reg_count > LE_MAX_INT_REGS ||
#if LE_ENABLE_ANALOG
        header->analog_in_count > LE_MAX_ANALOG_IN ||
#endif
#if LE_ENABLE_COMPLEX
        header->complex_reg_count > LE_MAX_COMPLEX ||
#endif
        (uint32_t)le_process_image_regs_len(header) +
            (uint32_t)header->state_img_len > (uint32_t)LE_RAM_WORKSPACE_BYTES)
    {
        return LE_ERR_CAPACITY;
    }

    /* Verify CRC32 over the whole payload (instructions + block table + state table + state image) */
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

    /* Compute offsets of the state-desc table and the state image. */
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
    const uint8_t* img = st + (size_t)header.state_desc_count * LE_STATE_DESC_BYTES;

    /* Carve the unified RAM workspace: the packed register arena lives at the
     * FRONT of the pool (sized from the header's actual register counts via
     * le_process_image_bind) and the preconfigured state image is copied
     * verbatim into the remaining slice. No allocation or per-field config:
     * the compiler baked defaults + all element properties into the image. */
    uint32_t regs_len = 0;
    status = le_process_image_bind(&vm->image, (uint8_t*)vm->ram_workspace,
                                   LE_RAM_WORKSPACE_BYTES, &header, &regs_len);
    if (status != LE_OK) {
        return status;
    }
    le_rt_bind((uint8_t*)vm->ram_workspace + regs_len, LE_RAM_WORKSPACE_BYTES - regs_len);
    le_rt_reset();
    if (header.state_img_len > 0) {
        if ((uint32_t)header.state_img_len > le_rt_workspace_bytes()) {
            return LE_ERR_CAPACITY;
        }
        memcpy(le_rt_workspace(), img, header.state_img_len);
    }
    if (header.state_desc_count > 0) {
        uint32_t base = 0;
        for (uint16_t s = 0; s < header.state_desc_count; s++) {
            const le_state_desc_t* sd = (const le_state_desc_t*)(st + (size_t)s * LE_STATE_DESC_BYTES);
            if (sd->kind == LE_BLK_NONE || sd->count == 0) continue;
            le_rt_set_kind_base(sd->kind, (int32_t)base);
            le_rt_set_kind_count(sd->kind, sd->count);
            base += (uint32_t)sd->size * (uint32_t)sd->count;
        }
    }

    /* Zero-copy reference to the program alias table (appended after the state image). */
    le_vm_load_aliases(vm, NULL, 0);
    if (header.alias_count > 0) {
        const le_alias_t* al = (const le_alias_t*)(img + header.state_img_len);
        le_vm_load_aliases(vm, al, header.alias_count);
    }

    /* Timing descriptor (appended after the alias table): compiler cost budget
     * and the circuit-declared fixed scan rate. The DESIGNER owns the rate: the
     * loader applies it to the VM clock (1e6/rate us period) then verifies the
     * estimated worst-case scan fits it, so an unachievable program is rejected
     * at LOAD (and the designer simply lowers the declared rate). */
    vm->timing_abstract_cycles = 0;
    vm->timing_margin_pct = 100;
    vm->timed_worst_us = 0;
    vm->timing_feasible = 0;
    if (header.timing_count != 0) {
        const uint8_t* td = img + header.state_img_len +
                            (size_t)header.alias_count * LE_ALIAS_BYTES;
        const le_timing_desc_t* t = (const le_timing_desc_t*)td;
        vm->timing_abstract_cycles = t->abstract_cycles;
        vm->timing_margin_pct = t->safety_margin_pct;

        /* Designer-declared scan rate -> VM period (unless a host previously
         * applied an override with le_vm_set_scan_period_us). */
        if (t->design_scan_rate_hz > 0 && vm->scan_period_us == 0) {
            uint32_t rate = (uint32_t)t->design_scan_rate_hz;
            uint32_t period_us = (1000000u + rate / 2u) / rate; /* rounded */
            le_vm_set_scan_period_us(vm, period_us);
        }

        /* Achievability gate: scale the compiler cost by the board's calibrated
         * nanoseconds-per-cycle and compare against the (now applied) period.
         * LE_NS_PER_ABSTRACT_CYCLE == 0 (or no period) disables the gate. */
        if (vm->scan_period_us > 0 && LE_NS_PER_ABSTRACT_CYCLE > 0) {
            uint64_t worst_ns = (uint64_t)t->abstract_cycles * (uint64_t)LE_NS_PER_ABSTRACT_CYCLE;
            uint32_t worst_us = (uint32_t)((worst_ns + 999u) / 1000u);
            vm->timed_worst_us = worst_us;
            if (worst_us > vm->scan_period_us) {
                return LE_ERR_TIMING_BUDGET;
            }
            vm->timing_feasible = 1;
        }
    }

    if (header.flags & LE_FLAG_AUTOSTART) {
        le_vm_start(vm);
    }

    return LE_OK;
}

le_status_t le_loader_timing(const le_vm_t* vm, le_timing_t* out)
{
    if (!vm || !out) return LE_ERR_NULL_PTR;

    out->scan_period_us = vm->scan_period_us;
    out->abstract_cycles = vm->timing_abstract_cycles;
    out->safety_margin_pct = vm->timing_margin_pct;
    out->worst_case_us = vm->timed_worst_us;
    out->margin_us = 0;
    out->margin_pct = 0;
    out->feasible = vm->timing_feasible;

    if (vm->scan_period_us > 0 && vm->timing_feasible && vm->timed_worst_us <= vm->scan_period_us) {
        out->margin_us = vm->scan_period_us - vm->timed_worst_us;
        out->margin_pct = (uint16_t)(((uint64_t)out->margin_us * 100u) / vm->scan_period_us);
    }
    return LE_OK;
}

/**
 * @file le_loader.c
 * @brief Implementation of binary loader and CRC32 verifier.
 */

#include "le_loader.h"
#include "le_nvram.h"
#include "le_rt.h"
#include "le_crypto.h"
#include <string.h>

static uint8_t s_trusted_public_key[32];
static bool    s_trusted_key_set = false;

void le_loader_set_trusted_key(const uint8_t pk[32])
{
    if (pk) {
        memcpy(s_trusted_public_key, pk, 32);
        s_trusted_key_set = true;
    } else {
        s_trusted_key_set = false;
    }
}

const uint8_t* le_loader_get_trusted_key(void)
{
    return s_trusted_key_set ? s_trusted_public_key : NULL;
}

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

le_status_t le_loader_parse_chunks(const uint8_t* buffer, size_t size, le_chunked_view_t* out_view)
{
    if (!buffer || !out_view) return LE_ERR_NULL_PTR;
    if (size < sizeof(le_container_header_t)) return LE_ERR_OUT_OF_BOUNDS;

    memset(out_view, 0, sizeof(*out_view));
    const le_container_header_t* cont = (const le_container_header_t*)buffer;
    if (cont->magic != LE_BIN_MAGIC_V2) return LE_ERR_INVALID_MAGIC;
    if (cont->version != LE_CONTAINER_VERSION) return LE_ERR_INVALID_VERSION;
    if ((size_t)cont->total_size > size || cont->total_size < sizeof(le_container_header_t)) {
        return LE_ERR_OUT_OF_BOUNDS;
    }

    /* Verify container header CRC with header_crc32 zeroed */
    le_container_header_t temp_hdr = *cont;
    temp_hdr.header_crc32 = 0;
    uint32_t calc_hdr_crc = le_crc32((const uint8_t*)&temp_hdr, sizeof(temp_hdr));
    if (calc_hdr_crc != cont->header_crc32) {
        return LE_ERR_CRC_MISMATCH;
    }

    out_view->container = cont;

    size_t off = sizeof(le_container_header_t);
    for (uint16_t i = 0; i < cont->chunk_count; i++) {
        if (off + sizeof(le_chunk_header_t) > (size_t)cont->total_size) {
            return LE_ERR_OUT_OF_BOUNDS;
        }
        const le_chunk_header_t* chk = (const le_chunk_header_t*)(buffer + off);
        off += sizeof(le_chunk_header_t);
        if (off + (size_t)chk->length > (size_t)cont->total_size) {
            return LE_ERR_OUT_OF_BOUNDS;
        }
        const uint8_t* payload = buffer + off;

        /* Verify chunk payload CRC32 */
        uint32_t calc_crc = le_crc32(payload, chk->length);
        if (calc_crc != chk->crc32) {
            return LE_ERR_CRC_MISMATCH;
        }

        switch (chk->tag) {
            case LE_CHUNK_TAG_HEAD:
                if (chk->length < sizeof(le_header_t)) return LE_ERR_INVALID_HEADER;
                out_view->head = (const le_header_t*)payload;
                break;
            case LE_CHUNK_TAG_INST:
                out_view->inst = (const le_instruction_t*)payload;
                out_view->inst_count = (uint32_t)(chk->length / sizeof(le_instruction_t));
                break;
            case LE_CHUNK_TAG_BLCK:
                out_view->blck = (const le_block_desc_t*)payload;
                out_view->blck_bytes = chk->length;
                {
                    uint32_t bcnt = 0;
                    size_t boff = 0;
                    while (boff + LE_BLOCK_DESC_HEADER_BYTES <= chk->length) {
                        const le_block_desc_t* bd = (const le_block_desc_t*)(payload + boff);
                        size_t bsz = (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                                     ((size_t)bd->in_count + (size_t)bd->out_count) * sizeof(uint16_t);
                        if (boff + bsz > chk->length) return LE_ERR_OUT_OF_BOUNDS;
                        boff += bsz;
                        bcnt++;
                    }
                    out_view->blck_count = bcnt;
                }
                break;
            case LE_CHUNK_TAG_STAT:
                out_view->stat = (const le_state_desc_t*)payload;
                out_view->stat_count = (uint32_t)(chk->length / LE_STATE_DESC_BYTES);
                break;
            case LE_CHUNK_TAG_SIMG:
                out_view->simg = payload;
                out_view->simg_len = chk->length;
                break;
#if LE_ENABLE_ALIASES
            case LE_CHUNK_TAG_ALIS:
                out_view->alis = (const le_alias_t*)payload;
                out_view->alis_count = (uint32_t)(chk->length / LE_ALIAS_BYTES);
                break;
#endif
#if LE_ENABLE_TIMING_MODEL
            case LE_CHUNK_TAG_TIME:
                if (chk->length >= LE_TIMING_DESC_BYTES) {
                    out_view->time = (const le_timing_desc_t*)payload;
                }
                break;
#endif
#if LE_ENABLE_MULTI_RATE
            case LE_CHUNK_TAG_RATE:
                if (chk->length >= sizeof(le_rate_desc_t)) {
                    out_view->rate = (const le_rate_desc_t*)payload;
                }
                break;
#endif
            case LE_CHUNK_TAG_NVMD:
                if (chk->length >= sizeof(le_nvram_desc_t)) {
                    out_view->nvmd = (const le_nvram_desc_t*)payload;
                }
                break;
#if LE_ENABLE_CRYPTO
            case LE_CHUNK_TAG_SIGN:
                out_view->sign = payload;
                out_view->sign_len = chk->length;
                break;
#endif
            default:
                break;
        }
        off += chk->length;
    }

    if (!out_view->head || !out_view->inst) {
        return LE_ERR_INVALID_HEADER;
    }
    return LE_OK;
}

le_status_t le_loader_validate_signed(const uint8_t* buffer, size_t size,
                                      const uint8_t public_key[32],
                                      le_header_t* out_header)
{
    if (!buffer || size < sizeof(le_container_header_t)) {
        return LE_ERR_NULL_PTR;
    }

    const uint8_t* effective_pk = public_key ? public_key : (s_trusted_key_set ? s_trusted_public_key : NULL);

    uint32_t magic = *(const uint32_t*)buffer;

    if (magic == LE_BIN_MAGIC_V2) {
        le_chunked_view_t view;
        le_status_t status = le_loader_parse_chunks(buffer, size, &view);
        if (status != LE_OK) return status;

        /* If signature authentication is required, verify the SIGN chunk */
        if (effective_pk) {
            if (!view.sign || view.sign_len < LE_CRYPTO_SIGNATURE_BYTES) {
                return LE_ERR_SIGNATURE_MISSING;
            }
            const uint8_t* sign_chk_ptr = (const uint8_t*)view.sign - LE_CHUNK_HEADER_BYTES;
            if (sign_chk_ptr < buffer || sign_chk_ptr > buffer + size) {
                return LE_ERR_OUT_OF_BOUNDS;
            }
            size_t signed_msg_len = (size_t)(sign_chk_ptr - buffer);
            le_status_t sig_status = le_crypto_ed25519_verify(view.sign, buffer, signed_msg_len, effective_pk);
            if (sig_status != LE_OK) {
                return LE_ERR_SIGNATURE_INVALID;
            }
        }

        const le_header_t* header = view.head;

        if (header->digital_in_count > LE_MAX_DIGITAL_IN ||
            header->digital_out_count > LE_MAX_DIGITAL_OUT ||
#if LE_ENABLE_ANALOG
            header->analog_in_count > LE_MAX_ANALOG_IN ||
            header->analog_out_count > LE_MAX_ANALOG_OUT ||
#endif
            (uint32_t)le_process_image_regs_len(header) +
                (uint32_t)header->state_img_len > (uint32_t)LE_RAM_WORKSPACE_BYTES)
        {
            return LE_ERR_CAPACITY;
        }

        if (out_header) {
            memcpy(out_header, header, sizeof(le_header_t));
        }
        return LE_OK;
    }

    return LE_ERR_INVALID_MAGIC;
}

le_status_t le_loader_validate(const uint8_t* buffer, size_t size, le_header_t* out_header)
{
    return le_loader_validate_signed(buffer, size, NULL, out_header);
}

le_status_t le_loader_load(le_vm_t* vm, const uint8_t* buffer, size_t size)
{
    return le_loader_load_signed(vm, buffer, size, NULL);
}

le_status_t le_loader_load_signed(le_vm_t* vm, const uint8_t* buffer, size_t size,
                                  const uint8_t public_key[32])
{
    if (!vm || !buffer) return LE_ERR_NULL_PTR;

    uint32_t magic = (size >= 4) ? *(const uint32_t*)buffer : 0;
    if (magic != LE_BIN_MAGIC) {
        return LE_ERR_INVALID_MAGIC;
    }

    le_header_t validated_hdr;
    le_status_t status = le_loader_validate_signed(buffer, size, public_key, &validated_hdr);
    if (status != LE_OK) return status;

    le_chunked_view_t view;
    status = le_loader_parse_chunks(buffer, size, &view);
    if (status != LE_OK) return status;

    const le_header_t* header = view.head;

    status = le_vm_load_program(vm, view.inst, header->instruction_count);
    if (status != LE_OK) return status;

    if (header->block_count > 0 && view.blck) {
        status = le_vm_load_blocks(vm, view.blck, header->block_count);
        if (status != LE_OK) return status;
    } else {
        status = le_vm_load_blocks(vm, NULL, 0);
        if (status != LE_OK) return status;
    }

    uint32_t regs_len = 0;
    status = le_process_image_bind(&vm->image, (uint8_t*)vm->ram_workspace,
                                   LE_RAM_WORKSPACE_BYTES, header, &regs_len);
    if (status != LE_OK) return status;

    le_rt_bind((uint8_t*)vm->ram_workspace + regs_len, LE_RAM_WORKSPACE_BYTES - regs_len);
    le_rt_reset();
    if (header->state_img_len > 0 && view.simg) {
        if ((uint32_t)header->state_img_len > le_rt_workspace_bytes()) {
            return LE_ERR_CAPACITY;
        }
        memcpy(le_rt_workspace(), view.simg, header->state_img_len);
    }
    if (header->state_desc_count > 0 && view.stat) {
        uint32_t base = 0;
        for (uint16_t s = 0; s < header->state_desc_count; s++) {
            const le_state_desc_t* sd = &view.stat[s];
            if (sd->kind == LE_BLK_NONE || sd->count == 0) continue;
            le_rt_set_kind_base(sd->kind, (int32_t)base);
            le_rt_set_kind_count(sd->kind, sd->count);
            base += (uint32_t)sd->size * (uint32_t)sd->count;
        }
    }

#if LE_ENABLE_ALIASES
    le_vm_load_aliases(vm, NULL, 0);
    if (header->alias_count > 0 && view.alis) {
        le_vm_load_aliases(vm, view.alis, header->alias_count);
    }
#endif

#if LE_ENABLE_TIMING_MODEL
    vm->timing_abstract_cycles = 0;
    vm->timing_margin_pct = 100;
    vm->timed_worst_us = 0;
    vm->timing_feasible = 0;
    if (view.time) {
        const le_timing_desc_t* t = view.time;
        vm->timing_abstract_cycles = t->abstract_cycles;
        vm->timing_margin_pct = t->safety_margin_pct;
        if (t->design_scan_rate_hz > 0) {
            le_vm_set_scan_rate_hz(vm, (uint32_t)t->design_scan_rate_hz);
        }
        if (vm->scan_period_us > 0 && LE_NS_PER_ABSTRACT_CYCLE > 0) {
#if defined(__AVR__)
            uint32_t worst_us = (uint32_t)(((uint32_t)t->abstract_cycles * (uint32_t)LE_NS_PER_ABSTRACT_CYCLE + 999u) / 1000u);
#else
            uint64_t worst_ns = (uint64_t)t->abstract_cycles * (uint64_t)LE_NS_PER_ABSTRACT_CYCLE;
            uint32_t worst_us = (uint32_t)((worst_ns + 999u) / 1000u);
#endif
            vm->timed_worst_us = worst_us;
            if (worst_us > vm->scan_period_us) {
                return LE_ERR_TIMING_BUDGET;
            }
            vm->timing_feasible = 1;
        }
    }
#endif

#if LE_ENABLE_MULTI_RATE
    if (view.rate) {
        le_vm_set_rate_prescalers(vm, view.rate->rate_fast_count,
                                  view.rate->rate_med_count,
                                  view.rate->rate_med_div,
                                  view.rate->rate_slow_div);
    } else {
        le_vm_set_rate_prescalers(vm, 0, 0, 1, 1);
    }
#endif

    if (view.nvmd) {
        le_status_t nvs = le_nvram_configure_layout(view.nvmd->retentive_bytes,
                                                   view.nvmd->ser_records,
                                                   view.nvmd->event_bytes);
        if (nvs != LE_OK) {
            return nvs;
        }
        if (view.nvmd->retentive_range_count > 0) {
            le_nvram_reg_cfg_t rcfg;
            rcfg.range_count = view.nvmd->retentive_range_count;
            if (rcfg.range_count > LE_NVRAM_MAX_RANGES) rcfg.range_count = LE_NVRAM_MAX_RANGES;
            for (uint16_t ri = 0; ri < rcfg.range_count; ri++) {
                rcfg.ranges[ri] = view.nvmd->ranges[ri];
            }
            (void)le_nvram_restore_regs_slot(0, &rcfg, &vm->image);
        }
    }

    if (view.container->flags & LE_FLAG_AUTOSTART) {
        le_vm_start(vm);
    }
    return LE_OK;
}

le_status_t le_loader_load_micro(le_vm_t* vm, const uint8_t* buffer, size_t size)
{
    if (!vm || !buffer || size < sizeof(le_container_header_t)) {
        return LE_ERR_NULL_PTR;
    }
    const le_container_header_t* cont = (const le_container_header_t*)buffer;
    if (cont->magic != LE_BIN_MAGIC) {
        return LE_ERR_INVALID_MAGIC;
    }
    const le_header_t* head = NULL;
    const le_instruction_t* inst = NULL;

    size_t off = sizeof(le_container_header_t);
    while (off + sizeof(le_chunk_header_t) <= size && off + sizeof(le_chunk_header_t) <= (size_t)cont->total_size) {
        const le_chunk_header_t* chk = (const le_chunk_header_t*)(buffer + off);
        off += sizeof(le_chunk_header_t);
        if (off + (size_t)chk->length > size) return LE_ERR_OUT_OF_BOUNDS;

        if (chk->tag == LE_CHUNK_TAG_HEAD && chk->length >= sizeof(le_header_t)) {
            head = (const le_header_t*)(buffer + off);
        } else if (chk->tag == LE_CHUNK_TAG_INST) {
            inst = (const le_instruction_t*)(buffer + off);
        }
        off += chk->length;
        if (head && inst) break;
    }

    if (!head || !inst) return LE_ERR_INVALID_HEADER;

    le_status_t status = le_vm_load_program(vm, inst, head->instruction_count);
    if (status != LE_OK) return status;

    uint32_t regs_len = 0;
    status = le_process_image_bind(&vm->image, (uint8_t*)vm->ram_workspace,
                                   LE_RAM_WORKSPACE_BYTES, head, &regs_len);
    if (status != LE_OK) return status;

    if (cont->flags & LE_FLAG_AUTOSTART) {
        le_vm_start(vm);
    }
    return LE_OK;
}

#if !defined(__AVR__)
le_status_t le_loader_convert_to_chunked_nv(const uint8_t* flat_bin, size_t flat_size,
                                            const le_rate_desc_t* rate,
                                            const le_nvram_desc_t* nvmd,
                                            uint8_t* out_buf, size_t max_out_size, size_t* out_size)
{
    if (!flat_bin || !out_buf || !out_size) return LE_ERR_NULL_PTR;

    if (flat_size < sizeof(le_header_t)) return LE_ERR_NULL_PTR;
    const le_header_t* raw_hdr = (const le_header_t*)flat_bin;
    if (raw_hdr->magic != LE_BIN_MAGIC_V1 && raw_hdr->magic != LE_BIN_MAGIC) {
        return LE_ERR_INVALID_MAGIC;
    }
    le_header_t hdr = *raw_hdr;
    hdr.magic = LE_BIN_MAGIC;
    hdr.version = LE_BIN_VERSION;

    size_t inst_size = (size_t)hdr.instruction_count * sizeof(le_instruction_t);
    const uint8_t* inst_ptr = flat_bin + sizeof(le_header_t);

    size_t bt_size = 0;
    const uint8_t* bt_ptr = inst_ptr + inst_size;
    {
        size_t off = sizeof(le_header_t) + inst_size;
        for (uint16_t b = 0; b < hdr.block_count; b++) {
            const le_block_desc_t* d = (const le_block_desc_t*)(flat_bin + off);
            size_t dsize = (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                           ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
            off += dsize;
            bt_size += dsize;
        }
    }

    const uint8_t* st_ptr = bt_ptr + bt_size;
    size_t st_size = (size_t)hdr.state_desc_count * LE_STATE_DESC_BYTES;

    const uint8_t* simg_ptr = st_ptr + st_size;
    size_t simg_size = (size_t)hdr.state_img_len;

    const uint8_t* alis_ptr = simg_ptr + simg_size;
    size_t alis_size = (size_t)hdr.alias_count * LE_ALIAS_BYTES;

    const uint8_t* time_ptr = alis_ptr + alis_size;
    size_t time_size = (hdr.timing_count != 0) ? (size_t)LE_TIMING_DESC_BYTES : 0;

    size_t rate_size = (rate != NULL) ? sizeof(le_rate_desc_t) : 0;
    size_t nvmd_size = (nvmd != NULL) ? sizeof(le_nvram_desc_t) : 0;

    uint16_t chunk_count = 2; /* HEAD + INST */
    if (hdr.block_count > 0) chunk_count++;
    if (hdr.state_desc_count > 0) chunk_count++;
    if (hdr.state_img_len > 0) chunk_count++;
    if (hdr.alias_count > 0) chunk_count++;
    if (hdr.timing_count > 0) chunk_count++;
    if (rate != NULL) chunk_count++;
    if (nvmd != NULL) chunk_count++;

    size_t needed = sizeof(le_container_header_t) + (size_t)chunk_count * sizeof(le_chunk_header_t) +
                    sizeof(le_header_t) + inst_size + bt_size + st_size + simg_size + alis_size + time_size + rate_size + nvmd_size;
    if (needed > max_out_size) return LE_ERR_OUT_OF_BOUNDS;

    size_t write_off = sizeof(le_container_header_t);

#define LE_EMIT_CHUNK(tag_val, pdata, plen) do { \
        le_chunk_header_t* chk = (le_chunk_header_t*)(out_buf + write_off); \
        chk->tag = (tag_val); \
        chk->length = (uint32_t)(plen); \
        chk->crc32 = le_crc32((const uint8_t*)(pdata), (plen)); \
        chk->flags = 0; \
        write_off += sizeof(le_chunk_header_t); \
        if ((plen) > 0) { \
            memcpy(out_buf + write_off, (pdata), (plen)); \
            write_off += (plen); \
        } \
    } while (0)

    LE_EMIT_CHUNK(LE_CHUNK_TAG_HEAD, &hdr, sizeof(le_header_t));
    LE_EMIT_CHUNK(LE_CHUNK_TAG_INST, inst_ptr, inst_size);
    if (hdr.block_count > 0) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_BLCK, bt_ptr, bt_size);
    }
    if (hdr.state_desc_count > 0) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_STAT, st_ptr, st_size);
    }
    if (hdr.state_img_len > 0) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_SIMG, simg_ptr, simg_size);
    }
    if (hdr.alias_count > 0) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_ALIS, alis_ptr, alis_size);
    }
    if (hdr.timing_count > 0) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_TIME, time_ptr, time_size);
    }
    if (rate != NULL) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_RATE, rate, sizeof(le_rate_desc_t));
    }
    if (nvmd != NULL) {
        LE_EMIT_CHUNK(LE_CHUNK_TAG_NVMD, nvmd, sizeof(le_nvram_desc_t));
    }
#undef LE_EMIT_CHUNK

    le_container_header_t* cont = (le_container_header_t*)out_buf;
    cont->magic = LE_BIN_MAGIC_V2;
    cont->version = LE_CONTAINER_VERSION;
    cont->flags = hdr.flags;
    cont->total_size = (uint32_t)write_off;
    cont->chunk_count = chunk_count;
    cont->reserved = 0;
    cont->header_crc32 = 0;
    cont->header_crc32 = le_crc32((const uint8_t*)cont, sizeof(le_container_header_t));

    *out_size = write_off;
    return LE_OK;
}

le_status_t le_loader_convert_to_chunked_ex(const uint8_t* flat_bin, size_t flat_size,
                                            const le_rate_desc_t* rate,
                                            uint8_t* out_buf, size_t max_out_size, size_t* out_size)
{
    return le_loader_convert_to_chunked_nv(flat_bin, flat_size, rate, NULL, out_buf, max_out_size, out_size);
}

le_status_t le_loader_convert_to_chunked(const uint8_t* flat_bin, size_t flat_size,
                                         uint8_t* out_buf, size_t max_out_size, size_t* out_size)
{
    return le_loader_convert_to_chunked_nv(flat_bin, flat_size, NULL, NULL, out_buf, max_out_size, out_size);
}

le_status_t le_loader_sign_container(const uint8_t* in_container, size_t in_size,
                                     const uint8_t secret_key[64],
                                     uint8_t* out_buf, size_t max_out_size, size_t* out_size)
{
    if (!in_container || !secret_key || !out_buf || !out_size) return LE_ERR_NULL_PTR;
    if (in_size < sizeof(le_container_header_t)) return LE_ERR_OUT_OF_BOUNDS;

    const le_container_header_t* in_cont = (const le_container_header_t*)in_container;
    if (in_cont->magic != LE_BIN_MAGIC_V2) return LE_ERR_INVALID_MAGIC;

    size_t sign_chunk_total = sizeof(le_chunk_header_t) + LE_CRYPTO_SIGNATURE_BYTES;
    size_t needed = in_size + sign_chunk_total;
    if (needed > max_out_size) return LE_ERR_OUT_OF_BOUNDS;

    /* Copy existing container */
    memcpy(out_buf, in_container, in_size);

    /* Update container header: chunk_count incremented, total_size updated */
    le_container_header_t* out_cont = (le_container_header_t*)out_buf;
    out_cont->chunk_count = in_cont->chunk_count + 1;
    out_cont->total_size = (uint32_t)needed;
    out_cont->header_crc32 = 0;
    out_cont->header_crc32 = le_crc32((const uint8_t*)out_cont, sizeof(le_container_header_t));

    /* Compute detached Ed25519 signature across all bytes preceding the SIGN chunk header */
    uint8_t sig[LE_CRYPTO_SIGNATURE_BYTES];
    le_status_t st = le_crypto_ed25519_sign(sig, out_buf, in_size, secret_key);
    if (st != LE_OK) return st;

    /* Append SIGN chunk */
    le_chunk_header_t* chk = (le_chunk_header_t*)(out_buf + in_size);
    chk->tag = LE_CHUNK_TAG_SIGN;
    chk->length = LE_CRYPTO_SIGNATURE_BYTES;
    chk->crc32 = le_crc32(sig, LE_CRYPTO_SIGNATURE_BYTES);
    chk->flags = 0;

    memcpy(out_buf + in_size + sizeof(le_chunk_header_t), sig, LE_CRYPTO_SIGNATURE_BYTES);

    *out_size = needed;
    return LE_OK;
}
#endif /* !__AVR__ */

#if LE_ENABLE_TIMING_MODEL
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
#endif

le_status_t le_loader_swap_online(le_vm_t* vm, const uint8_t* buffer, size_t size)
{
    if (!vm || !buffer) return LE_ERR_NULL_PTR;

    /* 1. Pre-validate the candidate program so the active VM is never disturbed on invalid image */
    le_header_t new_hdr;
    le_status_t status = le_loader_validate(buffer, size, &new_hdr);
    if (status != LE_OK) {
        return status;
    }

    /* If no program was previously loaded in the VM, perform a normal load */
    if (!vm->image.regs) {
        return le_loader_load(vm, buffer, size);
    }

    /* 2. Snapshot running state, outputs, direct registers, aliases, and state blocks */
    bool was_running = vm->running;

    /* Snapshot physical DOUT */
    uint8_t snap_dout[(LE_MAX_DIGITAL_OUT + 7) / 8];
    memset(snap_dout, 0, sizeof(snap_dout));
    uint16_t snap_dout_count = (vm->image.dout_count < new_hdr.digital_out_count) ?
                               vm->image.dout_count : new_hdr.digital_out_count;
    if (snap_dout_count > LE_MAX_DIGITAL_OUT) snap_dout_count = LE_MAX_DIGITAL_OUT;
    for (uint16_t i = 0; i < snap_dout_count; i++) {
        if (le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(i))) {
            snap_dout[i / 8] |= (uint8_t)(1U << (i % 8));
        }
    }

#if LE_ENABLE_ANALOG
    /* Snapshot physical AOUT */
    float snap_aout[LE_MAX_ANALOG_OUT];
    memset(snap_aout, 0, sizeof(snap_aout));
    uint16_t snap_aout_count = (vm->image.aout_count < new_hdr.analog_out_count) ?
                               vm->image.aout_count : new_hdr.analog_out_count;
    if (snap_aout_count > LE_MAX_ANALOG_OUT) snap_aout_count = LE_MAX_ANALOG_OUT;
    for (uint16_t i = 0; i < snap_aout_count; i++) {
        snap_aout[i] = le_pi_get_aout(&vm->image, i);
    }
#endif

    /* Snapshot direct user registers (%B, %F, %I, %C) */
#define LE_SWAP_MAX_BOOL_REGS 256
    uint8_t snap_bool_regs[(LE_SWAP_MAX_BOOL_REGS + 7) / 8];
    memset(snap_bool_regs, 0, sizeof(snap_bool_regs));
    uint16_t snap_bool_count = (vm->image.bool_count < new_hdr.bool_reg_count) ?
                               vm->image.bool_count : new_hdr.bool_reg_count;
    if (snap_bool_count > LE_SWAP_MAX_BOOL_REGS) snap_bool_count = LE_SWAP_MAX_BOOL_REGS;
    for (uint16_t i = 0; i < snap_bool_count; i++) {
        if (le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_BOOL_REG(i))) {
            snap_bool_regs[i / 8] |= (uint8_t)(1U << (i % 8));
        }
    }

#define LE_SWAP_MAX_FLOATS 128
    float snap_floats[LE_SWAP_MAX_FLOATS];
    uint16_t snap_float_count = (vm->image.float_count < new_hdr.float_reg_count) ?
                                vm->image.float_count : new_hdr.float_reg_count;
    if (snap_float_count > LE_SWAP_MAX_FLOATS) snap_float_count = LE_SWAP_MAX_FLOATS;
    for (uint16_t i = 0; i < snap_float_count; i++) {
        snap_floats[i] = le_process_image_get_float(&vm->image, LE_ADDR_MAKE_FLOAT(i));
    }

#define LE_SWAP_MAX_INTS 128
    int32_t snap_ints[LE_SWAP_MAX_INTS];
    uint16_t snap_int_count = (vm->image.int_count < new_hdr.int_reg_count) ?
                              vm->image.int_count : new_hdr.int_reg_count;
    if (snap_int_count > LE_SWAP_MAX_INTS) snap_int_count = LE_SWAP_MAX_INTS;
    for (uint16_t i = 0; i < snap_int_count; i++) {
        snap_ints[i] = le_process_image_get_int(&vm->image, LE_ADDR_MAKE_INT_REG(i));
    }

#if LE_ENABLE_COMPLEX
#define LE_SWAP_MAX_CMPLX 64
    le_complex_t snap_cmplx[LE_SWAP_MAX_CMPLX];
    uint16_t snap_cmplx_count = (vm->image.cmplx_count < new_hdr.complex_reg_count) ?
                                vm->image.cmplx_count : new_hdr.complex_reg_count;
    if (snap_cmplx_count > LE_SWAP_MAX_CMPLX) snap_cmplx_count = LE_SWAP_MAX_CMPLX;
    for (uint16_t i = 0; i < snap_cmplx_count; i++) {
        snap_cmplx[i] = le_process_image_get_complex(&vm->image, LE_ADDR_MAKE_CMPLX(i));
    }
#endif

    /* Snapshot symbolic register aliases by name and typed value */
#define LE_SWAP_MAX_ALIASES 64
    typedef struct {
        char name[LE_ALIAS_NAME_MAX + 1];
        uint8_t type; /* 0=bool, 1=float, 2=int, 3=cmplx */
        union {
            bool b;
            float f;
            int32_t i;
#if LE_ENABLE_COMPLEX
            le_complex_t c;
#endif
        } val;
    } le_alias_snap_t;

    le_alias_snap_t alias_snaps[LE_SWAP_MAX_ALIASES];
    uint16_t alias_snap_count = 0;
    if (vm->aliases && vm->alias_count > 0) {
        uint16_t lim = (vm->alias_count < LE_SWAP_MAX_ALIASES) ? vm->alias_count : LE_SWAP_MAX_ALIASES;
        for (uint16_t a = 0; a < lim; a++) {
            const le_alias_t* al = &vm->aliases[a];
            le_alias_snap_t* snap = &alias_snaps[alias_snap_count];
            memcpy(snap->name, al->name, LE_ALIAS_NAME_MAX);
            snap->name[LE_ALIAS_NAME_MAX] = '\0';
            uint16_t addr = al->addr;
            if (LE_ADDR_IS_BOOL(addr) || LE_ADDR_IS_DOUT(addr) || LE_ADDR_IS_DIN(addr)) {
                snap->type = 0;
                snap->val.b = le_process_image_get_bool(&vm->image, addr);
                alias_snap_count++;
            } else if (LE_ADDR_IS_FLOAT(addr) || LE_ADDR_IS_AOUT(addr)) {
                snap->type = 1;
                snap->val.f = le_process_image_get_float(&vm->image, addr);
                alias_snap_count++;
            } else if (LE_ADDR_IS_INT(addr) || LE_ADDR_IS_AIN(addr)) {
                snap->type = 2;
                snap->val.i = le_process_image_get_int(&vm->image, addr);
                alias_snap_count++;
#if LE_ENABLE_COMPLEX
            } else if (LE_ADDR_IS_CMPLX(addr)) {
                snap->type = 3;
                snap->val.c = le_process_image_get_complex(&vm->image, addr);
                alias_snap_count++;
#endif
            }
        }
    }

    /* Snapshot state blocks (timers, counters, filters, PID, etc.) */
#define LE_SWAP_MAX_STATE_RECORDS 32
#define LE_SWAP_STATE_BUF_SIZE 1024
    typedef struct {
        uint8_t  kind;
        uint16_t count;
        uint16_t size;
        uint16_t offset;
    } le_state_snap_hdr_t;

    uint8_t state_snap_buf[LE_SWAP_STATE_BUF_SIZE];
    le_state_snap_hdr_t state_snaps[LE_SWAP_MAX_STATE_RECORDS];
    uint16_t state_snap_count = 0;
    uint16_t state_buf_used = 0;

    for (uint8_t k = 0; k < (uint8_t)LE_BLK_LAST; k++) {
        uint16_t old_cnt = le_rt_kind_count(k);
        uint16_t sz = le_rt_kind_size(k);
        if (old_cnt > 0 && sz > 0 && state_snap_count < LE_SWAP_MAX_STATE_RECORDS) {
            uint32_t needed = (uint32_t)old_cnt * (uint32_t)sz;
            if ((uint32_t)state_buf_used + needed <= LE_SWAP_STATE_BUF_SIZE) {
                for (uint16_t idx = 0; idx < old_cnt; idx++) {
                    uint8_t* p = le_rt_state(k, idx);
                    if (p) {
                        memcpy(&state_snap_buf[state_buf_used + idx * sz], p, sz);
                    }
                }
                state_snaps[state_snap_count].kind = k;
                state_snaps[state_snap_count].count = old_cnt;
                state_snaps[state_snap_count].size = sz;
                state_snaps[state_snap_count].offset = state_buf_used;
                state_snap_count++;
                state_buf_used += (uint16_t)needed;
            }
        }
    }

    /* 3. Load the new program */
    status = le_loader_load(vm, buffer, size);
    if (status != LE_OK) {
        return status;
    }

    /* 4. Restore snapshotted outputs and registers into the new program */
    for (uint16_t i = 0; i < snap_dout_count; i++) {
        bool b = (snap_dout[i / 8] & (1U << (i % 8))) != 0;
        le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DOUT(i), b);
    }

#if LE_ENABLE_ANALOG
    for (uint16_t i = 0; i < snap_aout_count; i++) {
        le_pi_set_aout(&vm->image, i, snap_aout[i]);
    }
#endif

    for (uint16_t i = 0; i < snap_bool_count; i++) {
        bool b = (snap_bool_regs[i / 8] & (1U << (i % 8))) != 0;
        le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_BOOL_REG(i), b);
    }
    for (uint16_t i = 0; i < snap_float_count; i++) {
        le_process_image_set_float(&vm->image, LE_ADDR_MAKE_FLOAT(i), snap_floats[i]);
    }
    for (uint16_t i = 0; i < snap_int_count; i++) {
        le_process_image_set_int(&vm->image, LE_ADDR_MAKE_INT_REG(i), snap_ints[i]);
    }
#if LE_ENABLE_COMPLEX
    for (uint16_t i = 0; i < snap_cmplx_count; i++) {
        le_process_image_set_complex(&vm->image, LE_ADDR_MAKE_CMPLX(i), snap_cmplx[i]);
    }
#endif

    /* 5. Transfer matching named alias values across remapped addresses */
    for (uint16_t a = 0; a < alias_snap_count; a++) {
        const le_alias_snap_t* snap = &alias_snaps[a];
        uint16_t new_addr;
        if (le_alias_lookup(vm, snap->name, &new_addr) == LE_OK) {
            if (snap->type == 0) {
                le_process_image_set_bool(&vm->image, new_addr, snap->val.b);
            } else if (snap->type == 1) {
                le_process_image_set_float(&vm->image, new_addr, snap->val.f);
            } else if (snap->type == 2) {
                le_process_image_set_int(&vm->image, new_addr, snap->val.i);
#if LE_ENABLE_COMPLEX
            } else if (snap->type == 3) {
                le_process_image_set_complex(&vm->image, new_addr, snap->val.c);
#endif
            }
        }
    }

    /* 6. Restore matching stateful blocks (accumulated time, filter states, counters) */
    for (uint16_t s = 0; s < state_snap_count; s++) {
        uint8_t k = state_snaps[s].kind;
        uint16_t new_cnt = le_rt_kind_count(k);
        uint16_t new_sz = le_rt_kind_size(k);
        if (new_cnt > 0 && new_sz == state_snaps[s].size) {
            uint16_t restore_cnt = (state_snaps[s].count < new_cnt) ? state_snaps[s].count : new_cnt;
            for (uint16_t idx = 0; idx < restore_cnt; idx++) {
                uint8_t* new_ptr = le_rt_state(k, idx);
                if (new_ptr) {
                    const uint8_t* saved_ptr = &state_snap_buf[state_snaps[s].offset + idx * new_sz];
                    memcpy(new_ptr, saved_ptr, new_sz);
                }
            }
        }
    }

    /* 7. Preserve running status */
    if (was_running) {
        le_vm_start(vm);
    }

    return LE_OK;
}

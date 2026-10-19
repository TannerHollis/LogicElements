/**
 * @file le_storage.c
 * @brief Implementation of multi-configuration slot storage with sector descriptors.
 */

#include "le_storage.h"
#include "le_loader.h"
#include <string.h>

static bool storage_erase_flash_sector(const le_hal_t* hal, const le_storage_sector_t* sec)
{
    if (!hal || !sec) return false;
    if (sec->erase_size == 0 || sec->medium == LE_MEDIUM_RAM) return true;
    if (hal->storage_erase_sector) {
        return hal->storage_erase_sector(sec);
    }
    return true;
}

static bool storage_write_to_flash(const le_hal_t* hal, const le_storage_sector_t* sec,
                                   uint32_t offset_in_sector, const uint8_t* data, size_t len)
{
    if (!hal || !sec || !data || len == 0) return false;
    if (!hal->storage_write_page) return false;

    size_t page_sz = (sec->page_size > 0) ? sec->page_size : LE_STORAGE_PAGE_SIZE;
    size_t written = 0;
    uint8_t pad_buf[256];

    while (written < len) {
        size_t chunk = len - written;
        uint32_t page_offset = offset_in_sector + (uint32_t)written;

        if (chunk >= page_sz) {
            if (!hal->storage_write_page(sec, page_offset, data + written, page_sz)) {
                return false;
            }
            written += page_sz;
        } else {
            /* Partial page: pad trailing bytes with 0xFF */
            if (page_sz <= sizeof(pad_buf)) {
                memset(pad_buf, 0xFF, page_sz);
                memcpy(pad_buf, data + written, chunk);
                if (!hal->storage_write_page(sec, page_offset, pad_buf, page_sz)) {
                    return false;
                }
            } else {
                if (!hal->storage_write_page(sec, page_offset, data + written, chunk)) {
                    return false;
                }
            }
            written += chunk;
        }
    }
    return true;
}

void le_storage_init(le_storage_t* storage, const le_hal_t* hal)
{
    if (!storage) return;
    memset(storage, 0, sizeof(le_storage_t));
    storage->hal = hal;
    storage->active_slot = 0;

    /* If HAL storage is available, populate RAM partitions from Flash */
#if LE_PHYSICAL_SLOT_COUNT > 0
    if (hal && hal->storage_read) {
#if LE_PHYSICAL_SLOT_COUNT == 1
        const le_storage_sector_t* sec = le_storage_get_sector(0);
        hal->storage_read(sec, 0, storage->ram_partitions[0], LE_SLOT_SIZE_BYTES);
#else
        for (uint8_t s = 0; s < LE_PHYSICAL_SLOT_COUNT; s++) {
            const le_storage_sector_t* sec = le_storage_get_sector(s);
            hal->storage_read(sec, 0, storage->ram_partitions[s], LE_SLOT_SIZE_BYTES);
        }
#endif
    }
#endif
}

uint8_t le_storage_get_phantom_slot(const le_storage_t* storage)
{
    (void)storage;
    return LE_MAX_CONFIG_SLOTS;
}

uint8_t le_storage_get_slot_count(const le_storage_t* storage)
{
    (void)storage;
    return LE_MAX_CONFIG_SLOTS;
}

bool le_storage_write_chunk(le_storage_t* storage, uint8_t slot, uint32_t offset, const uint8_t* data, size_t len)
{
    /* Physical slots include the hidden phantom (index LE_MAX_CONFIG_SLOTS) so
     * uploads can stream into it; user slots are [0, LE_MAX_CONFIG_SLOTS). */
    if (!storage || !data || slot > LE_MAX_CONFIG_SLOTS) return false;
    if (offset + len > LE_SLOT_SIZE_BYTES) return false;

#if LE_PHYSICAL_SLOT_COUNT == 1
    if (slot == storage->active_slot) {
        memcpy(&storage->ram_partitions[0][offset], data, len);
    }
#elif LE_PHYSICAL_SLOT_COUNT > 1
    if (slot < LE_PHYSICAL_SLOT_COUNT) {
        memcpy(&storage->ram_partitions[slot][offset], data, len);
    }
#endif

    /* Write to non-volatile storage if HAL is available */
    if (storage->hal && storage->hal->storage_write_page) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        if (offset == 0) {
            storage_erase_flash_sector(storage->hal, sec);
        }
        return storage_write_to_flash(storage->hal, sec, offset, data, len);
    }

    return (LE_PHYSICAL_SLOT_COUNT > 0);
}

bool le_storage_read_chunk(const le_storage_t* storage, uint8_t slot, uint32_t offset, uint8_t* buffer, size_t len)
{
    if (!storage || !buffer || slot > LE_MAX_CONFIG_SLOTS) return false;
    if (offset + len > LE_SLOT_SIZE_BYTES) return false;

#if LE_PHYSICAL_SLOT_COUNT == 0
    if (storage->hal && storage->hal->storage_read) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        return storage->hal->storage_read(sec, offset, buffer, len);
    }
    return false;
#elif LE_PHYSICAL_SLOT_COUNT == 1
    if (slot == storage->active_slot) {
        memcpy(buffer, &storage->ram_partitions[0][offset], len);
        return true;
    }
    if (storage->hal && storage->hal->storage_read) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        return storage->hal->storage_read(sec, offset, buffer, len);
    }
    return false;
#else
    if (slot < LE_PHYSICAL_SLOT_COUNT) {
        memcpy(buffer, &storage->ram_partitions[slot][offset], len);
        return true;
    }
    return false;
#endif
}

le_status_t le_storage_verify_slot(const le_storage_t* storage, uint8_t slot, le_header_t* out_header)
{
    if (!storage || slot >= LE_MAX_CONFIG_SLOTS) return LE_ERR_OUT_OF_BOUNDS;

#if LE_PHYSICAL_SLOT_COUNT <= 1
#if LE_PHYSICAL_SLOT_COUNT == 1
    if (slot == storage->active_slot) {
        const uint8_t* slot_data = storage->ram_partitions[0];
        return le_loader_validate(slot_data, LE_SLOT_SIZE_BYTES, out_header);
    }
#endif
    if (storage->hal && storage->hal->storage_read) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        uint8_t buf[256];
        if (storage->hal->storage_read(sec, 0, buf, sizeof(buf))) {
            uint32_t magic = *(const uint32_t*)buf;
            if (magic == LE_BIN_MAGIC) {
                const le_container_header_t* cont = (const le_container_header_t*)buf;
                if (cont->version == LE_CONTAINER_VERSION && cont->total_size <= LE_SLOT_SIZE_BYTES) {
                    size_t off = sizeof(le_container_header_t);
                    while (off + sizeof(le_chunk_header_t) <= sizeof(buf)) {
                        const le_chunk_header_t* chk = (const le_chunk_header_t*)(buf + off);
                        if (chk->tag == LE_CHUNK_TAG_HEAD && (off + sizeof(le_chunk_header_t) + sizeof(le_header_t) <= sizeof(buf))) {
                            if (out_header) {
                                *out_header = *(const le_header_t*)(buf + off + sizeof(le_chunk_header_t));
                            }
                            return LE_OK;
                        }
                        off += sizeof(le_chunk_header_t) + chk->length;
                    }
                    if (out_header) {
                        memset(out_header, 0, sizeof(*out_header));
                        out_header->magic = magic;
                        out_header->version = LE_BIN_VERSION;
                        out_header->flags = cont->flags;
                    }
                    return LE_OK;
                }
            }
        }
    }
    return LE_ERR_INVALID_MAGIC;
#else
    const uint8_t* slot_data = storage->ram_partitions[slot];
    return le_loader_validate(slot_data, LE_SLOT_SIZE_BYTES, out_header);
#endif
}

bool le_storage_get_slot_info(const le_storage_t* storage, uint8_t slot, le_slot_info_t* info)
{
    if (!storage || !info || slot >= LE_MAX_CONFIG_SLOTS) return false;

    memset(info, 0, sizeof(le_slot_info_t));
    info->slot_id = slot;

    le_header_t header;
    le_status_t status = le_storage_verify_slot(storage, slot, &header);
    if (status == LE_OK) {
        info->valid = true;
        info->instruction_count = header.instruction_count;
        info->program_size = sizeof(le_header_t) + ((size_t)header.instruction_count * sizeof(le_instruction_t));
        info->crc32 = header.crc32;
        info->flags = header.flags;
        return true;
    }

    return false;
}

uint8_t le_storage_get_active_slot(const le_storage_t* storage)
{
    if (!storage) return 0;
    return storage->active_slot;
}

le_status_t le_storage_commit_upload(le_storage_t* storage, uint8_t target_slot, size_t expected_size)
{
    if (!storage) return LE_ERR_NULL_PTR;
    if (target_slot >= LE_MAX_CONFIG_SLOTS) return LE_ERR_OUT_OF_BOUNDS;
#if LE_MAX_CONFIG_SLOTS > 1
    if (target_slot == storage->active_slot) return LE_ERR_ACTIVE_SLOT;
#endif
    if (expected_size > LE_SLOT_SIZE_BYTES) return LE_ERR_CAPACITY;

    const uint8_t phantom = LE_MAX_CONFIG_SLOTS;

#if LE_PHYSICAL_SLOT_COUNT <= 1
    if (storage->hal && storage->hal->storage_read && storage->hal->storage_write_page) {
        const le_storage_sector_t* phantom_sec = le_storage_get_sector(phantom);
        const le_storage_sector_t* target_sec  = le_storage_get_sector(target_slot);

        uint8_t buf[256];
        if (!storage->hal->storage_read(phantom_sec, 0, buf, sizeof(buf))) {
            return LE_ERR_STORAGE;
        }
        uint32_t magic = *(const uint32_t*)buf;
        size_t program_total = expected_size;

        if (magic == LE_BIN_MAGIC) {
            const le_container_header_t* cont = (const le_container_header_t*)buf;
            if (cont->version != LE_CONTAINER_VERSION) {
                return LE_ERR_INVALID_VERSION;
            }
            if (program_total == 0) program_total = cont->total_size;
        } else {
            return LE_ERR_INVALID_MAGIC;
        }

        uint8_t copy_buf[256];
        size_t copied = 0;
        size_t to_copy = (program_total > 0 && program_total <= LE_SLOT_SIZE_BYTES) ?
                         program_total : LE_SLOT_SIZE_BYTES;

        if (!storage_erase_flash_sector(storage->hal, target_sec)) {
            return LE_ERR_STORAGE;
        }

        while (copied < to_copy) {
            size_t chunk = (to_copy - copied > sizeof(copy_buf)) ? sizeof(copy_buf) : (to_copy - copied);
            if (!storage->hal->storage_read(phantom_sec, (uint32_t)copied, copy_buf, chunk)) {
                return LE_ERR_STORAGE;
            }
            if (!storage_write_to_flash(storage->hal, target_sec, (uint32_t)copied, copy_buf, chunk)) {
                return LE_ERR_STORAGE;
            }
            copied += chunk;
        }
#if LE_PHYSICAL_SLOT_COUNT == 1
        if (target_slot == storage->active_slot && storage->hal->storage_read) {
            storage->hal->storage_read(target_sec, 0, storage->ram_partitions[0], to_copy);
        }
#endif
        return LE_OK;
    }
    return LE_ERR_STORAGE;
#else
    /* Full structural + CRC32 validation of the staged phantom image. Only a
     * fully-valid image is ever allowed to overwrite the target slot. A bad or
     * truncated transmission fails here and leaves the target untouched. */
    le_status_t status = le_loader_validate(storage->ram_partitions[phantom], LE_SLOT_SIZE_BYTES, NULL);
    if (status != LE_OK) {
        return status;
    }

    /* Commit: phantom -> target, in RAM ... */
    memcpy(storage->ram_partitions[target_slot], storage->ram_partitions[phantom], LE_SLOT_SIZE_BYTES);

    /* ... and in non-volatile storage when available. Erase target sector before writing. */
    if (storage->hal && storage->hal->storage_write_page) {
        const le_storage_sector_t* target_sec = le_storage_get_sector(target_slot);
        if (!storage_erase_flash_sector(storage->hal, target_sec)) {
            return LE_ERR_STORAGE;
        }
        if (!storage_write_to_flash(storage->hal, target_sec, 0, storage->ram_partitions[phantom], LE_SLOT_SIZE_BYTES)) {
            return LE_ERR_STORAGE;
        }
    }

    return LE_OK;
#endif
}

bool le_storage_activate_slot(le_storage_t* storage, uint8_t slot, le_vm_t* vm)
{
    if (!storage || !vm || slot >= LE_MAX_CONFIG_SLOTS) return false;

#if LE_PHYSICAL_SLOT_COUNT == 0
    if (storage->hal && storage->hal->storage_get_xip_ptr) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        const uint8_t* xip_ptr = storage->hal->storage_get_xip_ptr(sec, 0);
        if (xip_ptr) {
            le_status_t status = le_loader_load(vm, xip_ptr, LE_SLOT_SIZE_BYTES);
            if (status != LE_OK) {
                return false;
            }
            storage->active_slot = slot;
            return true;
        }
    }
    return false;
#elif LE_PHYSICAL_SLOT_COUNT == 1
    if (slot != storage->active_slot) {
        if (storage->hal && storage->hal->storage_read) {
            const le_storage_sector_t* sec = le_storage_get_sector(slot);
            if (!storage->hal->storage_read(sec, 0, storage->ram_partitions[0], LE_SLOT_SIZE_BYTES)) {
                return false;
            }
        }
    }
    le_status_t status = le_loader_load(vm, storage->ram_partitions[0], LE_SLOT_SIZE_BYTES);
    if (status != LE_OK) {
        return false;
    }
    storage->active_slot = slot;
    return true;
#else
    le_status_t status = le_loader_load(vm, storage->ram_partitions[slot], LE_SLOT_SIZE_BYTES);
    if (status != LE_OK) {
        return false;
    }

    storage->active_slot = slot;
    return true;
#endif
}

bool le_storage_swap_slot_online(le_storage_t* storage, uint8_t slot, le_vm_t* vm)
{
    if (!storage || !vm || slot >= LE_MAX_CONFIG_SLOTS) return false;

#if LE_PHYSICAL_SLOT_COUNT == 0
    if (storage->hal && storage->hal->storage_get_xip_ptr) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        const uint8_t* xip_ptr = storage->hal->storage_get_xip_ptr(sec, 0);
        if (xip_ptr) {
            le_status_t status = le_loader_swap_online(vm, xip_ptr, LE_SLOT_SIZE_BYTES);
            if (status != LE_OK) {
                return false;
            }
            storage->active_slot = slot;
            return true;
        }
    }
    return false;
#elif LE_PHYSICAL_SLOT_COUNT == 1
    if (storage->hal && storage->hal->storage_read) {
        const le_storage_sector_t* sec = le_storage_get_sector(slot);
        if (!storage->hal->storage_read(sec, 0, storage->ram_partitions[0], LE_SLOT_SIZE_BYTES)) {
            return false;
        }
    }
    le_status_t status = le_loader_swap_online(vm, storage->ram_partitions[0], LE_SLOT_SIZE_BYTES);
    if (status != LE_OK) {
        return false;
    }
    storage->active_slot = slot;
    return true;
#else
    le_status_t status = le_loader_swap_online(vm, storage->ram_partitions[slot], LE_SLOT_SIZE_BYTES);
    if (status != LE_OK) {
        return false;
    }

    storage->active_slot = slot;
    return true;
#endif
}

le_status_t le_storage_autostart_or_demo(le_vm_t* vm, le_storage_t* storage,
                                        uint8_t default_slot,
                                        const uint8_t* demo_prog, size_t demo_len,
                                        const char** out_source_desc)
{
    if (!vm) return LE_ERR_NULL_PTR;

    if (out_source_desc) *out_source_desc = "None";

    /* Attempt 1: Load from persistent storage slot */
    if (storage && le_storage_activate_slot(storage, default_slot, vm)) {
        if (out_source_desc) {
            *out_source_desc = (default_slot == 0) ? "Flash Slot 0 (XIP)" : "Persistent Storage Slot";
        }
        le_vm_start(vm);
        return LE_OK;
    }

    /* Attempt 2: Fall back to embedded demo program */
    if (demo_prog && demo_len > 0) {
        le_status_t st = le_loader_load(vm, demo_prog, demo_len);
        if (st == LE_OK) {
            if (out_source_desc) *out_source_desc = "Embedded Demo (ROM)";
            le_vm_start(vm);
            return LE_OK;
        }
        return st;
    }

    return LE_ERR_STORAGE;
}

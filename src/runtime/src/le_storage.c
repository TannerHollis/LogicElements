/**
 * @file le_storage.c
 * @brief Implementation of multi-configuration slot storage.
 */

#include "le_storage.h"
#include "le_loader.h"
#include <string.h>

void le_storage_init(le_storage_t* storage, const le_hal_t* hal)
{
    if (!storage) return;
    memset(storage, 0, sizeof(le_storage_t));
    storage->hal = hal;
    storage->active_slot = 0;

    /* If HAL storage is available, populate RAM partitions from Flash */
    if (hal && hal->storage_read) {
        for (uint8_t s = 0; s < LE_MAX_CONFIG_SLOTS; s++) {
            uint32_t flash_offset = (uint32_t)s * LE_SLOT_SIZE_BYTES;
            hal->storage_read(flash_offset, storage->ram_partitions[s], LE_SLOT_SIZE_BYTES);
        }
    }
}

uint8_t le_storage_get_slot_count(const le_storage_t* storage)
{
    (void)storage;
    return LE_MAX_CONFIG_SLOTS;
}

bool le_storage_write_chunk(le_storage_t* storage, uint8_t slot, uint32_t offset, const uint8_t* data, size_t len)
{
    if (!storage || !data || slot >= LE_MAX_CONFIG_SLOTS) return false;
    if (offset + len > LE_SLOT_SIZE_BYTES) return false;

    /* Write to RAM partition */
    memcpy(&storage->ram_partitions[slot][offset], data, len);

    /* Write to non-volatile storage if HAL is available */
    if (storage->hal && storage->hal->storage_write) {
        uint32_t flash_offset = ((uint32_t)slot * LE_SLOT_SIZE_BYTES) + offset;
        storage->hal->storage_write(flash_offset, data, len);
    }

    return true;
}

bool le_storage_read_chunk(const le_storage_t* storage, uint8_t slot, uint32_t offset, uint8_t* buffer, size_t len)
{
    if (!storage || !buffer || slot >= LE_MAX_CONFIG_SLOTS) return false;
    if (offset + len > LE_SLOT_SIZE_BYTES) return false;

    memcpy(buffer, &storage->ram_partitions[slot][offset], len);
    return true;
}

le_status_t le_storage_verify_slot(const le_storage_t* storage, uint8_t slot, le_header_t* out_header)
{
    if (!storage || slot >= LE_MAX_CONFIG_SLOTS) return LE_ERR_OUT_OF_BOUNDS;

    const uint8_t* slot_data = storage->ram_partitions[slot];
    return le_loader_validate(slot_data, LE_SLOT_SIZE_BYTES, out_header);
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
    } else {
        info->valid = false;
    }

    return true;
}

uint8_t le_storage_get_active_slot(const le_storage_t* storage)
{
    if (!storage) return 0;
    return storage->active_slot;
}

bool le_storage_activate_slot(le_storage_t* storage, uint8_t slot, le_vm_t* vm)
{
    if (!storage || !vm || slot >= LE_MAX_CONFIG_SLOTS) return false;

    le_status_t status = le_loader_load(vm, storage->ram_partitions[slot], LE_SLOT_SIZE_BYTES);
    if (status != LE_OK) {
        return false;
    }

    storage->active_slot = slot;
    return true;
}

/**
 * @file le_board_config.c
 * @brief AVR (ATmega328P / Arduino Uno) Storage and NVRAM configuration.
 */

#include "le_board_config.h"
#include "le_storage.h"

const le_storage_sector_t g_board_storage_sectors[] = {
    {
        .slot_id      = 0,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 0,
    },
    {
        .slot_id      = 1,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 4096,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 1,
    },
    {
        .slot_id      = LE_SLOT_PHANTOM,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 8192,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 2,
    }
};

const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);

const le_storage_sector_t g_board_nvram_sector = {
    .slot_id      = 0,
    .medium       = LE_MEDIUM_INTERNAL_FLASH,
    .phys_addr    = 12288,
    .size_bytes   = LE_NVRAM_TOTAL_BYTES,
    .erase_size   = 4096,
    .page_size    = 256,
    .hw_sector_id = 3,
};

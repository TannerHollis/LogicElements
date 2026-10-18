/**
 * @file le_board_config.c
 * @brief STM32 Storage and NVRAM sector configuration.
 */

#include "le_board_config.h"
#include "le_storage.h"

/* Example flash offsets for STM32 high-flash sectors */
#define LE_STM32_SLOT0_OFFSET   0x00060000UL
#define LE_STM32_SLOT1_OFFSET   0x00064000UL
#define LE_STM32_PHANTOM_OFFSET 0x00068000UL
#define LE_STM32_NVRAM_OFFSET   0x00070000UL

const le_storage_sector_t g_board_storage_sectors[] = {
    {
        .slot_id      = 0,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 5,
    },
    {
        .slot_id      = 1,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 4096,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 6,
    },
    {
        .slot_id      = LE_SLOT_PHANTOM,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 8192,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 7,
    }
};

const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);

const le_storage_sector_t g_board_nvram_sector = {
    .slot_id      = 0,
    .medium       = LE_MEDIUM_INTERNAL_FLASH,
    .phys_addr    = LE_STM32_NVRAM_OFFSET,
    .size_bytes   = LE_NVRAM_TOTAL_BYTES,
    .erase_size   = 4096,
    .page_size    = 256,
    .hw_sector_id = 8,
};

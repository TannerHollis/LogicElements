/**
 * @file le_board_config.c
 * @brief Board Port Storage and NVRAM declarative configuration.
 */

#include "le_board_config.h"
#include "le_storage.h"

/**
 * @brief Declarative storage sector layout.
 * Define physical flash sectors for Slot 0..N and the Phantom slot.
 */
const le_storage_sector_t g_board_storage_sectors[] = {
    {
        .slot_id      = 0,
        .medium       = (LE_PORT_STORAGE_BACKEND == 0) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = (LE_PORT_STORAGE_BACKEND == 0) ? 0 : LE_STORAGE_SECTOR_SIZE,
        .page_size    = (LE_PORT_STORAGE_BACKEND == 0) ? 1 : LE_STORAGE_PAGE_SIZE,
        .hw_sector_id = 0,
    },
    {
        .slot_id      = 1,
        .medium       = (LE_PORT_STORAGE_BACKEND == 0) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = (uint32_t)1 * LE_SLOT_SIZE_BYTES,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = (LE_PORT_STORAGE_BACKEND == 0) ? 0 : LE_STORAGE_SECTOR_SIZE,
        .page_size    = (LE_PORT_STORAGE_BACKEND == 0) ? 1 : LE_STORAGE_PAGE_SIZE,
        .hw_sector_id = 0,
    },
    {
        .slot_id      = LE_SLOT_PHANTOM,
        .medium       = (LE_PORT_STORAGE_BACKEND == 0) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = (uint32_t)LE_MAX_CONFIG_SLOTS * LE_SLOT_SIZE_BYTES,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = (LE_PORT_STORAGE_BACKEND == 0) ? 0 : LE_STORAGE_SECTOR_SIZE,
        .page_size    = (LE_PORT_STORAGE_BACKEND == 0) ? 1 : LE_STORAGE_PAGE_SIZE,
        .hw_sector_id = 0,
    }
};

const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);

const le_storage_sector_t g_board_nvram_sector = {
    .slot_id      = 0,
    .medium       = (LE_PORT_NVRAM_BACKEND == 0) ? LE_MEDIUM_RAM :
                    (LE_PORT_NVRAM_BACKEND == 1) ? LE_MEDIUM_DIRECT_NVRAM : LE_MEDIUM_INTERNAL_FLASH,
    .phys_addr    = (uint32_t)(LE_MAX_CONFIG_SLOTS + 1) * LE_SLOT_SIZE_BYTES,
    .size_bytes   = LE_NVRAM_TOTAL_BYTES,
    .erase_size   = (LE_PORT_NVRAM_BACKEND == 0 || LE_PORT_NVRAM_BACKEND == 1) ? 0 : LE_NVRAM_BLOCK_SIZE,
    .page_size    = (LE_PORT_NVRAM_BACKEND == 0 || LE_PORT_NVRAM_BACKEND == 1) ? 1 : LE_NVRAM_PAGE_SIZE,
    .hw_sector_id = 0,
};

#include "le_custom_node.h"

/**
 * @brief Declarative custom nodes table.
 * Declare board peripherals or custom C algorithms here.
 */
const le_custom_node_t g_board_custom_nodes[] = {
    /* Example:
    {
        .func_id      = 1,
        .type_id      = "MY_BOARD_PWM",
        .display_name = "Board PWM",
        .category     = "Peripherals",
        .description  = "Hardware PWM duty cycle generator",
        .inputs       = (const le_custom_port_t[]){ { "Duty", LE_PORT_TYPE_FLOAT, 0.0f }, { "Enable", LE_PORT_TYPE_BOOL, 0.0f } },
        .in_count     = 2,
        .outputs      = (const le_custom_port_t[]){ { "Active", LE_PORT_TYPE_BOOL, 0.0f } },
        .out_count    = 1,
        .handler      = my_pwm_handler
    }
    */
    { 0 }
};

const size_t g_board_custom_node_count = 0;


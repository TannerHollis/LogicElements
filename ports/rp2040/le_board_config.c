/**
 * @file le_board_config.c
 * @brief RP2040 / Raspberry Pi Pico Storage and NVRAM sector configuration.
 */

#include "le_board_config.h"
#include "le_storage.h"

#define LE_RP2040_FLASH_TARGET_OFFSET   (1024 * 1024)   /* 1MB offset for slot storage */
#define LE_RP2040_NVRAM_TARGET_OFFSET   (1536 * 1024)   /* 1.5MB offset for NVRAM wear pool */

const le_storage_sector_t g_board_storage_sectors[] = {
    {
        .slot_id      = 0,
        .medium       = LE_MEDIUM_EXTERNAL_FLASH,
        .phys_addr    = 0,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 0,
    },
    {
        .slot_id      = 1,
        .medium       = LE_MEDIUM_EXTERNAL_FLASH,
        .phys_addr    = 4096,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 0,
    },
    {
        .slot_id      = LE_SLOT_PHANTOM,
        .medium       = LE_MEDIUM_EXTERNAL_FLASH,
        .phys_addr    = 8192,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 0,
    }
};

const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);

const le_storage_sector_t g_board_nvram_sector = {
    .slot_id      = 0,
    .medium       = LE_MEDIUM_EXTERNAL_FLASH,
    .phys_addr    = LE_RP2040_NVRAM_TARGET_OFFSET,
    .size_bytes   = LE_NVRAM_TOTAL_BYTES,
    .erase_size   = 4096,
    .page_size    = 256,
    .hw_sector_id = 0,
};

#include "le_custom_node.h"

static le_status_t rp2040_stepper_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_count; (void)out_count;
    bool enable = le_custom_get_bool(img, args, 0);
    bool dir    = le_custom_get_bool(img, args, 1);
    (void)dir;
    le_custom_set_bool(img, args, in_count, 0, enable);
    return LE_OK;
}

static const le_custom_port_t s_stepper_inputs[] = {
    { "StepEnable", LE_PORT_TYPE_BOOL, 0.0f },
    { "Direction",  LE_PORT_TYPE_BOOL, 1.0f }
};
static const le_custom_port_t s_stepper_outputs[] = {
    { "StepActive", LE_PORT_TYPE_BOOL, 0.0f }
};

const le_custom_node_t g_board_custom_nodes[] = {
    {
        .func_id      = 1,
        .type_id      = "RP2040_PIO_Stepper",
        .display_name = "PIO Stepper Driver",
        .category     = "RP2040 Peripherals",
        .description  = "Deterministic PIO state machine step/dir generator",
        .inputs       = s_stepper_inputs,
        .in_count     = 2,
        .outputs      = s_stepper_outputs,
        .out_count    = 1,
        .handler      = rp2040_stepper_handler
    }
};

const size_t g_board_custom_node_count = sizeof(g_board_custom_nodes) / sizeof(g_board_custom_nodes[0]);


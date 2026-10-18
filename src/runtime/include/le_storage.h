/**
 * @file le_storage.h
 * @brief Multi-configuration slot storage manager for LogicElements.
 */

#ifndef LE_STORAGE_H
#define LE_STORAGE_H

#include "le_types.h"
#include "le_vm.h"
#include "le_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slot capacities and partitioning strategies are configured in le_config.h */

#ifndef LE_SLOT_PHANTOM
#define LE_SLOT_PHANTOM                 0xFFU
#endif

/**
 * @brief Physical backing medium for a storage or NVRAM partition.
 */
typedef enum {
    LE_MEDIUM_RAM = 0,             /**< In-memory SRAM (zero erase, byte-writable) */
    LE_MEDIUM_INTERNAL_FLASH,      /**< On-chip MCU NOR Flash (sector erase + word/page program) */
    LE_MEDIUM_EXTERNAL_FLASH,      /**< External SPI/QSPI/OSPI NOR Flash (sector erase + page program) */
    LE_MEDIUM_DIRECT_NVRAM,        /**< External byte-addressable NVRAM (FRAM, MRAM, battery SRAM) */
} le_media_type_t;

/**
 * @brief Declarative descriptor for a single storage slot or NVRAM partition.
 */
typedef struct le_storage_sector_s {
    uint8_t         slot_id;       /**< 0, 1, ... or LE_SLOT_PHANTOM (0xFF) */
    le_media_type_t medium;        /**< Physical medium type */
    uint32_t        phys_addr;     /**< Physical base address or memory-mapped base */
    uint32_t        size_bytes;    /**< Total slot capacity in bytes */
    uint32_t        erase_size;    /**< Sector/block erase size (e.g. 4096 or 131072; 0 for RAM/FRAM) */
    uint16_t        page_size;     /**< Flash page programming size (e.g. 256 for SPI, 32 for STM32, 1 for RAM) */
    uint16_t        hw_sector_id;  /**< Hardware sector/bank identifier (e.g. FLASH_SECTOR_6 on STM32) */
} le_storage_sector_t;

/**
 * @brief Board-provided storage sector descriptors defined in le_config.c.
 */
extern const le_storage_sector_t g_board_storage_sectors[];
extern const size_t g_board_storage_sector_count;
extern const le_storage_sector_t g_board_nvram_sector;

/**
 * @brief Finds the storage sector descriptor for a given slot index.
 *
 * @param slot Slot index (0..LE_MAX_CONFIG_SLOTS - 1, or LE_SLOT_PHANTOM / LE_MAX_CONFIG_SLOTS).
 * @return Pointer to matching le_storage_sector_t, or a default synthesized descriptor if not found.
 */
const le_storage_sector_t* le_storage_get_sector(uint8_t slot);

/**
 * @brief Metadata descriptor for an individual configuration slot.
 */
typedef struct {
    bool     valid;             /**< Indicates whether the slot contains a verified .lebin program. */
    uint8_t  slot_id;           /**< Zero-based slot index. */
    uint16_t instruction_count; /**< Number of virtual machine instructions in the program. */
    uint32_t program_size;      /**< Total size of the program binary in bytes. */
    uint32_t crc32;             /**< Computed CRC-32 checksum of the program payload. */
    uint16_t flags;             /**< Program execution flags (@ref LE_FLAG_AUTOSTART). */
} le_slot_info_t;

/**
 * @brief Multi-configuration storage manager instance.
 */
typedef struct {
    const le_hal_t* hal;                                                    /**< Pointer to hardware callbacks (or NULL for RAM-only). */
    uint8_t         active_slot;                                            /**< Index of the currently loaded and active slot. */
#if LE_PHYSICAL_SLOT_COUNT > 0
    uint8_t         ram_partitions[LE_PHYSICAL_SLOT_COUNT][LE_SLOT_SIZE_BYTES]; /**< In-memory storage partitions (last index = hidden phantom scratch). */
#endif
} le_storage_t;

/**
 * @brief Initializes the multi-configuration storage manager.
 *
 * @param storage Pointer to the storage instance to initialize.
 * @param hal Pointer to the hardware abstraction layer (or `NULL` for RAM-only mode).
 */
void le_storage_init(le_storage_t* storage, const le_hal_t* hal);

/**
 * @brief Returns the total number of configuration slots available.
 *
 * @param storage Pointer to the storage instance.
 * @return Number of allocated configuration slots.
 */
uint8_t le_storage_get_slot_count(const le_storage_t* storage);

/**
 * @brief Retrieves metadata and validation status for a configuration slot.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based slot index (`0` to `LE_MAX_CONFIG_SLOTS - 1`).
 * @param info Pointer to the destination structure to receive slot metadata.
 * @return Returns `true` if the slot exists and info was populated; otherwise, `false`.
 */
bool le_storage_get_slot_info(const le_storage_t* storage, uint8_t slot, le_slot_info_t* info);

/**
 * @brief Writes a chunk of binary data to a specific configuration slot.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based target slot index.
 * @param offset Byte offset within the slot partition.
 * @param data Pointer to the source data buffer.
 * @param len Number of bytes to write.
 * @return Returns `true` if the write succeeded; otherwise, `false`.
 */
bool le_storage_write_chunk(le_storage_t* storage, uint8_t slot, uint32_t offset, const uint8_t* data, size_t len);

/**
 * @brief Reads data from a configuration slot partition.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based target slot index.
 * @param offset Byte offset within the slot partition.
 * @param buffer Pointer to the destination buffer.
 * @param len Number of bytes to read.
 * @return Returns `true` if the read succeeded; otherwise, `false`.
 */
bool le_storage_read_chunk(const le_storage_t* storage, uint8_t slot, uint32_t offset, uint8_t* buffer, size_t len);

/**
 * @brief Verifies the integrity and header of a stored `.lebin` binary.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based target slot index.
 * @param out_header Optional pointer to receive the validated program header (or `NULL`).
 * @return Returns `LE_OK` if valid; otherwise, a descriptive @ref le_status_t error code.
 */
le_status_t le_storage_verify_slot(const le_storage_t* storage, uint8_t slot, le_header_t* out_header);

/**
 * @brief Returns the index of the currently active configuration slot.
 *
 * @param storage Pointer to the storage instance.
 * @return Zero-based index of the active configuration slot.
 */
uint8_t le_storage_get_active_slot(const le_storage_t* storage);

/**
 * @brief Activates a configuration slot and loads its program into the virtual machine.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based target slot index to activate.
 * @param vm Pointer to the virtual machine instance to load.
 * @return Returns `true` if the slot was validated and loaded; otherwise, `false`.
 */
bool le_storage_activate_slot(le_storage_t* storage, uint8_t slot, le_vm_t* vm);

/**
 * @brief Performs a bump-free online swap to a configuration slot, preserving live state.
 *
 * Validates the target slot, captures live outputs (%DOUT, %AOUT), common direct registers,
 * re-mapped symbolic alias values, and live internal state blocks (timers, counters, filters, PID),
 * loads the program from @p slot, and restores live state into the running virtual machine.
 *
 * @param storage Pointer to the storage instance.
 * @param slot Zero-based target slot index to activate.
 * @param vm Pointer to the virtual machine instance to swap into.
 * @return Returns `true` if the swap succeeded; otherwise, `false`.
 */
bool le_storage_swap_slot_online(le_storage_t* storage, uint8_t slot, le_vm_t* vm);

/**
 * @brief Returns the index of the hidden phantom (scratch) slot.
 *
 * The phantom slot is always `LE_MAX_CONFIG_SLOTS`. It is used internally to
 * stage uploads so a bad transmission can never overwrite a live config. It is
 * never reported through @ref le_storage_get_slot_count and is not addressable
 * as a user slot.
 *
 * @param storage Pointer to the storage instance (may be NULL).
 * @return The phantom slot index (`LE_MAX_CONFIG_SLOTS`).
 */
uint8_t le_storage_get_phantom_slot(const le_storage_t* storage);

/**
 * @brief Atomically commits a fully-validated phantom upload to a user slot.
 *
 * Verifies the phantom (staged) partition with the full @ref le_loader_validate
 * (magic, version, bounds, capacity, and complete CRC32) and only then copies it
 * over the target slot (RAM + non-volatile storage). A target equal to the
 * active slot, or outside `[0, LE_MAX_CONFIG_SLOTS)`, is rejected so the running
 * config and slot table are never clobbered by a bad transmission.
 *
 * @param storage Pointer to the storage instance.
 * @param target_slot User-visible destination slot (`0` to `LE_MAX_CONFIG_SLOTS - 1`).
 * @param expected_size Declared upload size in bytes (must equal what was staged).
 * @return `LE_OK` on commit, or `LE_ERR_NULL_PTR`, `LE_ERR_OUT_OF_BOUNDS`,
 *         `LE_ERR_ACTIVE_SLOT`, `LE_ERR_CAPACITY`, `LE_ERR_STORAGE`, or the
 *         @ref le_status_t validation error from the phantom (e.g. @ref LE_ERR_CRC_MISMATCH).
 */
le_status_t le_storage_commit_upload(le_storage_t* storage, uint8_t target_slot, size_t expected_size);

/**
 * @brief Boot helper that attempts to load and run a configuration slot, falling
 * back to an embedded demo program if the slot is empty or unformatted.
 *
 * Execution flow:
 *   1. Tries to activate @p default_slot (typically Slot 0) via @ref le_storage_activate_slot.
 *      If successful, starts the VM via @ref le_vm_start and sets @p out_source_desc
 *      to a descriptive string (e.g. "Storage Slot 0").
 *   2. If the slot has not been flashed or fails verification, attempts to load
 *      @p demo_prog via @ref le_loader_load. If successful, starts the VM and
 *      sets @p out_source_desc (e.g. "Embedded Demo ROM").
 *   3. If both fail, leaves the VM stopped and returns @ref LE_ERR_STORAGE.
 *
 * @param[in,out] vm Pointer to the virtual machine instance.
 * @param[in,out] storage Pointer to the storage instance (may be NULL if no storage).
 * @param[in] default_slot Slot index to attempt to boot from (usually 0).
 * @param[in] demo_prog Optional pointer to embedded fallback bytecode array (may be NULL).
 * @param[in] demo_len Length of fallback bytecode array in bytes.
 * @param[out] out_source_desc Optional pointer to receive a human-readable description
 *                             of which source was activated (may be NULL).
 * @return `LE_OK` if a program was loaded and the VM started; otherwise error code.
 */
le_status_t le_storage_autostart_or_demo(le_vm_t* vm, le_storage_t* storage,
                                        uint8_t default_slot,
                                        const uint8_t* demo_prog, size_t demo_len,
                                        const char** out_source_desc);

#ifdef __cplusplus
}
#endif

#endif /* LE_STORAGE_H */

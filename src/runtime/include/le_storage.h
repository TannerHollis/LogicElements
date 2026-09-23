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

/* ========================================================================== */
/* Adopter-configurable storage capacity                                      */
/* ========================================================================== */

#ifndef LE_MAX_CONFIG_SLOTS
/** @brief Default number of non-volatile configuration slots. */
#define LE_MAX_CONFIG_SLOTS     3
#endif

#ifndef LE_SLOT_SIZE_BYTES
/** @brief Default byte capacity allocated per configuration slot. */
#define LE_SLOT_SIZE_BYTES      2048
#endif

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
    uint8_t         ram_partitions[LE_MAX_CONFIG_SLOTS][LE_SLOT_SIZE_BYTES]; /**< In-memory storage partitions. */
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

#ifdef __cplusplus
}
#endif

#endif /* LE_STORAGE_H */

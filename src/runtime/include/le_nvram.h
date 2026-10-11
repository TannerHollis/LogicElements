/**
 * @file le_nvram.h
 * @brief Non-Volatile RAM (NVRAM) and wear-leveling management subsystem.
 *
 * Provides persistent storage for:
 *   1. Retentive process-image registers (%B, %I, %F, %C).
 *   2. Sequence of Events Recorder (SER) transition logs.
 *   3. Event Recorder (Oscillography) waveform fault captures.
 *
 * Supports two physical backing models:
 *   - Direct byte-addressable NVRAM (external SPI/I2C FRAM, MRAM, battery-backed SRAM).
 *   - On-chip or external SPI NOR Flash using the runtime's built-in log-structured
 *     wear-leveling journal (monotonically sequenced sectors, rolling wear-count,
 *     append-only records, and compaction).
 */

#ifndef LE_NVRAM_H
#define LE_NVRAM_H

#include "le_types.h"
#include "le_process_image.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "le_board_config.h"

/* Logical partition offsets */
#define LE_NVRAM_OFFSET_MANIFEST    0x00000000UL
#define LE_NVRAM_OFFSET_REGS        0x00000040UL
#define LE_NVRAM_SLOT_REGS_OFFSET(slot) (LE_NVRAM_OFFSET_REGS + ((uint32_t)(slot) * LE_NVRAM_REGS_BYTES))
#define LE_NVRAM_OFFSET_SER         (LE_NVRAM_OFFSET_REGS + ((uint32_t)LE_MAX_CONFIG_SLOTS * LE_NVRAM_REGS_BYTES))
#define LE_NVRAM_OFFSET_EVENT_REC   (LE_NVRAM_OFFSET_SER + LE_NVRAM_SER_BYTES)

#define LE_NVRAM_MAGIC              0x4E56524DUL /* ASCII 'NVRM' */
#define LE_NVRAM_VERSION            1

/**
 * @brief Partition table / manifest stored at logical offset 0.
 */
typedef struct {
    uint32_t magic;             /**< LE_NVRAM_MAGIC ('NVRM') */
    uint16_t version;           /**< Layout version (1) */
    uint16_t flags;             /**< Reserved flags */
    uint32_t total_bytes;       /**< Total capacity */
    uint32_t config_slots;      /**< Number of configuration slots allocated (LE_MAX_CONFIG_SLOTS) */
    uint32_t regs_per_slot;     /**< Retentive register capacity per slot (LE_NVRAM_REGS_BYTES) */
    uint32_t regs_offset;       /**< Base offset of retentive registers */
    uint32_t regs_bytes;        /**< Total size of retentive register area across all slots */
    uint32_t ser_offset;        /**< Offset of SER log */
    uint32_t ser_bytes;         /**< Size of SER log */
    uint32_t event_rec_offset;  /**< Offset of Event Recorder buffer */
    uint32_t event_rec_bytes;   /**< Size of Event Recorder buffer */
    uint32_t crc32;             /**< CRC32 of manifest fields */
} le_nvram_manifest_t;

/* ========================================================================== */
/* Retentive Register Configuration                                           */
/* ========================================================================== */

#define LE_NVRAM_MAX_RANGES         8
#define LE_NVRAM_REGS_MAGIC         0x4E565247UL /* ASCII 'NVRG' */

typedef struct {
    uint16_t start_addr;        /**< Virtual address (e.g. LE_ADDR_MAKE_BOOL_REG(0)) */
    uint16_t count;             /**< Number of registers in range */
} le_nvram_range_t;

typedef struct {
    uint16_t         range_count;
    le_nvram_range_t ranges[LE_NVRAM_MAX_RANGES];
} le_nvram_reg_cfg_t;

typedef struct {
    uint32_t magic;             /**< LE_NVRAM_REGS_MAGIC */
    uint16_t range_count;       /**< Number of serialized ranges */
    uint16_t total_payload_bytes;/**< Byte count of packed register data */
    uint32_t crc32;             /**< Checksum of payload */
} le_nvram_regs_header_t;

/* ========================================================================== */
/* Flash Wear-Leveling Structures & Driver Interface                          */
/* ========================================================================== */

#define LE_WL_SECTOR_MAGIC          0x574C5345UL /* ASCII 'WLSE' */
#define LE_WL_RECORD_MARKER         0xA55AU

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;             /**< LE_WL_SECTOR_MAGIC */
    uint32_t seq_num;           /**< Monotonic sector sequence ID */
    uint32_t erase_count;       /**< Number of erase cycles for this sector */
    uint32_t crc32;             /**< Header checksum */
} le_wl_sector_header_t;

typedef struct {
    uint16_t marker;            /**< LE_WL_RECORD_MARKER */
    uint16_t len;               /**< Payload length in bytes */
    uint32_t nv_offset;         /**< Logical NVRAM offset */
    uint32_t crc32;             /**< Payload CRC32 */
} le_wl_record_header_t;
#pragma pack(pop)

/**
 * @brief Hardware driver interface for flash-backed wear leveling.
 *
 * The board port implementer provides sector erase, block program, and
 * read callbacks. The runtime's log-structured journal handles all wear
 * leveling, compaction, rollover, and integrity checks automatically.
 */
typedef struct {
    uint32_t flash_base_offset; /**< Physical flash offset where wear-level sectors begin */
    uint32_t sector_size;       /**< Sector / block erase size (e.g. 4096, LE_NVRAM_BLOCK_SIZE) */
    uint32_t page_size;         /**< Write page size (e.g. 256, LE_NVRAM_PAGE_SIZE; 0 defaults to 256) */
    uint32_t sector_count;      /**< Number of sectors allocated (must be >= 2) */
    bool (*erase_sector)(uint32_t sector_idx, uint32_t physical_sector_addr);
    bool (*program)(uint32_t physical_addr, const uint8_t* data, size_t len);
    bool (*read)(uint32_t physical_addr, uint8_t* data, size_t len);
} le_flash_wl_driver_t;

/* ========================================================================== */
/* Runtime NVRAM Subsystem Functions                                          */
/* ========================================================================== */

/**
 * @brief Initializes direct byte-addressable NVRAM mode (FRAM / EEPROM / RAM mirror).
 *
 * In this mode, writes and reads dispatch directly to the active HAL nvram callbacks
 * or local memory without flash sector erase wear-leveling overhead.
 */
bool le_nvram_init_direct(void);

/**
 * @brief Mounts the wear-leveling flash driver into the NVRAM subsystem.
 *
 * Scans existing flash sectors, determines the active sequence, recovers
 * uncompacted records into the logical cache, and prepares the journal for writes.
 *
 * @param driver Pointer to the flash driver configuration.
 * @return True if initialized/mounted successfully; false otherwise.
 */
bool le_nvram_init_flash(const le_flash_wl_driver_t* driver);

/**
 * @brief Reads bytes from logical NVRAM.
 *
 * @param offset Logical byte offset (0 to LE_NVRAM_TOTAL_BYTES - 1).
 * @param buf Destination buffer pointer.
 * @param len Number of bytes to read.
 * @return True on success; false on out-of-bounds or hardware error.
 */
bool le_nvram_read(uint32_t offset, void* buf, size_t len);

/**
 * @brief Writes bytes to logical NVRAM.
 *
 * If flash wear-leveling is active, this appends a log record to the active
 * sector (performing compaction/rollover if the sector fills).
 *
 * @param offset Logical byte offset (0 to LE_NVRAM_TOTAL_BYTES - 1).
 * @param buf Source data buffer pointer.
 * @param len Number of bytes to write.
 * @return True on success; false on out-of-bounds or hardware error.
 */
bool le_nvram_write(uint32_t offset, const void* buf, size_t len);

/**
 * @brief Logically erases or zeroes a region of logical NVRAM.
 *
 * If offset is 0 and len >= LE_NVRAM_TOTAL_BYTES, this triggers a full format.
 * Otherwise, this zeroes the specified region and updates persistent storage.
 *
 * @param offset Logical byte offset.
 * @param len Number of bytes to erase.
 * @return True on success; false on out-of-bounds or hardware error.
 */
bool le_nvram_erase(uint32_t offset, size_t len);

/**
 * @brief Fully formats and resets the NVRAM partition and wear-leveling journal.
 *
 * Erases all allocated flash sectors (or direct memory) and writes a fresh manifest header.
 *
 * @return True on success; false on hardware error.
 */
bool le_nvram_format(void);

/**
 * @brief Serializes and saves retentive variables for a specific config slot to NVRAM.
 *
 * @param slot Target configuration slot index (0 to LE_MAX_CONFIG_SLOTS - 1).
 * @param cfg Configured retentive variable address ranges.
 * @param img Active process image.
 * @return True if written successfully; false otherwise.
 */
bool le_nvram_save_regs_slot(uint8_t slot, const le_nvram_reg_cfg_t* cfg, const le_process_image_t* img);

/**
 * @brief Restores retentive variables for a specific config slot from NVRAM into the process image.
 *
 * Verifies the header magic and CRC32 before restoring values.
 *
 * @param slot Source configuration slot index (0 to LE_MAX_CONFIG_SLOTS - 1).
 * @param cfg Configured retentive variable address ranges.
 * @param img Target process image to populate.
 * @return True if valid retentive data was restored; false on CRC mismatch or uninitialized NVRAM.
 */
bool le_nvram_restore_regs_slot(uint8_t slot, const le_nvram_reg_cfg_t* cfg, le_process_image_t* img);

/**
 * @brief Serializes and saves retentive variables for slot 0 to NVRAM (convenience alias).
 *
 * @param cfg Configured retentive variable address ranges.
 * @param img Active process image.
 * @return True if written successfully; false otherwise.
 */
bool le_nvram_save_regs(const le_nvram_reg_cfg_t* cfg, const le_process_image_t* img);

/**
 * @brief Restores retentive variables for slot 0 from NVRAM into the process image (convenience alias).
 *
 * Verifies the header magic and CRC32 before restoring values.
 *
 * @param cfg Configured retentive variable address ranges.
 * @param img Target process image to populate.
 * @return True if valid retentive data was restored; false on CRC mismatch or uninitialized NVRAM.
 */
bool le_nvram_restore_regs(const le_nvram_reg_cfg_t* cfg, le_process_image_t* img);

/**
 * @brief Flushes pending writes or forces compaction to persistent storage.
 */
bool le_nvram_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_NVRAM_H */

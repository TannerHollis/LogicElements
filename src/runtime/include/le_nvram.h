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

#include "le_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logical partition manifest offset */
#define LE_NVRAM_OFFSET_MANIFEST    0x00000000UL

#define LE_NVRAM_MAGIC              0x4E56524DUL /* ASCII 'NVRM' */
#define LE_NVRAM_VERSION            2

/**
 * @brief Dynamic partition manifest stored at logical offset 0.
 *
 * Driven by circuit requirements without fixed compile-time boundaries.
 */
typedef struct {
    uint32_t magic;             /**< LE_NVRAM_MAGIC ('NVRM') */
    uint16_t version;           /**< Layout version (2) */
    uint16_t flags;             /**< Reserved flags */
    uint32_t total_bytes;       /**< Total capacity of the physical NVRAM medium */
    uint32_t config_slots;      /**< Number of configuration slots allocated (LE_MAX_CONFIG_SLOTS) */
    uint32_t regs_per_slot;     /**< Dynamic retentive register capacity per slot */
    uint32_t regs_offset;       /**< Base logical offset of retentive registers */
    uint32_t regs_bytes;        /**< Total size of retentive register area across all slots */
    uint32_t ser_offset;        /**< Offset of SER log (0 if not allocated) */
    uint32_t ser_bytes;         /**< Size of SER log (0 if not allocated) */
    uint32_t event_rec_offset;  /**< Offset of Event Recorder buffer (0 if not allocated) */
    uint32_t event_rec_bytes;   /**< Size of Event Recorder buffer (0 if not allocated) */
    uint32_t crc32;             /**< CRC32 of manifest fields */
} le_nvram_manifest_t;

/* Backward-compatibility default fallbacks for unconfigured operation */
#ifndef LE_NVRAM_DEFAULT_REGS_BYTES
#define LE_NVRAM_DEFAULT_REGS_BYTES 1024UL
#endif
#ifndef LE_NVRAM_DEFAULT_SER_BYTES
#define LE_NVRAM_DEFAULT_SER_BYTES  3072UL
#endif

/* Dynamic partition query and configuration helpers */
#define LE_NVRAM_OFFSET_REGS            le_nvram_get_regs_offset(0)
#define LE_NVRAM_SLOT_REGS_OFFSET(slot) le_nvram_get_regs_offset(slot)
#define LE_NVRAM_OFFSET_SER             le_nvram_get_ser_offset()
#define LE_NVRAM_OFFSET_EVENT_REC       le_nvram_get_event_rec_offset()

/* ========================================================================== */
/* Retentive Register Configuration                                           */
/* ========================================================================== */

#define LE_NVRAM_REGS_MAGIC         0x4E565247UL /* ASCII 'NVRG' */

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
    uint32_t max_erase_cycles;  /**< Rated endurance per sector (e.g. 10000 to 100000; 0 uses LE_NVRAM_MAX_ERASE_CYCLES) */
    bool (*erase_sector)(uint32_t sector_idx, uint32_t physical_sector_addr);
    bool (*program)(uint32_t physical_addr, const uint8_t* data, size_t len);
    bool (*read)(uint32_t physical_addr, uint8_t* data, size_t len);
} le_flash_wl_driver_t;

/**
 * @brief NVRAM wear-leveling and health diagnostic statistics.
 */
typedef struct {
    bool     is_flash;             /**< True if running in flash wear-leveling mode, false if direct FRAM/EEPROM/RAM */
    uint32_t active_sector_idx;    /**< Currently active journal sector index */
    uint32_t active_seq;           /**< Monotonic sequence number of current sector */
    uint32_t active_erase_count;   /**< Erase cycles on the active sector */
    uint32_t max_erase_count;      /**< Maximum erase cycles observed across all flash sectors */
    uint32_t min_erase_count;      /**< Minimum erase cycles observed across all flash sectors */
    uint32_t avg_erase_count;      /**< Average erase cycles across all flash sectors */
    uint32_t rated_erase_cycles;   /**< Rated endurance limit (e.g. 100,000 cycles) */
    float    wear_pct;             /**< Flash wear level percentage (0.00% to 100.00%) */
    float    health_pct;           /**< Remaining health percentage (100.00% - wear_pct) */
    uint32_t sector_bytes_used;    /**< Bytes written in current sector */
    uint32_t sector_bytes_free;    /**< Unwritten slack bytes remaining in current sector */
    uint32_t sector_count;         /**< Total sectors allocated in wear pool */
    uint32_t sector_size;          /**< Size of each sector in bytes */
} le_nvram_stats_t;

/* ========================================================================== */
/* Runtime NVRAM Subsystem Functions                                          */
/* ========================================================================== */

/**
 * @brief Retrieves wear-leveling health and erase statistics.
 *
 * @param stats Pointer to le_nvram_stats_t structure to receive metrics.
 * @return True on success; false if stats pointer is NULL or NVRAM is uninitialized.
 */
bool le_nvram_get_stats(le_nvram_stats_t* stats);

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

/* ========================================================================== */
/* Dynamic Circuit-Driven Partitioning Functions                              */
/* ========================================================================== */

/**
 * @brief Dynamically configures the NVRAM partition layout based on circuit requirements.
 *
 * Slices the board's total NVRAM medium dynamically:
 *   offset 0:                 Manifest header (sizeof(le_nvram_manifest_t))
 *   offset manifest_size:     Retentive registers (config_slots * regs_per_slot)
 *   offset regs_end:          Sequence of Events Recorder (ser_bytes)
 *   offset ser_end:           Event Recorder buffer (event_bytes)
 *
 * @param regs_per_slot Retentive register bytes allocated per configuration slot.
 * @param ser_records Number of SER transition records to buffer in NVRAM (0 = disabled).
 * @param event_bytes Total Event Recorder buffer size in bytes (0 = disabled).
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_CAPACITY if requested sizes exceed total NVRAM.
 */
le_status_t le_nvram_configure_layout(uint32_t regs_per_slot, uint32_t ser_records, uint32_t event_bytes);

/**
 * @brief Returns the total byte capacity of the physical NVRAM medium.
 */
uint32_t le_nvram_get_total_bytes(void);

/**
 * @brief Returns the base logical byte offset for a configuration slot's retentive registers.
 *
 * @param slot Configuration slot index (0 .. LE_MAX_CONFIG_SLOTS - 1).
 * @return Byte offset within logical NVRAM, or 0 if unallocated.
 */
uint32_t le_nvram_get_regs_offset(uint8_t slot);

/**
 * @brief Returns the retentive register capacity allocated per configuration slot.
 */
uint32_t le_nvram_get_regs_bytes(uint8_t slot);

/**
 * @brief Returns the base logical byte offset of the Sequence of Events (SER) log.
 *
 * @return Byte offset within logical NVRAM, or 0 if SER is not allocated in NVRAM.
 */
uint32_t le_nvram_get_ser_offset(void);

/**
 * @brief Returns the byte size allocated for the Sequence of Events (SER) log.
 */
uint32_t le_nvram_get_ser_bytes(void);

/**
 * @brief Returns the base logical byte offset of the Event Recorder waveform buffer.
 *
 * @return Byte offset within logical NVRAM, or 0 if Event Recorder is not allocated in NVRAM.
 */
uint32_t le_nvram_get_event_rec_offset(void);

/**
 * @brief Returns the byte size allocated for the Event Recorder waveform buffer.
 */
uint32_t le_nvram_get_event_rec_bytes(void);

/**
 * @brief Retrieves the active NVRAM manifest descriptor.
 */
const le_nvram_manifest_t* le_nvram_get_manifest(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_NVRAM_H */

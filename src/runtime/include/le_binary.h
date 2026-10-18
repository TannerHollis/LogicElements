/**
 * @file le_binary.h
 * @brief Binary format definitions, container headers, chunk tags, and instruction layouts.
 */

#ifndef LE_BINARY_H
#define LE_BINARY_H

#include "le_config.h"
#include "le_opcode_defs.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Binary Format Constants                                                    */
/* ========================================================================== */

#define LE_BIN_MAGIC            0x4C454231  /* ASCII "LEB1" - flat v12 binary */
#define LE_BIN_MAGIC_V1         0x4C454231  /* ASCII "LEB1" */
#define LE_BIN_MAGIC_V2         0x4C454232  /* ASCII "LEB2" - chunked container format */
#define LE_BIN_VERSION          12  // Current flat binary version
#define LE_CONTAINER_VERSION    13  // Chunked container format version

#define LE_FOURCC(a, b, c, d) \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

#define LE_CHUNK_TAG_HEAD       LE_FOURCC('H', 'E', 'A', 'D')
#define LE_CHUNK_TAG_INST       LE_FOURCC('I', 'N', 'S', 'T')
#define LE_CHUNK_TAG_BLCK       LE_FOURCC('B', 'L', 'C', 'K')
#define LE_CHUNK_TAG_STAT       LE_FOURCC('S', 'T', 'A', 'T')
#define LE_CHUNK_TAG_SIMG       LE_FOURCC('S', 'I', 'M', 'G')
#define LE_CHUNK_TAG_ALIS       LE_FOURCC('A', 'L', 'I', 'S')
#define LE_CHUNK_TAG_TIME       LE_FOURCC('T', 'I', 'M', 'E')
#define LE_CHUNK_TAG_RATE       LE_FOURCC('R', 'A', 'T', 'E')
#define LE_CHUNK_TAG_NVMD       LE_FOURCC('N', 'V', 'M', 'D')
#define LE_CHUNK_TAG_SIGN       LE_FOURCC('S', 'I', 'G', 'N')

/* Program Header Flags */
#define LE_FLAG_AUTOSTART       (1U << 0)
#define LE_FLAG_WATCHDOG_EN     (1U << 1)

/**
 * @brief Binary directive describing a group of same-kind state blocks.
 * Appended after the (variable-arity) block table so the loader can reserve
 * heap slices and bind pointers at load time. Instances of one kind occupy
 * contiguous runtime-block rows.
 */
typedef struct {
    uint8_t  kind;    /**< le_block_kind_t (LE_BLK_NONE is ignored). */
    uint8_t  count;   /**< Number of instances of @p kind. */
    uint16_t size;    /**< Bytes of one instance's state struct. */
} le_state_desc_t;

/** @brief Bytes of one @ref le_state_desc_t record. */
#define LE_STATE_DESC_BYTES 4

/* ========================================================================== */
/* Binary instruction and header structures (packed)                         */
/* ========================================================================== */

#pragma pack(push, 1)

/**
 * @brief Packed binary file header for compiled LogicElements bytecode (.lebin).
 */
typedef struct {
    uint32_t magic;             /**< Magic identifier: 0x4C454231 (ASCII "LEB1"). */
    uint16_t version;           /**< Binary format version number (currently 12). */
    uint16_t flags;             /**< Program execution flags (for example, LE_FLAG_AUTOSTART). */
    uint16_t instruction_count; /**< Total number of instructions in the bytecode payload. */
    uint16_t digital_in_count;  /**< Number of digital input channels (%IN) required. */
    uint16_t digital_out_count; /**< Number of digital output channels (%OUT) required. */
    uint16_t bool_reg_count;    /**< Number of internal boolean registers (%B) required. */
    uint16_t float_reg_count;   /**< Number of float registers (%F) required. */
    uint16_t complex_reg_count; /**< Number of complex registers (%C, real+imag pairs) required. */
    uint16_t int_reg_count;     /**< Number of int registers (%I) required. */
    uint16_t analog_in_count;   /**< Number of analog input channels (%AIN) required. */
    uint16_t analog_out_count;  /**< Number of analog output channels (%AOUT) required. */
    uint16_t block_count;       /**< Number of variable-arity block descriptors in the payload. */
    uint16_t state_desc_count;  /**< Number of state-group records (kinds present) in the payload. */
    uint32_t state_img_len;     /**< Bytes of the preconfigured state image (copied to RAM at load). */
    uint16_t alias_count;       /**< Number of user-declared register aliases (le_alias_t entries) appended after the state image. */
    uint16_t timing_count;      /**< 1 when an 8-byte le_timing_desc_t follows the alias table (0 otherwise). */
    uint16_t reserved;          /**< Reserved / padding to keep header aligned to 44 bytes (multiple of 4). */
    uint32_t crc32;             /**< IEEE 802.3 CRC32 of the whole payload (instructions + block + state + state image + aliases + timing). */
} le_header_t;

/**
 * @brief Container file header for chunked LogicElements binaries (.lebin v13+).
 */
typedef struct {
    uint32_t magic;         /**< Magic identifier: 0x4C454232 (ASCII "LEB2"). */
    uint16_t version;       /**< Container version (LE_CONTAINER_VERSION = 13). */
    uint16_t flags;         /**< Container execution flags (e.g. LE_FLAG_AUTOSTART). */
    uint32_t total_size;    /**< Total container file size in bytes including this header. */
    uint16_t chunk_count;   /**< Number of chunks stored in this container. */
    uint16_t reserved;      /**< Reserved / padding for 32-bit alignment. */
    uint32_t header_crc32;  /**< CRC32 of container header (computed with header_crc32 field zeroed). */
} le_container_header_t;

#define LE_CONTAINER_HEADER_BYTES sizeof(le_container_header_t)

/**
 * @brief Chunk descriptor header in the chunked container format.
 */
typedef struct {
    uint32_t tag;           /**< 4-byte FourCC identifier (e.g. LE_CHUNK_TAG_HEAD). */
    uint32_t length;        /**< Payload length in bytes (excluding chunk header). */
    uint32_t crc32;         /**< IEEE 802.3 CRC32 of chunk payload. */
    uint32_t flags;         /**< Chunk attributes/flags (0 for standard chunk). */
} le_chunk_header_t;

#define LE_CHUNK_HEADER_BYTES sizeof(le_chunk_header_t)

/**
 * @brief Optional timing descriptor appended after the alias table (10 bytes).
 */
typedef struct {
    uint32_t abstract_cycles;   /**< Total worst-case abstract cost (compiler units). */
    uint16_t safety_margin_pct; /**< Compiler safety multiplier (e.g. 150 = 1.5x). */
    uint16_t design_scan_rate_hz; /**< Circuit-declared fixed scan rate (0 = unspecified). */
    uint16_t rsvd;              /**< Reserved (0). */
} le_timing_desc_t;

/** @brief Bytes of one @ref le_timing_desc_t record. */
#define LE_TIMING_DESC_BYTES 10

/**
 * @brief Multi-rate execution domain descriptor chunk payload.
 */
typedef struct {
    uint16_t rate_fast_count; /**< Number of instructions in the Fast/Protection domain (runs every tick). */
    uint16_t rate_med_count;  /**< Number of instructions in the Medium/Control domain. */
    uint16_t rate_med_div;    /**< Scan divisor for Medium domain (e.g. 48 for 100 Hz at 4800 Hz). */
    uint16_t rate_slow_div;   /**< Scan divisor for Slow domain (e.g. 480 for 10 Hz at 4800 Hz). */
} le_rate_desc_t;

#define LE_RATE_DESC_BYTES sizeof(le_rate_desc_t)

#define LE_NVRAM_MAX_RANGES 8

/**
 * @brief Address range for retentive registers.
 */
typedef struct {
    uint16_t start_addr;        /**< Virtual address (e.g. LE_ADDR_MAKE_BOOL_REG(0)) */
    uint16_t count;             /**< Number of registers in range */
} le_nvram_range_t;

/**
 * @brief Circuit-declared non-volatile RAM requirements descriptor chunk (NVMD).
 */
typedef struct {
    uint32_t retentive_bytes;       /**< Total bytes required for retentive registers payload + header */
    uint32_t ser_records;           /**< Maximum SER transition event records to retain (0 = none) */
    uint32_t ser_bytes;             /**< Exact bytes for SER NVRAM log */
    uint32_t event_records;         /**< Maximum Event Recorder sample records to retain (0 = none) */
    uint32_t event_bytes;           /**< Exact bytes for Event Recorder NVRAM buffer */
    uint16_t retentive_range_count; /**< Number of retentive address ranges */
    uint16_t reserved;              /**< Alignment padding */
    le_nvram_range_t ranges[LE_NVRAM_MAX_RANGES]; /**< Retentive register address ranges */
} le_nvram_desc_t;

#define LE_NVRAM_DESC_BYTES sizeof(le_nvram_desc_t)

/**
 * @brief Fixed 8-byte instruction format for the virtual machine.
 */
typedef struct {
    uint8_t  opcode;    /**< Operation code from @ref le_opcode_t. */
    uint8_t  modifier;  /**< Bitmask of input/output inversion modifiers or device index. */
    uint16_t in_a;      /**< 16-bit process image address for operand A. */
    uint16_t in_b;      /**< 16-bit process image address for operand B. */
    uint16_t out;       /**< 16-bit process image address for the destination result. */
} le_instruction_t;

/**
 * @brief Variable-arity block descriptor (referenced by LE_OP_BLOCK's in_a).
 */
typedef struct {
    uint8_t  in_count;   /**< Number of live input operands (0..N). */
    uint8_t  out_count;  /**< Number of output operands (1..M). */
    uint8_t  flags;      /**< Reserved for per-function hints (edge, reset, ...). */
    uint8_t  reserved;   /**< Reserved (0). */
} le_block_desc_t;

/** @brief Bytes of a descriptor header (excludes the variable arg array). */
#define LE_BLOCK_DESC_HEADER_BYTES 4

/**
 * @brief A single user-declared register alias.
 */
typedef struct {
    char     name[LE_ALIAS_NAME_MAX]; /* Alias, left-aligned, NUL-terminated (max 7 chars). */
    uint8_t  kind;                    /* Reserved; 0 = generic/writable register. */
    uint8_t  pad;                     /* Reserved (0). */
    uint16_t addr;                    /* Process-image address the alias resolves to. */
} le_alias_t;                         /* 11 bytes, packed */

/** @brief Bytes of one alias table entry. */
#define LE_ALIAS_BYTES              (LE_ALIAS_NAME_MAX + 1 + 1 + 2)

#pragma pack(pop)

/**
 * @brief Timing / achievability report for a loaded program.
 */
typedef struct {
    uint32_t scan_period_us;    /**< Fixed scan period configured on the VM (0 = none). */
    uint32_t worst_case_us;     /**< Estimated worst-case scan duration in microseconds. */
    uint32_t margin_us;         /**< scan_period_us - worst_case_us (0 when infeasible/unconfigured). */
    uint32_t abstract_cycles;   /**< Compiler cost units carried in the binary. */
    uint16_t margin_pct;        /**< 100 * margin_us / scan_period_us. */
    uint16_t safety_margin_pct; /**< Compiler safety multiplier carried in the binary. */
    uint8_t  feasible;          /**< 1 when worst_case_us <= scan_period_us. */
} le_timing_t;

/**
 * @brief Zero-copy view of chunks parsed from a chunked container (.lebin v13).
 */
typedef struct {
    const le_container_header_t* container;
    const le_header_t*           head;
    const le_instruction_t*      inst;
    uint32_t                     inst_count;
    const le_block_desc_t*       blck;
    size_t                       blck_bytes;
    uint32_t                     blck_count;
    const le_state_desc_t*       stat;
    uint32_t                     stat_count;
    const uint8_t*               simg;
    uint32_t                     simg_len;
    const le_alias_t*            alis;
    uint32_t                     alis_count;
    const le_timing_desc_t*      time;
    const le_rate_desc_t*        rate;
    const le_nvram_desc_t*       nvmd;
    const uint8_t*               sign;
    uint32_t                     sign_len;
} le_chunked_view_t;

#ifdef __cplusplus
}
#endif

#endif /* LE_BINARY_H */

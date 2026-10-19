/**
 * @file le_config.h
 * @brief LogicElements Universal Master Configuration Header.
 *
 * This header serves as the SINGLE SOURCE OF TRUTH for all hardware
 * capabilities, memory partitioning, execution topology, storage backends,
 * and subsystem switches across the entire LogicElements runtime.
 *
 * PRECEDENCE RULES:
 *   1. Custom Board File: If -DLE_CONFIG_FILE="path/to/my_board.h" is passed,
 *      that file is included first and its defines take absolute priority.
 *   2. Command-Line Defines: Any -D<FLAG>=<VAL> in CMake or Makefiles overrides
 *      the default value of any option defined below.
 *   3. Sane Embedded Defaults: If no custom configuration is supplied, safe,
 *      deterministic, single-core, execute-in-place defaults are applied.
 */

#ifndef LE_CONFIG_H
#define LE_CONFIG_H

#ifdef LE_CONFIG_FILE
#include LE_CONFIG_FILE
#elif defined(__has_include)
#if __has_include("le_board_config.h")
#include "le_board_config.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LE_ALIGN4
#if defined(__GNUC__) || defined(__clang__)
#define LE_ALIGN4 __attribute__((aligned(4)))
#elif defined(_MSC_VER)
#define LE_ALIGN4 __declspec(align(4))
#else
#define LE_ALIGN4
#endif
#endif


/* ========================================================================== */
/* 1. Processor Core & Execution Topology                                     */
/* ========================================================================== */

/**
 * @brief Core Processing Topology:
 *   LE_CORE_SINGLE (0) : Portable single-core execution (Core 0 does I/O + VM).
 *   LE_CORE_AMP    (1) : Asymmetric Multiprocessing (Core 0 = Real-Time I/O, ADC,
 *                        PWM, Comms; Core 1 = Dedicated VM bytecode worker).
 *   LE_CORE_SMP    (2) : Symmetric Multiprocessing / RTOS multithreaded worker.
 */
#define LE_CORE_SINGLE                  0
#define LE_CORE_AMP                     1
#define LE_CORE_SMP                     2

#ifndef LE_CORE_MODE
#define LE_CORE_MODE                    LE_CORE_SINGLE
#endif

/**
 * @brief Inter-Core Watchdog Timeout (Microseconds) in AMP mode.
 * If Core 1 fails to complete a scan cycle within this budget, Core 0 trips the
 * safety watchdog and de-energizes all physical outputs.
 */
#ifndef LE_AMP_WATCHDOG_TIMEOUT_US
#define LE_AMP_WATCHDOG_TIMEOUT_US      100000UL /* 100 ms default */
#endif

/**
 * @brief Bytecode Execution Memory Mode:
 *   LE_EXEC_IN_PLACE (0) : VM fetches instructions directly from memory-mapped
 *                          Flash / ROM via XIP. Saves RAM completely.
 *   LE_EXEC_IN_RAM   (1) : VM copies bytecode to RAM before execution.
 *                          Required for microprocessors without memory-mapped flash.
 */
#define LE_EXEC_IN_PLACE                0
#define LE_EXEC_IN_RAM                  1

#ifndef LE_EXEC_MODE
#define LE_EXEC_MODE                    LE_EXEC_IN_PLACE
#endif

/**
 * @brief VM Instruction Dispatch Engine:
 *   LE_DISPATCH_AUTO   (0) : Auto-detect (Computed goto on GCC/Clang, switch-case elsewhere).
 *   LE_DISPATCH_SWITCH (1) : Standard ANSI C while-switch loop (portable, small code size).
 *   LE_DISPATCH_DIRECT (2) : Direct threaded code (GCC/Clang labels-as-values, lowest latency).
 */
#define LE_DISPATCH_AUTO                0
#define LE_DISPATCH_SWITCH              1
#define LE_DISPATCH_DIRECT              2

#ifndef LE_DISPATCH_MODE
#define LE_DISPATCH_MODE                LE_DISPATCH_AUTO
#endif

#if (LE_DISPATCH_MODE == LE_DISPATCH_DIRECT)
#define LE_USE_THREADED_DISPATCH        1
#else
#define LE_USE_THREADED_DISPATCH        0
#endif

/* ========================================================================== */
/* 2. Memory Hierarchy & RAM Partitioning                                     */
/* ========================================================================== */

/**
 * @brief Unified RAM Workspace (Bytes).
 * Sized dynamically at program load by le_loader. Holds the packed internal
 * register arena (%B, %I, %F, %C) and preconfigured state blocks (timers,
 * filters, protection relays).
 */
#ifndef LE_RAM_WORKSPACE_BYTES
#define LE_RAM_WORKSPACE_BYTES          2048
#endif

/**
 * @brief Storage Partitioning RAM Strategy:
 *   LE_STORAGE_PARTITION_ZERO_RAM     (0) : Pure XIP. Zero RAM partitions allocated in
 *                                           le_storage_t. Program bytecode stays in Flash.
 *   LE_STORAGE_PARTITION_SINGLE_CACHE (1) : Exactly 1 slot buffer in RAM. Used for active
 *                                           caching or staging uploads.
 *   LE_STORAGE_PARTITION_FULL_RAM     (2) : All slots + phantom scratch buffer in RAM.
 *                                           ((LE_MAX_CONFIG_SLOTS + 1) * LE_SLOT_SIZE_BYTES).
 */
#define LE_STORAGE_PARTITION_ZERO_RAM     0
#define LE_STORAGE_PARTITION_SINGLE_CACHE 1
#define LE_STORAGE_PARTITION_FULL_RAM     2

#ifndef LE_STORAGE_PARTITION_MODE
#define LE_STORAGE_PARTITION_MODE       LE_STORAGE_PARTITION_FULL_RAM
#endif

/**
 * @brief Number of physical in-memory slot buffers (derived from partition mode).
 */
#ifndef LE_PHYSICAL_SLOT_COUNT
#if (LE_STORAGE_PARTITION_MODE == LE_STORAGE_PARTITION_ZERO_RAM)
#define LE_PHYSICAL_SLOT_COUNT          0
#elif (LE_STORAGE_PARTITION_MODE == LE_STORAGE_PARTITION_SINGLE_CACHE)
#define LE_PHYSICAL_SLOT_COUNT          1
#else
#define LE_PHYSICAL_SLOT_COUNT          (LE_MAX_CONFIG_SLOTS + 1)
#endif
#endif

/**
 * @brief Program Storage Hardware Backend:
 *   LE_STORAGE_BACKEND_FLASH (0) : On-chip or external SPI/QSPI NOR Flash (via HAL).
 *   LE_STORAGE_BACKEND_RAM   (1) : In-Memory RAM Mirror (simulation / bring-up).
 */
#define LE_STORAGE_BACKEND_FLASH        0
#define LE_STORAGE_BACKEND_RAM          1

#ifndef LE_STORAGE_BACKEND
#define LE_STORAGE_BACKEND              LE_STORAGE_BACKEND_FLASH
#endif

/* ========================================================================== */
/* 3. Program Storage & Slot Budgeting                                        */
/* ========================================================================== */

#ifndef LE_MAX_CONFIG_SLOTS
#define LE_MAX_CONFIG_SLOTS             3       /**< Number of on-flash program slots */
#endif

#ifndef LE_SLOT_SIZE_BYTES
#define LE_SLOT_SIZE_BYTES              2048    /**< Maximum binary bytecode capacity per slot */
#endif

#ifndef LE_STORAGE_PAGE_SIZE
#define LE_STORAGE_PAGE_SIZE            256     /**< Default flash page write size */
#endif

#ifndef LE_STORAGE_SECTOR_SIZE
#define LE_STORAGE_SECTOR_SIZE          4096    /**< Default flash sector erase size */
#endif

/* ========================================================================== */
/* 4. Operational NVRAM & Flash Wear-Leveling                                 */
/* ========================================================================== */

/**
 * @brief NVRAM Hardware Backend:
 *   LE_NVRAM_BACKEND_FLASH (0) : On-chip or QSPI Flash with runtime wear leveling.
 *   LE_NVRAM_BACKEND_FRAM  (1) : Byte-addressable SPI/I2C FRAM, MRAM, or EEPROM.
 *   LE_NVRAM_BACKEND_RAM   (2) : In-Memory RAM Mirror (transient simulation).
 */
#define LE_NVRAM_BACKEND_FLASH          0
#define LE_NVRAM_BACKEND_FRAM           1
#define LE_NVRAM_BACKEND_RAM            2

#ifndef LE_NVRAM_BACKEND
#define LE_NVRAM_BACKEND                LE_NVRAM_BACKEND_FLASH
#endif

#ifndef LE_NVRAM_TOTAL_BYTES
#define LE_NVRAM_TOTAL_BYTES            (16 * 1024) /**< Total logical NVRAM capacity (16 KB) */
#endif

#ifndef LE_NVRAM_BLOCK_SIZE
#define LE_NVRAM_BLOCK_SIZE             4096        /**< Physical erase sector size for Flash */
#endif

#ifndef LE_NVRAM_PAGE_SIZE
#define LE_NVRAM_PAGE_SIZE              256         /**< Flash programming page size */
#endif

#ifndef LE_NVRAM_REGS_BYTES
#define LE_NVRAM_REGS_BYTES             1024        /**< Retentive registers per slot */
#endif

#ifndef LE_NVRAM_SER_BYTES
#define LE_NVRAM_SER_BYTES              3072        /**< Sequence of Events circular log buffer */
#endif

#ifndef LE_NVRAM_EVENT_REC_BYTES
#define LE_NVRAM_EVENT_REC_BYTES        (LE_NVRAM_TOTAL_BYTES - (4096 + (LE_MAX_CONFIG_SLOTS * LE_NVRAM_REGS_BYTES) + LE_NVRAM_SER_BYTES))
#endif

#ifndef LE_NVRAM_CACHE_SIZE
#define LE_NVRAM_CACHE_SIZE             12288UL     /**< Working cache for manifest and retentive registers */
#endif

#ifndef LE_NVRAM_MAX_ERASE_CYCLES
#define LE_NVRAM_MAX_ERASE_CYCLES       100000UL    /**< Flash endurance rating for wear-leveling percentage */
#endif

/* ========================================================================== */
/* 5. Physical Hardware I/O Pin Capacities                                    */
/* ========================================================================== */

#ifndef LE_MAX_DIGITAL_IN
#define LE_MAX_DIGITAL_IN               64      /**< Max physical digital inputs (%DIN) */
#endif

#ifndef LE_MAX_DIGITAL_OUT
#define LE_MAX_DIGITAL_OUT              64      /**< Max physical digital outputs (%DOUT) */
#endif

#ifndef LE_MAX_ANALOG_IN
#define LE_MAX_ANALOG_IN                16      /**< Max physical analog inputs (ADC, %AIN) */
#endif

#ifndef LE_MAX_ANALOG_OUT
#define LE_MAX_ANALOG_OUT               16      /**< Max physical analog outputs (DAC/PWM, %AOUT) */
#endif

/* ========================================================================== */
/* 6. Subsystem Feature Switches & Modular Stripping                          */
/* ========================================================================== */

#ifndef LE_ENABLE_PROTECTION
#define LE_ENABLE_PROTECTION            1       /**< P&C relays (OVERCURRENT, DIST_21, DIFF_87, ANSI_81) */
#endif

#ifndef LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX               1       /**< Complex arithmetic, %C registers & conversions */
#endif

#ifndef LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG                1       /**< Analog I/O channels (%AIN, %AOUT) and scalers */
#endif

#ifndef LE_ENABLE_DSP
#define LE_ENABLE_DSP                   1       /**< DSP filter opcodes (LPF, BIQUAD, RMS, MEDIAN, etc.) */
#endif

#ifndef LE_ENABLE_TRANSCENDENTAL_MATH
#define LE_ENABLE_TRANSCENDENTAL_MATH   1       /**< Extended float math opcodes (sin, cos, tan, log, exp, pow) */
#endif

#ifndef LE_ENABLE_WORD_OPCODES
#define LE_ENABLE_WORD_OPCODES          1       /**< Enable 32-bit bitwise word opcodes (WORD_AND, WORD_OR, etc.) */
#endif

#ifndef LE_ENABLE_TIMING_MODEL
#define LE_ENABLE_TIMING_MODEL          1       /**< Enable program timing budget feasibility checks */
#endif

#ifndef LE_ENABLE_ALIASES
#define LE_ENABLE_ALIASES               1       /**< Enable symbolic alias resolution table */
#endif

#ifndef LE_ENABLE_MULTI_RATE
#define LE_ENABLE_MULTI_RATE            1       /**< Enable multi-rate execution prescalers */
#endif

#ifndef LE_ENABLE_SERIAL_BUS
#define LE_ENABLE_SERIAL_BUS            1       /**< I2C and SPI bus opcodes */
#endif

#ifndef LE_ENABLE_CRYPTO
#define LE_ENABLE_CRYPTO                1       /**< Ed25519 signature verification */
#endif

#ifndef LE_ENABLE_FPU
#if defined(__ARM_FP) || (defined(__VFP_FP__) && !defined(__SOFTFP__)) || defined(__riscv_flen) || defined(__FPU_PRESENT)
#define LE_ENABLE_FPU                   1       /**< Hardware FPU present and enabled */
#else
#define LE_ENABLE_FPU                   0
#endif
#endif

/* Prerequisite Dependency Cascades */
#if LE_ENABLE_PROTECTION
#undef  LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX               1       /**< Protection relays require complex phasors */
#endif

#if LE_ENABLE_COMPLEX || LE_ENABLE_DSP
#undef  LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG                1       /**< Complex phasors & DSP acquire from analog */
#endif

/* ========================================================================== */
/* 7. Math, Numerical Stability, & DSP Tuning                                 */
/* ========================================================================== */

#define LE_DIV_BY_ZERO_ZERO             0       /**< Output 0.0f on divide by zero (safe default) */
#define LE_DIV_BY_ZERO_INFINITY         1       /**< Output IEEE 754 +/- INFINITY */
#define LE_DIV_BY_ZERO_SATURATE         2       /**< Clamp to +/- FLT_MAX (saturating math) */

#ifndef LE_DIV_BY_ZERO_POLICY
#define LE_DIV_BY_ZERO_POLICY           LE_DIV_BY_ZERO_ZERO
#endif

#ifndef LE_FLUSH_NAN_TO_ZERO
#define LE_FLUSH_NAN_TO_ZERO            0       /**< 1 = Flush NaNs to 0.0f, 0 = IEEE 754 standard */
#endif

#if LE_ENABLE_PROTECTION
#ifndef LE_MAX_RAW_SAMPLES
#define LE_MAX_RAW_SAMPLES              64      /**< Raw sample buffer for high-rate phasor estimation */
#endif
#ifndef LE_MAX_SAMPLES_PER_CYCLE
#define LE_MAX_SAMPLES_PER_CYCLE        LE_MAX_RAW_SAMPLES
#endif
#endif

#if LE_ENABLE_DSP
#ifndef LE_MOVING_AVG_MAX_WINDOW
#define LE_MOVING_AVG_MAX_WINDOW        32      /**< Max sample window in moving-avg state */
#endif
#ifndef LE_RMS_MAX_WINDOW
#define LE_RMS_MAX_WINDOW               32      /**< Max sample window in True RMS state */
#endif
#ifndef LE_MAX_MEDIAN_WINDOW
#define LE_MAX_MEDIAN_WINDOW            9       /**< Max sample window in median filter state */
#endif
#ifndef LE_MAX_LUT_POINTS
#define LE_MAX_LUT_POINTS               16      /**< Max breakpoints in 1D lookup table state */
#endif
#endif

/* ========================================================================== */
/* 8. Timing Model, Scan Rates, & Communications                              */
/* ========================================================================== */

#ifndef LE_NS_PER_ABSTRACT_CYCLE
#define LE_NS_PER_ABSTRACT_CYCLE        600     /**< Calibrated execution nanoseconds per compiler cycle */
#endif

#ifndef LE_DEFAULT_SCAN_DT_SEC
#define LE_DEFAULT_SCAN_DT_SEC          0.01f   /**< Fallback dt (seconds) when no scan rate is specified */
#endif

#ifndef LE_MAX_ALIASES
#define LE_MAX_ALIASES                  32      /**< User + board declared register aliases */
#endif

#ifndef LE_MAX_PULSES
#define LE_MAX_PULSES                   8       /**< Max concurrent aliased pulses */
#endif

#ifndef LE_ALIAS_NAME_MAX
#define LE_ALIAS_NAME_MAX               7       /**< Max characters in an alias name */
#endif

#ifndef LE_COMMS_MAX_PAYLOAD
#define LE_COMMS_MAX_PAYLOAD            128     /**< Max packet payload bytes (UART / USB CDC) */
#endif

#define LE_COMMS_SYNC_BYTE              0xAA    /**< Packet framing sync byte */

#ifndef LE_COMMS_CLI_RX_CHUNK_SIZE
#define LE_COMMS_CLI_RX_CHUNK_SIZE      32      /**< Chunk size (bytes) for comms/CLI UART pump */
#endif

#ifndef LE_RINGBUF_DEFAULT_SIZE
#define LE_RINGBUF_DEFAULT_SIZE         2048    /**< Default capacity (bytes) for async ring buffers */
#endif

#ifdef __cplusplus
}
#endif

#endif /* LE_CONFIG_H */

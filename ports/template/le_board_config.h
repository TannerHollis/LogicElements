/**
 * @file le_board_config.h
 * @brief Board Configuration Template for LogicElements Runtime.
 *
 * Copy this file into your board's project include directory or define LE_CONFIG_FILE
 * to point to your custom board header.
 *
 * This file is the Single Source of Truth for:
 *   1. Processor core topology (single-core vs AMP dual-core) & execute-in-place (XIP).
 *   2. RAM partition strategy & workspace sizes.
 *   3. Configuration slots and storage backends.
 *   4. NVRAM, wear-leveling, and retentive registers.
 *   5. Physical I/O capacities.
 *   6. Subsystem feature switches (protection, complex, analog, DSP, serial bus).
 *   7. Math & numerical stability policies.
 *   8. Timing model & comms packet sizes.
 */

#ifndef LE_PORT_CONFIG_TEMPLATE_H
#define LE_PORT_CONFIG_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 1. Processor Core & Execution Topology                                     */
/* ========================================================================== */

/**
 * Core Processing Topology:
 *   LE_CORE_SINGLE (0) : Portable single-core execution (Core 0 handles I/O + VM).
 *   LE_CORE_AMP    (1) : Asymmetric Multiprocessing (Core 0 = I/O & comms; Core 1 = VM worker).
 *   LE_CORE_SMP    (2) : Symmetric Multiprocessing / RTOS multithreaded worker.
 */
#define LE_CORE_MODE                    LE_CORE_SINGLE

/** Inter-core scan completion watchdog timeout in microseconds (AMP mode). */
#define LE_AMP_WATCHDOG_TIMEOUT_US      50000UL

/**
 * Execution Mode:
 *   LE_EXEC_IN_PLACE (0) : Execute directly from Flash/ROM (zero-RAM bytecode footprint).
 *   LE_EXEC_IN_RAM   (1) : Shadow program into RAM for maximum execution speed.
 */
#define LE_EXEC_MODE                    LE_EXEC_IN_PLACE

/**
 * VM Instruction Dispatch Engine:
 *   LE_DISPATCH_AUTO   (0) : Threaded dispatch on GCC/Clang; switch-case on MSVC/others.
 *   LE_DISPATCH_SWITCH (1) : Standard ANSI C switch-case loop.
 *   LE_DISPATCH_DIRECT (2) : GCC/Clang computed gotos (labels as values).
 */
#define LE_DISPATCH_MODE                LE_DISPATCH_AUTO

/* ========================================================================== */
/* 2. Memory Hierarchy & RAM Partitioning                                     */
/* ========================================================================== */

/**
 * Total unified RAM workspace allocated for dynamic register arenas (%B, %I, %F, %C)
 * and state blocks (timers, counters, filters, edge triggers, user nodes).
 * Adjust based on available target RAM:
 *   - Constrained (e.g. AVR ATmega328P with 2 KB RAM) : 512 bytes
 *   - Mid-range   (e.g. STM32F4 / RP2040 with 256 KB) : 4096 - 16384 bytes
 *   - High-end    (e.g. STM32F7 / RP2350 with 512 KB) : 65536 - 262144 bytes
 */
#define LE_RAM_WORKSPACE_BYTES          2048

/**
 * Storage Partition Allocation Mode (RAM footprint for program slots):
 *   LE_STORAGE_PARTITION_ZERO_RAM     (0) : 0 bytes RAM. Slots read directly via Flash XIP.
 *   LE_STORAGE_PARTITION_SINGLE_CACHE (1) : 1 slot buffer in RAM (for phantom uploads/cache).
 *   LE_STORAGE_PARTITION_FULL_RAM     (2) : All slots held in RAM (default for desktop/sim).
 */
#define LE_STORAGE_PARTITION_MODE       LE_STORAGE_PARTITION_SINGLE_CACHE

/**
 * Program Storage Physical Backing:
 *   LE_STORAGE_FLASH (0) : On-chip or external SPI Flash (persistent).
 *   LE_STORAGE_RAM   (1) : In-memory RAM array (simulation / rapid bringup).
 */
#define LE_STORAGE_BACKEND              LE_STORAGE_RAM

/* ========================================================================== */
/* 3. Program Storage & Slots                                                 */
/* ========================================================================== */

/** Number of user configuration slots (0..LE_MAX_CONFIG_SLOTS - 1). */
#define LE_MAX_CONFIG_SLOTS             3

/** Maximum byte size per configuration slot. */
#define LE_SLOT_SIZE_BYTES              2048

/* ========================================================================== */
/* 4. Operational NVRAM & Flash Wear-Leveling                                 */
/* ========================================================================== */

/**
 * NVRAM Hardware Backend:
 *   LE_NVRAM_FLASH    (0) : Flash memory using runtime log-structured wear-leveling.
 *   LE_NVRAM_EXTERNAL (1) : Direct byte-addressable external NVRAM (SPI FRAM, MRAM).
 *   LE_NVRAM_RAM      (2) : RAM mirror (simulation / rapid bring-up).
 */
#define LE_NVRAM_BACKEND                LE_NVRAM_RAM

/** Total logical NVRAM capacity in bytes. */
#define LE_NVRAM_TOTAL_BYTES            16384UL

/** Physical sector erase block size in bytes (e.g. 4096 on NOR flash). */
#define LE_NVRAM_BLOCK_SIZE             4096UL

/** Write page size in bytes (e.g. 256 for NOR flash/SPI FRAM). */
#define LE_NVRAM_PAGE_SIZE              256UL

/**
 * Note: Retentive registers, SER transition log, and Event Recorder waveform
 * partitions are dynamically allocated per active circuit (circuit-driven NVRAM).
 * No static partition boundaries are imposed by the board.
 */

/** Working image cache for active retentive registers and manifest. */
#define LE_NVRAM_CACHE_SIZE             12288UL

/* ========================================================================== */
/* 5. Physical Hardware I/O Capacity                                          */
/* ========================================================================== */

#define LE_MAX_DIGITAL_IN               16
#define LE_MAX_DIGITAL_OUT              16
#define LE_MAX_ANALOG_IN                4
#define LE_MAX_ANALOG_OUT               4

/* ========================================================================== */
/* 6. Subsystem Feature Switches (Strip unused modules for code size/RAM)     */
/* ========================================================================== */

#define LE_ENABLE_PROTECTION            1   /* Power system protection elements */
#define LE_ENABLE_COMPLEX               1   /* Complex numbers & phasors (%C) */
#define LE_ENABLE_ANALOG                1   /* Analog & signal conditioning */
#define LE_ENABLE_DSP                   1   /* Fourier/DFT, harmonic filters */
#define LE_ENABLE_SERIAL_BUS            1   /* Hardware I2C / SPI transactions */
#define LE_ENABLE_CRYPTO                0   /* Ed25519/HMAC binary verification */
#define LE_ENABLE_FPU                   1   /* Hardware floating-point unit */

/* ========================================================================== */
/* 7. Math & Numerical Stability                                              */
/* ========================================================================== */

/**
 * Division by Zero Policy:
 *   LE_DIV_POLICY_SATURATE (0) : Return ±FLT_MAX / INT32_MAX.
 *   LE_DIV_POLICY_ZERO     (1) : Return 0.
 *   LE_DIV_POLICY_NAN      (2) : IEEE 754 standard (generates NaN / Inf).
 */
#define LE_DIV_BY_ZERO_POLICY           LE_DIV_POLICY_SATURATE

#define LE_FLUSH_NAN_TO_ZERO            1   /* Sanitize NaN/Inf inputs to 0.0 */
#define LE_MAX_RAW_SAMPLES              64  /* Max raw sample window for DFT */
#define LE_MOVING_AVG_MAX_WINDOW        128 /* Max window for moving average */
#define LE_RMS_MAX_WINDOW               128 /* Max window for True RMS */
#define LE_MAX_MEDIAN_WINDOW            31  /* Max window for median filter */
#define LE_MAX_LUT_POINTS               64  /* Max piecewise linear LUT points */

/* ========================================================================== */
/* 8. Timing Model & Communications                                           */
/* ========================================================================== */

/** Nanoseconds per compiler abstract execution cycle for cost model. */
#define LE_NS_PER_ABSTRACT_CYCLE        600

/** Default scan cycle delta-time in seconds (fallback if not circuit-owned). */
#define LE_DEFAULT_SCAN_DT_SEC          0.001f

/** Comms packet max payload size in bytes. */
#define LE_COMMS_MAX_PAYLOAD            128

/** Framing synchronization byte for UART/USB protocol. */
#define LE_COMMS_SYNC_BYTE              0xAA

/** RX chunk buffer size used when pumping incoming UART/USB bytes to comms/CLI. */
#define LE_COMMS_CLI_RX_CHUNK_SIZE      32

/** Optional default size for async UART/CDC TX and RX ring buffers (bytes). */
#define LE_RINGBUF_DEFAULT_SIZE         2048

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_CONFIG_TEMPLATE_H */

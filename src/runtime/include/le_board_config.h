/**
 * @file le_board_config.h
 * @brief Centralized board hardware capabilities, tuning parameters, and feature switches.
 *
 * This header serves as the SINGLE SOURCE OF TRUTH for all board-specific
 * compile-time configuration defines across LogicElements.
 *
 * Target boards can override any define here by:
 *   1. Passing `-DLE_BOARD_CONFIG_FILE="path/to/my_board_config.h"`
 *   2. Passing `-D<MACRO>=<VALUE>` compiler flags in CMake / Makefile
 *
 * NOTE ON REGISTERS:
 * Internal process image registers (%B boolean, %I integer, %F float, %C complex)
 * DO NOT have static compile-time limits. Register memory is packed dynamically
 * at load time inside LE_RAM_WORKSPACE_BYTES. The only constraint is that the
 * packed arena + preconfigured state image fits within LE_RAM_WORKSPACE_BYTES.
 */

#ifndef LE_BOARD_CONFIG_H
#define LE_BOARD_CONFIG_H

#ifdef LE_BOARD_CONFIG_FILE
#include LE_BOARD_CONFIG_FILE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Physical Hardware I/O Pin Capacities (Actual board terminal/pin limits)    */
/* ========================================================================== */

#ifndef LE_MAX_DIGITAL_IN
#define LE_MAX_DIGITAL_IN           64      /**< Max physical digital inputs (%DIN) */
#endif

#ifndef LE_MAX_DIGITAL_OUT
#define LE_MAX_DIGITAL_OUT          64      /**< Max physical digital outputs (%DOUT) */
#endif

#ifndef LE_MAX_ANALOG_IN
#define LE_MAX_ANALOG_IN            16      /**< Max physical analog inputs (ADC channels, %AIN) */
#endif

#ifndef LE_MAX_ANALOG_OUT
#define LE_MAX_ANALOG_OUT           16      /**< Max physical analog outputs (DAC/PWM channels, %AOUT) */
#endif

/* ========================================================================== */
/* Unified RAM Memory Architecture                                             */
/* ========================================================================== */

#ifndef LE_RAM_WORKSPACE_BYTES
/**
 * @brief Unified RAM pool for packed register arena and state blocks.
 *
 * Carved dynamically by le_loader at program load. Internal registers (%B, %I,
 * %F, %C) and state structs (timers, filters, relays) all share this pool.
 */
#define LE_RAM_WORKSPACE_BYTES      2048
#endif

/* ========================================================================== */
/* Optional Subsystem Capability Switches                                      */
/* ========================================================================== */

#ifndef LE_ENABLE_PROTECTION
#define LE_ENABLE_PROTECTION        1       /**< P&C relays (OVERCURRENT, PID, PHASOR, DIST_21, DIFF_87) */
#endif

#ifndef LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX           1       /**< Complex arithmetic, %C registers & conversions */
#endif

#ifndef LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG            1       /**< Analog I/O channels (%AIN, %AOUT) and scaling */
#endif

#ifndef LE_ENABLE_SERIAL_BUS
#define LE_ENABLE_SERIAL_BUS        1       /**< I2C and SPI bus opcodes */
#endif

#ifndef LE_ENABLE_DSP
#define LE_ENABLE_DSP               1       /**< DSP filter opcodes (LPF, BIQUAD, RMS, MEDIAN, etc.) */
#endif

/* ---- Prerequisite implication cascades ----------------------------------- */
#if LE_ENABLE_PROTECTION
#undef  LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX           1       /**< Protection relays require complex phasors */
#endif

#if LE_ENABLE_COMPLEX
#undef  LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG            1       /**< Complex phasors acquire from analog channels */
#endif

#if LE_ENABLE_DSP
#undef  LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG            1       /**< DSP filters sample from analog channels */
#endif

/* ========================================================================== */
/* Non-Volatile Program Configuration Slots (.lebin bytecode)                */
/* ========================================================================== */

#ifndef LE_MAX_CONFIG_SLOTS
#define LE_MAX_CONFIG_SLOTS         3       /**< Number of on-flash program bytecode slots */
#endif

#ifndef LE_SLOT_SIZE_BYTES
#define LE_SLOT_SIZE_BYTES          2048    /**< Maximum binary bytecode capacity per slot */
#endif

/* ========================================================================== */
/* Operational Non-Volatile Memory (NVRAM / Retentive / SER / Waveforms)     */
/* ========================================================================== */

#ifndef LE_NVRAM_TOTAL_BYTES
#define LE_NVRAM_TOTAL_BYTES        (16 * 1024) /**< Total logical NVRAM capacity (16 KB) */
#endif

#ifndef LE_NVRAM_BLOCK_SIZE
#define LE_NVRAM_BLOCK_SIZE         4096        /**< Physical erase sector/block size for Flash NVRAM (e.g. 4096) */
#endif

#ifndef LE_NVRAM_PAGE_SIZE
#define LE_NVRAM_PAGE_SIZE          256         /**< Write page size for Flash / EEPROM page programming (e.g. 32, 64, 128, 256) */
#endif

#ifndef LE_NVRAM_REGS_BYTES
#define LE_NVRAM_REGS_BYTES         1024        /**< Retentive register block size per config slot (1 KB per slot) */
#endif

#ifndef LE_NVRAM_SER_BYTES
#define LE_NVRAM_SER_BYTES          3072        /**< SER circular event log size (3 KB, device-wide) */
#endif

#ifndef LE_NVRAM_EVENT_REC_BYTES
#define LE_NVRAM_EVENT_REC_BYTES    (LE_NVRAM_TOTAL_BYTES - (64 + (LE_MAX_CONFIG_SLOTS * LE_NVRAM_REGS_BYTES) + LE_NVRAM_SER_BYTES)) /**< Event Recorder capture buffer size */
#endif

/* ========================================================================== */
/* Timing Model & Execution Budget                                             */
/* ========================================================================== */

#ifndef LE_NS_PER_ABSTRACT_CYCLE
/**
 * @brief Port-calibrated worst-case execution time per compiler abstract cycle.
 *
 * Used by the loader to verify that the declared scan_rate_hz fits inside
 * the fixed scan period.
 */
#define LE_NS_PER_ABSTRACT_CYCLE    600
#endif

/* ========================================================================== */
/* Subsystem Working Buffer Bounds                                            */
/* ========================================================================== */

#ifndef LE_MAX_ALIASES
#define LE_MAX_ALIASES              32      /**< User + board declared register aliases */
#endif

#if LE_ENABLE_PROTECTION
#ifndef LE_MAX_RAW_SAMPLES
#define LE_MAX_RAW_SAMPLES          64      /**< Raw sample buffer for high-rate acquisition */
#endif
#ifndef LE_MAX_SAMPLES_PER_CYCLE
#define LE_MAX_SAMPLES_PER_CYCLE    LE_MAX_RAW_SAMPLES
#endif
#endif

#if LE_ENABLE_DSP
#ifndef LE_MOVING_AVG_MAX_WINDOW
#define LE_MOVING_AVG_MAX_WINDOW    32      /**< Max sample window in moving-avg state */
#endif

#ifndef LE_RMS_MAX_WINDOW
#define LE_RMS_MAX_WINDOW           32      /**< Max sample window in True RMS state */
#endif

#ifndef LE_MAX_MEDIAN_WINDOW
#define LE_MAX_MEDIAN_WINDOW        9       /**< Max sample window in median filter state */
#endif

#ifndef LE_MAX_LUT_POINTS
#define LE_MAX_LUT_POINTS           16      /**< Max breakpoints in 1D lookup table state */
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* LE_BOARD_CONFIG_H */

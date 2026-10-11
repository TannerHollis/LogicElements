/**
 * @file le_port.h
 * @brief LogicElements Board Porting Interface (v3 / le_hal_t contract).
 *
 * A board port bridges the platform-independent runtime to physical hardware
 * by providing TWO things:
 *
 *   1. A complete `le_hal_t` function-pointer table (returned by
 *      `get_my_mcu_hal()`). The runtime, storage, comms and custom-node
 *      subsystems all dispatch through this table via `le_hal_set()`.
 *
 *   2. Thin scan-loop helpers (`le_port_init`, `le_port_read_inputs`,
 *      `le_port_write_outputs`, `le_port_deenergize_outputs`) that map the
 *      pin tables into the process image at the top/bottom of each PLC scan.
 *
 * Include this header and the matching `le_port.c` in your MCU project.
 */

/* ========================================================================== */
/* Board build tuning — must match your .leconfig                             */
/* --------------------------------------------------------------------------
 * Define these at compile time so the runtime matches the board profile the
 * compiler validates against (they override the defaults in le_types.h):
 *
 *   -DLE_RAM_WORKSPACE_BYTES=2048   unified RAM pool (packed register arena +
 *                                   state blocks) the loader carves at load
 *   -DLE_MAX_DIGITAL_IN=N -DLE_MAX_DIGITAL_OUT=N
 *   -DLE_MAX_ANALOG_IN=N  -DLE_MAX_ANALOG_OUT=N
 *   (Internal %B, %I, %F, %C registers are dynamically packed within workspace)
 *
 * Fixed-rate scan configuration (deterministic execution):
 *   The SCAN RATE IS CIRCUIT-OWNED: each circuit declares `scan_rate_hz` in its
 *   JSON and the compiler embeds it in the .lebin timing descriptor; the loader
 *   applies the period (1e6/rate us) to the VM clock and verifies achievability.
 *   The board only calibrates the COST MODEL used for that verification:
 *   -DLE_NS_PER_ABSTRACT_CYCLE=600  port-calibrated worst-case nanoseconds per
 *                                   compiler abstract cycle (the `.leconfig`
 *                                   `ns_per_abstract_cycle`). Programs whose
 *                                   estimated scan exceeds their declared period
 *                                   are rejected with LE_ERR_TIMING_BUDGET, so a
 *                                   designer lowers `scan_rate_hz` until it fits.
 *
 * Optionally disable subsystems you do not need (smaller RAM + opcode space):
 *   -DLE_ENABLE_PROTECTION=0 -DLE_ENABLE_COMPLEX=0 -DLE_ENABLE_ANALOG=0
 *   -DLE_ENABLE_SERIAL_BUS=0 -DLE_ENABLE_DSP=0
 *
 * Operational NVRAM configuration (matches your .leconfig):
 *   -DLE_NVRAM_TOTAL_BYTES=16384    total logical NVRAM capacity (default: 16 KB)
 *   -DLE_NVRAM_BLOCK_SIZE=4096      physical erase sector size for Flash (e.g. 4096)
 *   -DLE_NVRAM_PAGE_SIZE=256        programming page size for Flash / EEPROM (e.g. 256)
 *   -DLE_NVRAM_REGS_BYTES=1024      retentive register block size per config slot
 *   -DLE_NVRAM_SER_BYTES=3072       circular SER transition log buffer size
 *
 * NVRAM backend selection in le_port.c:
 *   -DLE_PORT_NVRAM_BACKEND=0       in-memory RAM mirror (rapid bring-up / simulation)
 *   -DLE_PORT_NVRAM_BACKEND=1       external SPI/I2C byte NVRAM / FRAM (MB85RS64, FM25V02)
 *   -DLE_PORT_NVRAM_BACKEND=2       plain on-chip / SPI NOR Flash with runtime wear leveling
 *
 * The reference ports ship with the exact flag set for their .leconfig;
 * see ports/<board>/le_port.h. A program is rejected at load when its register
 * arena + state image exceed LE_RAM_WORKSPACE_BYTES.
 * ========================================================================== */
#ifndef LE_PORT_H
#define LE_PORT_H

#include "le_types.h"
#include "le_hal.h"
#include "le_process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the board's complete Hardware Abstraction Layer table.
 *
 * The returned pointer is passed to `le_hal_set()` at startup so the runtime
 * can use GPIO, analog, time, UART, storage, serial-bus and custom-node HAL
 * callbacks.
 *
 * @return Pointer to the static `le_hal_t` table for this board.
 */
const le_hal_t* get_my_mcu_hal(void);

/**
 * @brief Initializes board GPIOs, timers, and communication peripherals.
 *
 * Configures all required microcontroller pins, hardware timers, and
 * communication interfaces prior to runtime execution. Usable directly, but
 * normally called through the HAL `init` callback inside `get_my_mcu_hal()`.
 */
void le_port_init(void);

/**
 * @brief Reads physical hardware pins and updates the Process Image inputs (%I).
 *
 * Called at the beginning of each logic scan cycle.
 *
 * @param img Pointer to the active process image table.
 */
void le_port_read_inputs(le_process_image_t* img);

/**
 * @brief Flushes Process Image outputs (%Q) to physical pins and relays.
 *
 * Called at the end of each logic scan cycle.
 *
 * @param img Pointer to the active process image table.
 */
void le_port_write_outputs(const le_process_image_t* img);

/**
 * @brief De-energizes all mapped outputs (safety interlock).
 *
 * Called when the VM is stopped or during a program upload so physical
 * outputs are never left energized.
 */
void le_port_deenergize_outputs(void);

/**
 * @brief Reads a byte buffer from Non-Volatile RAM (NV registers, SER, Event Recorder).
 *
 * @param offset Logical byte offset in NVRAM partition space.
 * @param buf Destination buffer pointer.
 * @param len Number of bytes to read.
 * @return True on success; false on failure or out-of-bounds.
 */
bool le_port_nvram_read(uint32_t offset, void* buf, size_t len);

/**
 * @brief Writes a byte buffer to Non-Volatile RAM (NV registers, SER, Event Recorder).
 *
 * If backed by external SPI FRAM / EEPROM / battery SRAM, writes are directly committed.
 * If backed by raw on-chip Flash, uses the runtime wear-leveling engine to protect flash sectors.
 *
 * @param offset Logical byte offset in NVRAM partition space.
 * @param buf Source data buffer pointer.
 * @param len Number of bytes to write.
 * @return True on success; false on failure or out-of-bounds.
 */
bool le_port_nvram_write(uint32_t offset, const void* buf, size_t len);

/**
 * @brief Erases a region of Non-Volatile RAM, or formats the entire NVRAM partition.
 *
 * If offset is 0 and len >= LE_NVRAM_TOTAL_BYTES, this triggers a full format/wipe.
 *
 * @param offset Logical byte offset in NVRAM partition space.
 * @param len Number of bytes to erase.
 * @return True on success; false on failure or out-of-bounds.
 */
bool le_port_nvram_erase(uint32_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_H */

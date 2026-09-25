/**
 * @file le_port.h
 * @brief LogicElements Board Porting Interface (v3 / le_hal_t contract) -
 *        Raspberry Pi Pico 2 W (RP2350).
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
 * Include this header and the matching `le_port_rp2350.c` in your Pico project
 * (see the companion LogicElements-Pico2W app, or any pico-sdk CMake target).
 */

/* ========================================================================== */
/* Board build tuning - matches ports/rp2350/rp2350_pico2_w.leconfig          */
/* --------------------------------------------------------------------------
 * The reference Pico 2 W firmware passes these as target compile definitions
 * (see LogicElements-Pico2W/CMakeLists.txt). They override the runtime defaults
 * in le_types.h / le_storage.h so the loader, storage and cost model agree with
 * the board profile the compiler validates against:
 *
 *   -DLE_RAM_WORKSPACE_BYTES=262144          RP2350 has 520 KB SRAM; the profile
 *                                           declares a 256 KB unified workspace
 *   -DLE_MAX_DIGITAL_IN=24 -DLE_MAX_DIGITAL_OUT=24 -DLE_MAX_BOOL_REGS=256
 *   -DLE_MAX_FLOATS=128    -DLE_MAX_INT_REGS=128 -DLE_MAX_ANALOG_IN=4
 *   -DLE_MAX_CONFIG_SLOTS=2 -DLE_SLOT_SIZE_BYTES=4096
 *   -DLE_NS_PER_ABSTRACT_CYCLE=250          (RP2350 Cortex-M33 @ 150 MHz cost
 *                                           model - the board only calibrates
 *                                           this estimate; the scan rate is
 *                                           circuit-owned via `scan_rate_hz`)
 * feature switches (protection/complex/analog/dsp/serial_bus) = defaults (1).
 *
 * Optional onboard-LED through the wireless (CYW43) driver: define
 *   -DLE_PORT_CYW43_LED=1
 * to map %Q0 to the Pico 2 W onboard LED (CYW43 WL_GPIO0) instead of an
 * external GPIO; this pulls in the cyw43 arch driver (no Wi-Fi stack needed).
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
 * @brief Initializes board GPIOs, timers, ADC and communication peripherals.
 *
 * Configures all required microcontroller pins, hardware timers, and
 * communication interfaces prior to runtime execution. Usable directly, but
 * normally called through the HAL `init` callback inside `get_my_mcu_hal()`.
 */
void le_port_init(void);

/**
 * @brief Reads physical hardware pins and updates the Process Image inputs (%%I).
 *
 * Called at the beginning of each logic scan cycle.
 *
 * @param img Pointer to the active process image table.
 */
void le_port_read_inputs(le_process_image_t* img);

/**
 * @brief Flushes Process Image outputs (%%Q) to physical pins and relays.
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
 * @brief Background flush of the async USB-CDC TX ring buffer.
 *
 * Call this regularly (e.g. every PLC scan) so buffered comms/CLI output is
 * pushed to the USB CDC FIFO without writers blocking on USB throughput.
 */
void le_port_cdc_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_H */



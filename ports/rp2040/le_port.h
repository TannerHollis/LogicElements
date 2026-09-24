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
/* Board build tuning — matches ports/rp2040/rp2040_pico.leconfig             */
/* --------------------------------------------------------------------------
 * -DLE_RAM_WORKSPACE_BYTES=4096           (Pico has 264 KB RAM; profile
 *                                         declares a 4096-byte workspace; the
 *                                         runtime default is 2048 — RAISE it)
 * -DLE_MAX_DIGITAL_IN=16 -DLE_MAX_DIGITAL_OUT=16  -DLE_MAX_BOOL_REGS=128
 * -DLE_MAX_FLOATS=64     -DLE_MAX_INT_REGS=64     -DLE_MAX_ANALOG_IN=3
 * -DLE_NS_PER_ABSTRACT_CYCLE=400    (profile ns_per_abstract_cycle — RP2040
 *                                    Cortex-M0+ @ 133 MHz ~0.4 us per cycle; the
 *                                    board only calibrates this COST MODEL — the
 *                                    scan rate is circuit-owned via `scan_rate_hz`)
 * feature switches (protection/complex/analog/dsp/serial_bus) = defaults (1).
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

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_H */

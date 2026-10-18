/**
 * @file le_port.h
 * @brief LogicElements Board Porting Interface (v3 / le_hal_t contract).
 *
 * A board port bridges the platform-independent runtime to physical hardware
 * by providing TWO things:
 *   1. A complete Hardware Abstraction Layer table (`get_my_mcu_hal()`).
 *   2. High-level scan-loop helpers (`le_port_init`, `le_port_read_inputs`,
 *      `le_port_write_outputs`, `le_port_deenergize_outputs`).
 *
 * All hardware tuning, workspace sizing, and peripheral counts are configured
 * in `include/le_board_config.h`.
 */

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
 * Pass this pointer to `le_hal_set()` at startup to bind all low-level hardware
 * callbacks (GPIO, ADC, UART, Storage, NVRAM, Timers) to the runtime engine.
 *
 * @return Pointer to the static `le_hal_t` table for this board.
 */
const le_hal_t* get_my_mcu_hal(void);

/**
 * @brief Initializes board clocks, GPIO modes, timers, and communication buses.
 */
void le_port_init(void);

/**
 * @brief Reads physical hardware pins and updates Process Image inputs (%I, %AIN).
 *
 * @param[in,out] img Pointer to the active process image table.
 */
void le_port_read_inputs(le_process_image_t* img);

/**
 * @brief Flushes Process Image outputs (%Q, %AOUT) to physical actuators and relays.
 *
 * @param[in] img Pointer to the active process image table.
 */
void le_port_write_outputs(const le_process_image_t* img);

/**
 * @brief De-energizes all mapped outputs to safe inactive states (safety interlock).
 */
void le_port_deenergize_outputs(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_H */

/**
 * @file le_port.h
 * @brief LogicElements Board Porting Interface (v3 / le_hal_t contract) - RP2040 / Pico.
 *
 * All hardware tuning, workspace sizing, and peripheral counts are configured
 * in `ports/rp2040/le_board_config.h` matching `rp2040_pico.leconfig`.
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
 * @return Pointer to the static `le_hal_t` table for RP2040.
 */
const le_hal_t* get_my_mcu_hal(void);

/**
 * @brief Initializes board GPIOs, timers, ADC, and communication peripherals.
 */
void le_port_init(void);

/**
 * @brief Reads physical hardware pins and updates Process Image inputs (%I, %AIN).
 *
 * @param[in,out] img Pointer to the active process image table.
 */
void le_port_read_inputs(le_process_image_t* img);

/**
 * @brief Flushes Process Image outputs (%Q, %AOUT) to physical pins and relays.
 *
 * @param[in] img Pointer to the active process image table.
 */
void le_port_write_outputs(const le_process_image_t* img);

/**
 * @brief De-energizes all mapped outputs (safety interlock).
 */
void le_port_deenergize_outputs(void);

#ifdef __cplusplus
}
#endif

#endif /* LE_PORT_H */

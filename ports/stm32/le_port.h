/**
 * @file le_port.h
 * @brief LogicElements Board Porting Interface (v3 / le_hal_t contract) - STM32F401.
 *
 * All hardware tuning, workspace sizing, and peripheral counts are configured
 * in `ports/stm32/le_board_config.h` matching `stm32f401.leconfig`.
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
 * @return Pointer to the static `le_hal_t` table for STM32.
 */
const le_hal_t* get_my_mcu_hal(void);

/**
 * @brief Initializes board clocks, GPIOs, timers, ADC, and communication peripherals.
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

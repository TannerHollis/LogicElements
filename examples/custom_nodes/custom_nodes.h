/**
 * @file custom_nodes.h
 * @brief Header declarations for custom board peripheral drivers and functions.
 *
 * This header is automatically included when the LogicElements compiler
 * exports a standalone embedded C header (le_compile -c output.h).
 */

#ifndef CUSTOM_NODES_H
#define CUSTOM_NODES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the hardware timer PWM duty cycle.
 * @param duty_percent Duty cycle percentage between 0.0% and 100.0%.
 * @param enable Set to true to energize output; false to de-energize.
 * @return True if output is actively driven; false otherwise.
 */
bool hardware_pwm_set(float duty_percent, bool enable);

/**
 * @brief Reads the hardware quadrature encoder counter.
 * @param reset Set to true to reset counter value to zero.
 * @return Current 32-bit encoder position.
 */
int32_t hardware_encoder_read(bool reset);

/**
 * @brief Applies single-pole low-pass filtering to an analog sample.
 * @param raw_input Instantaneous input sample value.
 * @return Smoothed output value.
 */
float hardware_filter_apply(float raw_input);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_NODES_H */

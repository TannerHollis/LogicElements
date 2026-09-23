/**
 * @file le_process_image.h
 * @brief Process Image table and memory accessors for LogicElements C Runtime.
 */

#ifndef LE_PROCESS_IMAGE_H
#define LE_PROCESS_IMAGE_H

#include "le_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t                din[(LE_MAX_DIGITAL_IN + 7) / 8];
    uint8_t                dout[(LE_MAX_DIGITAL_OUT + 7) / 8];
    uint8_t                bool_regs[(LE_MAX_BOOL_REGS + 7) / 8];
    float                  floats[LE_MAX_FLOATS];
    int32_t                int_regs[LE_MAX_INT_REGS];
    int32_t                ain_raw[LE_MAX_ANALOG_IN];
    float                  ain[LE_MAX_ANALOG_IN];
    le_scale_state_t       scalers[LE_MAX_SCALERS];
    le_timer_state_t       timers[LE_MAX_TIMERS];
    le_counter_state_t     counters[LE_MAX_COUNTERS];
    le_pid_state_t         pids[LE_MAX_PID];
    le_overcurrent_state_t overcurrents[LE_MAX_OVERCURRENT];
#if LE_ENABLE_PROTECTION
    le_phasor_state_t      phasors[LE_MAX_PHASORS];
    le_symcomp_state_t     symcomps[LE_MAX_PHASORS / 3 + 1];
    le_diff87_state_t      diff87s[LE_MAX_DIFF87];
    le_dist21_state_t      dist21s[LE_MAX_DIST21];
#endif
#if LE_ENABLE_SERIAL_BUS
    le_i2c_device_state_t  i2c_devices[LE_MAX_I2C_DEVICES];
    le_spi_device_state_t  spi_devices[LE_MAX_SPI_DEVICES];
#endif
#if LE_ENABLE_DSP
    le_lpf_state_t         lpfs[LE_MAX_DSP_FILTERS];
    le_biquad_state_t      biquads[LE_MAX_DSP_FILTERS];
    le_moving_avg_state_t  moving_avgs[LE_MAX_DSP_FILTERS];
    le_rate_limiter_state_t rate_limiters[LE_MAX_DSP_FILTERS];
    le_deadband_state_t    deadbands[LE_MAX_DSP_FILTERS];
    le_washout_state_t     washouts[LE_MAX_DSP_FILTERS];
    le_peak_state_t        peaks[LE_MAX_DSP_FILTERS];
    le_rms_state_t         rms_meters[LE_MAX_DSP_FILTERS];
    le_median_state_t      medians[LE_MAX_DSP_FILTERS];
    le_derivative_state_t  derivatives[LE_MAX_DSP_FILTERS];
    le_zero_crossing_state_t zero_crossings[LE_MAX_DSP_FILTERS];
    le_lut_1d_state_t      luts[LE_MAX_DSP_FILTERS];
    le_totalizer_state_t   totalizers[LE_MAX_DSP_FILTERS];
    le_min_max_hold_state_t min_max_holds[LE_MAX_DSP_FILTERS];
#endif
} le_process_image_t;

/**
 * @brief Initializes the process image table, clearing all bits, registers, and block states.
 *
 * @param img Pointer to the process image structure to initialize.
 */
void le_process_image_init(le_process_image_t* img);

/**
 * @brief Reads a boolean value from the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and bit offset.
 * @return Returns the boolean state at the specified address; returns `false` if invalid.
 */
bool le_process_image_get_bool(const le_process_image_t* img, uint16_t addr);

/**
 * @brief Writes a boolean value to the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and bit offset.
 * @param val Boolean value to write.
 */
void le_process_image_set_bool(le_process_image_t* img, uint16_t addr, bool val);

/**
 * @brief Reads a 32-bit float value from the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and float index.
 * @return Returns the float value at the specified address; returns `0.0f` if invalid.
 */
float le_process_image_get_float(const le_process_image_t* img, uint16_t addr);

/**
 * @brief Writes a 32-bit float value to the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and float index.
 * @param val Float value to write.
 */
void le_process_image_set_float(le_process_image_t* img, uint16_t addr, float val);

/**
 * @brief Reads a 32-bit signed integer value from the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and integer index.
 * @return Returns the integer value at the specified address; returns `0` if invalid.
 */
int32_t le_process_image_get_int(const le_process_image_t* img, uint16_t addr);

/**
 * @brief Writes a 32-bit signed integer value to the specified 16-bit process image address.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit address indicating memory region and integer index.
 * @param val Integer value to write.
 */
void le_process_image_set_int(le_process_image_t* img, uint16_t addr, int32_t val);

/**
 * @brief Configures parameters for a linear scaling function block.
 *
 * @param img Pointer to the process image structure.
 * @param idx Zero-based scaler block index (`0` to `LE_MAX_SCALERS - 1`).
 * @param raw_min Minimum expected raw input value.
 * @param raw_max Maximum expected raw input value.
 * @param scale_min Scaled engineering unit minimum.
 * @param scale_max Scaled engineering unit maximum.
 * @param clamp Clamps the output between `scale_min` and `scale_max` when `true`.
 */
void le_process_image_set_scaler(le_process_image_t* img, uint8_t idx,
                                 float raw_min, float raw_max,
                                 float scale_min, float scale_max,
                                 bool clamp);

#if LE_ENABLE_SERIAL_BUS
/**
 * @brief Configures parameters for an external I2C peripheral block.
 *
 * @param img Pointer to the process image structure.
 * @param idx Zero-based device index (`0` to `LE_MAX_I2C_DEVICES - 1`).
 * @param addr_7bit 7-bit slave hardware address on the I2C bus.
 * @param startup_data Pointer to startup command bytes (or `NULL` if unused).
 * @param startup_len Number of startup configuration bytes (`0` to `16`).
 * @param poll_rate_ms Periodic poll interval in milliseconds (`0` for edge-triggered polling only).
 * @param poll_tx_data Pointer to command or register address bytes transmitted during polling.
 * @param poll_tx_len Number of bytes to transmit during poll.
 * @param poll_rx_len Number of bytes to read into the receive buffer.
 * @param data_dest_addr Destination address in the process image (%R or %M), or `LE_ADDR_UNUSED`.
 */
void le_i2c_device_config(le_process_image_t* img, uint8_t idx, uint8_t addr_7bit,
                          const uint8_t* startup_data, uint8_t startup_len,
                          uint32_t poll_rate_ms,
                          const uint8_t* poll_tx_data, uint8_t poll_tx_len,
                          uint8_t poll_rx_len, uint16_t data_dest_addr);

/**
 * @brief Configures parameters for an external SPI peripheral block.
 *
 * @param img Pointer to the process image structure.
 * @param idx Zero-based device index (`0` to `LE_MAX_SPI_DEVICES - 1`).
 * @param cs_pin Chip select GPIO pin identifier.
 * @param startup_data Pointer to startup command bytes (or `NULL` if unused).
 * @param startup_len Number of startup configuration bytes (`0` to `16`).
 * @param poll_rate_ms Periodic poll interval in milliseconds (`0` for edge-triggered polling only).
 * @param poll_tx_data Pointer to command or register address bytes transmitted during polling.
 * @param poll_len Total number of bytes to transfer across MOSI and MISO.
 * @param data_dest_addr Destination address in the process image (%R or %M), or `LE_ADDR_UNUSED`.
 */
void le_spi_device_config(le_process_image_t* img, uint8_t idx, uint8_t cs_pin,
                          const uint8_t* startup_data, uint8_t startup_len,
                          uint32_t poll_rate_ms,
                          const uint8_t* poll_tx_data, uint8_t poll_len,
                          uint16_t data_dest_addr);
#endif

#if LE_ENABLE_DSP
/**
 * @brief Configures single-pole low-pass filter parameters.
 */
void le_process_image_set_lpf(le_process_image_t* img, uint8_t idx, float alpha);

/**
 * @brief Configures 2nd-order Direct Form II Biquad filter coefficients.
 */
void le_process_image_set_biquad(le_process_image_t* img, uint8_t idx,
                                 float b0, float b1, float b2, float a1, float a2);

/**
 * @brief Configures moving average filter window length.
 */
void le_process_image_set_moving_avg(le_process_image_t* img, uint8_t idx, uint16_t window_size);

/**
 * @brief Configures slew rate limiter rising and falling limits.
 */
void le_process_image_set_rate_limiter(le_process_image_t* img, uint8_t idx,
                                       float rising_rate, float falling_rate);

/**
 * @brief Configures deadband threshold and center offset.
 */
void le_process_image_set_deadband(le_process_image_t* img, uint8_t idx,
                                   float threshold, float center);

/**
 * @brief Configures high-pass washout filter coefficient.
 */
void le_process_image_set_washout(le_process_image_t* img, uint8_t idx, float alpha);

/**
 * @brief Configures peak/envelope follower decay rate.
 */
void le_process_image_set_peak(le_process_image_t* img, uint8_t idx, float decay_rate);

/**
 * @brief Configures True RMS meter window length.
 */
void le_process_image_set_rms(le_process_image_t* img, uint8_t idx, uint16_t window_size);

/**
 * @brief Configures median filter window length.
 */
void le_process_image_set_median(le_process_image_t* img, uint8_t idx, uint16_t window_size);

/**
 * @brief Configures filtered derivative parameters.
 */
void le_process_image_set_derivative(le_process_image_t* img, uint8_t idx, float alpha, float gain);

/**
 * @brief Configures zero-crossing detector and frequency counter.
 */
void le_process_image_set_zero_crossing(le_process_image_t* img, uint8_t idx, float hysteresis, float sample_rate_hz);

/**
 * @brief Configures 1D lookup table breakpoints.
 */
void le_process_image_set_lut_1d(le_process_image_t* img, uint8_t idx, const float* x, const float* y, uint16_t num_points);

/**
 * @brief Configures totalizer / numerical integrator parameters.
 */
void le_process_image_set_totalizer(le_process_image_t* img, uint8_t idx,
                                    float time_base_sec, float scale_factor,
                                    float sample_time_sec, float max_limit);

/**
 * @brief Configures min/max peak hold output mode.
 */
void le_process_image_set_min_max_hold(le_process_image_t* img, uint8_t idx, uint8_t mode);
#endif

/**
 * @brief Resolves the active state for a timer instance.
 * When the running program bound timers to the zero-heap arena (loader), the
 * live state lives in the heap; otherwise it falls back to the fixed
 * @ref le_process_image_t::timers array (manual instructions / legacy).
 * @return Pointer to the timer state, or NULL if @p idx is out of range.
 */
le_timer_state_t* le_process_image_timer(const le_process_image_t* img, uint16_t idx);

/**
 * @brief Resolves the active state for a counter instance (heap-bound or
 * fixed-array fallback), mirroring @ref le_process_image_timer.
 * @return Pointer to the counter state, or NULL if @p idx is out of range.
 */
le_counter_state_t* le_process_image_counter(const le_process_image_t* img, uint16_t idx);

/**
 * @brief Generic resolver: live state pointer for a stateful kind instance.
 * Returns the heap-bound slice if the running program bound @p kind, otherwise
 * a pointer into the fixed-array fallback for that kind. NULL when @p idx is
 * out of range for the bound rows or the fixed array.
 */
uint8_t* le_process_image_kind_state(const le_process_image_t* img, uint8_t kind, uint16_t idx);

/**
 * @brief Applies per-kind factory defaults to a freshly bound state block so
 * heap-bound elements behave like their fixed-array counterparts.
 */
void le_rt_apply_defaults(uint8_t kind, uint8_t* state);

/**
 * @brief Applies one per-element config directive to a state block (heap-bound
 * or fixed-array fallback). See the kind/slot mapping in le_rt_configure.
 */
void le_rt_configure(const le_process_image_t* img, uint8_t kind, uint8_t idx, uint8_t slot, float value);

#ifdef __cplusplus
}
#endif

#endif /* LE_PROCESS_IMAGE_H */

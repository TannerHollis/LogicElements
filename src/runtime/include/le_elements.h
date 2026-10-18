/**
 * @file le_elements.h
 * @brief Element block state structures for LogicElements runtime elements.
 *
 * All state structs are 1-byte packed (#pragma pack(push, 1)) so that their
 * binary memory representation matches across compilers and matches the preconfigured
 * state image in the compiled .lebin.
 */

#ifndef LE_ELEMENTS_H
#define LE_ELEMENTS_H

#include "le_config.h"
#include "le_complex.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

/**
 * @brief Runtime state tracking for timer blocks (TON, TOF, TP).
 */
typedef struct {
    uint32_t start_time_ms; /**< Timestamp in milliseconds when the timing interval began. */
    uint32_t preset_ms;     /**< Preset duration in milliseconds. */
    uint32_t elapsed_ms;    /**< Current elapsed time in milliseconds. */
    bool     prev_in;       /**< Previous cycle input state for edge detection. */
    bool     q;             /**< Current output state. */
} le_timer_state_t;

/**
 * @brief Runtime state tracking for counter blocks (CTU, CTD, CTUD).
 */
typedef struct {
    int32_t  count;         /**< Current accumulated count value. */
    int32_t  preset;        /**< Target preset count value. */
    bool     prev_cu;       /**< Previous cycle count-up input state. */
    bool     prev_cd;       /**< Previous cycle count-down input state. */
    bool     qu;            /**< Count-up threshold reached flag. */
    bool     qd;            /**< Count-down zero reached flag. */
} le_counter_state_t;

/**
 * @brief Closed-loop PID controller state and tuning parameters.
 */
typedef struct {
    float    kp;            /**< Proportional gain coefficient. */
    float    ki;            /**< Integral gain coefficient. */
    float    kd;            /**< Derivative gain coefficient. */
    float    integral;      /**< Accumulated integral error sum. */
    float    prev_error;    /**< Error value from previous cycle for derivative computation. */
    float    out_min;       /**< Minimum saturation clamp limit for output. */
    float    out_max;       /**< Maximum saturation clamp limit for output. */
    uint32_t last_time_ms;  /**< Timestamp of previous evaluation cycle in milliseconds. */
} le_pid_state_t;

/**
 * @brief Linear analog signal scaling and clamping parameters.
 */
typedef struct {
    float raw_min;          /**< Minimum expected raw input value. */
    float raw_max;          /**< Maximum expected raw input value. */
    float scale_min;        /**< Scaled engineering unit minimum. */
    float scale_max;        /**< Scaled engineering unit maximum. */
    bool  clamp;            /**< Whether to clamp the result within [scale_min, scale_max]. */
} le_scale_state_t;

/**
 * @brief IEC/IEEE inverse-time overcurrent curve types (ANSI 51).
 */
typedef enum {
    LE_CURVE_IEC_NORMAL      = 0,  /**< IEC Normal Inverse: A=0.14, B=0.0, p=0.02 */
    LE_CURVE_IEC_VERY        = 1,  /**< IEC Very Inverse: A=13.5, B=0.0, p=1.0 */
    LE_CURVE_IEC_EXTREME     = 2,  /**< IEC Extremely Inverse: A=80.0, B=0.0, p=2.0 */
    LE_CURVE_IEEE_MODERATELY = 3,  /**< IEEE Moderately Inverse: A=0.0515, B=0.1140, p=0.02 */
    LE_CURVE_IEEE_VERY       = 4,  /**< IEEE Very Inverse: A=19.61, B=0.491, p=2.0 */
    LE_CURVE_IEEE_EXTREME    = 5,  /**< IEEE Extremely Inverse: A=28.2, B=0.1217, p=2.0 */
    LE_CURVE_IEEE_SHORT      = 6,  /**< IEEE Short Time Inverse: A=0.00342, B=0.00262, p=0.02 */
    LE_CURVE_IEEE_LONG       = 7,  /**< IEEE Long Time Inverse: A=26.13, B=0.349, p=2.0 */
    LE_CURVE_CUSTOM          = 8,  /**< User-supplied curve (a_coeff/b_coeff/p_coeff baked by compiler) */
} le_overcurrent_curve_t;

/**
 * @brief Inverse-time overcurrent protection element state (ANSI 51).
 */
typedef struct {
    float    pickup;        /**< Pickup current threshold in per-unit or amperes. */
    float    time_dial;     /**< Time dial multiplier (TMS). */
    uint8_t  curve_type;    /**< IEC/IEEE curve characteristics selection (le_overcurrent_curve_t). */
    float    accumulator;   /**< Trip integration accumulator state. */
    bool     tripped;       /**< Tripped condition flag. */
    float    a_coeff;       /**< Curve coefficient A (IEEE C37.112 / IEC 60255). */
    float    b_coeff;       /**< Curve coefficient B (IEEE C37.112 / IEC 60255). */
    float    p_coeff;       /**< Curve coefficient p (IEEE C37.112 / IEC 60255). */
} le_overcurrent_state_t;

#if LE_ENABLE_PROTECTION

/**
 * @brief Single-phase fundamental phasor extraction filter state.
 */
typedef struct {
    uint16_t     samples_per_cycle;                  /**< Default samples per cycle (config) */
    bool         self_sync;                          /**< true=0-degree reference, false=sync-referenced */
    le_complex_t phasor;                             /**< Filtered fundamental complex phasor */
    float        magnitude;                          /**< RMS magnitude of extracted phasor */
    float        angle_rad;                          /**< Phase angle in radians (-pi to +pi) */
    uint16_t     raw_write_idx;                      /**< Raw sample buffer write pointer */
    float        raw_samples[LE_MAX_RAW_SAMPLES];    /**< High-rate raw sample buffer */
} le_phasor_state_t;

/**
 * @brief Three-phase fundamental phasor extraction filter state (PHASOR_3P).
 */
typedef struct {
    uint16_t     samples_per_cycle;    /**< Default samples per cycle (config) */
    bool         self_sync;            /**< true=0-degree reference, false=sync-referenced */

    /* Phase A extractor state */
    float        raw_samples_a[LE_MAX_RAW_SAMPLES];  /**< Phase A high-rate raw samples */
    uint16_t     raw_write_idx_a;                    /**< Phase A raw buffer write pointer */
    le_complex_t phasor_a;                           /**< Phase A fundamental complex phasor */
    float        magnitude_a;                        /**< Phase A RMS magnitude of extracted phasor */
    float        angle_rad_a;                        /**< Phase A phase angle in radians (-pi to +pi) */

    /* Phase B extractor state */
    float        raw_samples_b[LE_MAX_RAW_SAMPLES];  /**< Phase B high-rate raw samples */
    uint16_t     raw_write_idx_b;                    /**< Phase B raw buffer write pointer */
    le_complex_t phasor_b;                           /**< Phase B fundamental complex phasor */
    float        magnitude_b;                        /**< Phase B RMS magnitude of extracted phasor */
    float        angle_rad_b;                        /**< Phase B phase angle in radians (-pi to +pi) */

    /* Phase C extractor state */
    float        raw_samples_c[LE_MAX_RAW_SAMPLES];  /**< Phase C high-rate raw samples */
    uint16_t     raw_write_idx_c;                    /**< Phase C raw buffer write pointer */
    le_complex_t phasor_c;                           /**< Phase C fundamental complex phasor */
    float        magnitude_c;                        /**< Phase C RMS magnitude of extracted phasor */
    float        angle_rad_c;                        /**< Phase C phase angle in radians (-pi to +pi) */
} le_phasor3_state_t;

/**
 * @brief Dynamic frequency tracking and estimation state (ANSI 81).
 */
typedef struct {
    float    nominal_freq_hz;     /**< Nominal center frequency (e.g. 60.0 or 50.0 Hz). */
    float    hysteresis;          /**< Noise deadband threshold around zero. */
    float    min_freq_hz;         /**< Lower validity bound (below this, `valid` is cleared). */
    float    max_freq_hz;         /**< Upper validity bound (above this, `valid` is cleared). */
    float    filter_alpha;        /**< Output smoothing filter factor (0.0 to 1.0). */

    /* Runtime tracking state */
    float    prev_sample;         /**< Previous input sample for edge & slope interpolation. */
    float    prev_cross_frac;     /**< Fractional sub-sample offset of previous crossing. */
    float    tracked_freq_hz;     /**< Current (filtered) tracked frequency output. */
    uint32_t samples_since_cross; /**< Sample ticks since last valid crossing. */
    int8_t   armed_state;         /**< Schmitt-trigger arming state (-1 = neg arm, +1 = pos arm). */
    bool     valid;               /**< True if currently locked and tracking within [min,max]. */
    bool     initialized;         /**< True once the first (reference) crossing is recorded. */
} le_freq_est_state_t;

/**
 * @brief Three-phase symmetrical components decomposition state.
 */
typedef struct {
    le_complex_t phase_a;   /**< Phase A input phasor. */
    le_complex_t phase_b;   /**< Phase B input phasor. */
    le_complex_t phase_c;   /**< Phase C input phasor. */
    le_complex_t seq_0;     /**< Zero sequence component (3*I0). */
    le_complex_t seq_1;     /**< Positive sequence component (I1). */
    le_complex_t seq_2;     /**< Negative sequence component (I2). */
} le_symcomp_state_t;

/**
 * @brief Mho circle distance relay protection zone state (ANSI 21).
 */
typedef struct {
    float  reach_ohms;           /**< Zone reach impedance magnitude (ohms). */
    float  line_angle_deg;       /**< Line/impedance characteristic angle (degrees). */
    float  offset_mag;           /**< Mho-offset compensation magnitude (ohms). */
    float  offset_angle_deg;     /**< Mho-offset phasor angle (degrees). */
    float  prefault_v_threshold; /**< If live |V| falls below this, treat as fault and use prefault V. */
    uint32_t prefault_duration_ms; /**< How long to keep using prefault V after arming (ms). */
    le_complex_t prefault_v;     /**< Remembered pre-fault (known-good) voltage phasor. */
    uint32_t prefault_arm_ms;    /**< Timestamp when prefault substitution was armed. */
    bool   prefault_armed;       /**< True between arming and duration expiry. */
    float  r_meas;               /**< Measured apparent resistance (R). */
    float  x_meas;               /**< Measured apparent reactance (X). */
    bool   tripped;              /**< Distance zone trip flag. */
    float  r_center_norm;        /**< Cached normal center R. */
    float  x_center_norm;        /**< Cached normal center X. */
    float  r_center_off;         /**< Cached offset center R. */
    float  x_center_off;         /**< Cached offset center X. */
    float  radius_sq;            /**< Cached radius squared. */
    bool   geom_cached;          /**< True if geometry has been computed. */
} le_dist21_state_t;

/**
 * @brief Dual-slope differential protection characteristic (ANSI 87).
 */
typedef struct {
    float  o87p;       /**< Differential pickup (pu), the y-intercept of the ramp. */
    float  slp1;       /**< First slope (pu operate per pu restraint), I_rt <= irs1. */
    float  irs1;       /**< Restraint-current knee where the slope changes slope (pu). */
    float  slp2;       /**< Second slope (pu operate per pu restraint), I_rt > irs1. */
    float  operate;    /**< Measured operate current (|vector sum|), pu. */
    float  restraint;  /**< Measured restraint current (sum of phasor magnitudes), pu. */
    bool   tripped;    /**< Differential (87) trip flag. */
} le_diff87_state_t;

/**
 * @brief Three-phase transformer phase-shift compensation (ANSI 87T).
 */
typedef struct {
    uint8_t comp;   /**< SEL compensation matrix index k (1..12). */
} le_comp33_state_t;
#endif

#if LE_ENABLE_SERIAL_BUS
/**
 * @brief Master I2C peripheral transaction device configuration and state.
 */
typedef struct {
    uint8_t  addr_7bit;           /**< 7-bit slave address. */
    uint8_t  startup_data[16];    /**< Startup register configuration bytes. */
    uint8_t  startup_len;         /**< Length of startup configuration data. */
    uint32_t poll_rate_ms;        /**< Periodic polling interval in milliseconds (0 for trigger-only). */
    uint8_t  poll_tx_data[16];    /**< Command/register bytes sent during polling. */
    uint8_t  poll_tx_len;         /**< Length of transmit data during polling. */
    uint8_t  poll_rx_len;         /**< Number of bytes expected in receive response. */
    uint16_t data_dest_addr;      /**< Destination process image address for received data. */
    bool     initialized;         /**< Set to true once startup sequence has been transmitted. */
    uint32_t last_poll_ms;        /**< Timestamp of previous polling transaction. */
    bool     prev_trigger;        /**< Previous cycle trigger input for rising-edge detection. */
    uint8_t  rx_buf[16];          /**< Buffer holding last received payload bytes. */
    uint32_t poll_count;          /**< Total count of successful polling transactions. */
    bool     last_success;        /**< True if the most recent transaction succeeded. */
} le_i2c_device_state_t;

/**
 * @brief Master SPI peripheral transaction device configuration and state.
 */
typedef struct {
    uint8_t  cs_pin;              /**< GPIO chip select pin number. */
    uint8_t  startup_data[16];    /**< Startup initialization bytes. */
    uint8_t  startup_len;         /**< Length of startup initialization data. */
    uint32_t poll_rate_ms;        /**< Periodic polling interval in milliseconds (0 for trigger-only). */
    uint8_t  poll_tx_data[16];    /**< Command bytes sent during polling. */
    uint8_t  poll_len;            /**< Number of bytes transferred per polling transaction. */
    uint16_t data_dest_addr;      /**< Destination process image address for received data. */
    bool     initialized;         /**< Set to true once startup sequence has been transmitted. */
    uint32_t last_poll_ms;        /**< Timestamp of previous polling transaction. */
    bool     prev_trigger;        /**< Previous cycle trigger input for rising-edge detection. */
    uint8_t  rx_buf[16];          /**< Buffer holding last received payload bytes. */
    uint32_t poll_count;          /**< Total count of successful polling transactions. */
    bool     last_success;        /**< True if the most recent transaction succeeded. */
} le_spi_device_state_t;
#endif

#if LE_ENABLE_DSP
/**
 * @brief Single-pole Exponential Moving Average (EWMA) low-pass filter state.
 */
typedef struct {
    float    alpha;        /**< Filter smoothing coefficient (0.0 to 1.0). */
    float    prev_y;       /**< Filter output value from previous cycle. */
    bool     initialized;  /**< Set to true on first evaluation cycle. */
} le_lpf_state_t;

/**
 * @brief 2nd-order Direct Form II Biquad IIR filter state and coefficients.
 */
typedef struct {
    float    b0;           /**< Direct path numerator coefficient. */
    float    b1;           /**< 1-sample delay numerator coefficient. */
    float    b2;           /**< 2-sample delay numerator coefficient. */
    float    a1;           /**< 1-sample delay denominator feedback coefficient. */
    float    a2;           /**< 2-sample delay denominator feedback coefficient. */
    float    w1;           /**< Delay register state 1. */
    float    w2;           /**< Delay register state 2. */
    bool     initialized;  /**< Set to true on first evaluation cycle. */
} le_biquad_state_t;

/**
 * @brief N-tap finite sliding window moving average filter state.
 */
typedef struct {
    float    buffer[LE_MOVING_AVG_MAX_WINDOW]; /**< Circular sample history window. */
    uint16_t window_size;                      /**< Active filter tap window length (1..32). */
    uint16_t write_idx;                        /**< Circular buffer write pointer. */
    uint16_t count;                            /**< Number of accumulated samples so far. */
    float    sum;                              /**< Running sum of active samples in window. */
} le_moving_avg_state_t;

/**
 * @brief Slew rate limiter element state.
 */
typedef struct {
    float    rising_rate;  /**< Maximum permitted positive change per cycle. */
    float    falling_rate; /**< Maximum permitted negative change per cycle (positive magnitude). */
    float    prev_y;       /**< Slew output from previous cycle. */
    bool     initialized;  /**< Set to true on first evaluation cycle. */
} le_rate_limiter_state_t;

/**
 * @brief Deadband / noise threshold filter parameters.
 */
typedef struct {
    float    threshold;    /**< Half-width threshold deadband around center. */
    float    center;       /**< Nominal center baseline offset (typically 0.0). */
} le_deadband_state_t;

/**
 * @brief High-pass washout filter state.
 */
typedef struct {
    float    alpha;        /**< Filter coefficient (0.0 to 1.0). */
    float    prev_x;       /**< Input sample from previous cycle. */
    float    prev_y;       /**< Output sample from previous cycle. */
    bool     initialized;  /**< Set to true on first evaluation cycle. */
} le_washout_state_t;

/**
 * @brief Peak / envelope follower detector state.
 */
typedef struct {
    float    decay_rate;   /**< Multiplicative decay factor per cycle (0.0 to 1.0, e.g. 0.995). */
    float    peak;         /**< Current tracked envelope magnitude. */
    bool     initialized;  /**< Set to true on first evaluation cycle. */
} le_peak_state_t;

/**
 * @brief True RMS sliding window meter state.
 */
typedef struct {
    float    buffer[LE_RMS_MAX_WINDOW]; /**< Circular history of squared samples. */
    uint16_t window_size;               /**< Active RMS window length (1..32). */
    uint16_t write_idx;                 /**< Circular buffer write pointer. */
    uint16_t count;                     /**< Number of accumulated samples so far. */
    float    sum_sq;                    /**< Running sum of squared samples in window. */
} le_rms_state_t;

/**
 * @brief Median filter element state.
 */
typedef struct {
    float    buffer[LE_MAX_MEDIAN_WINDOW]; /**< Circular sample history window. */
    uint16_t window_size;                  /**< Filter window size (3, 5, 7, 9). */
    uint16_t write_idx;                    /**< Circular buffer write pointer. */
    uint16_t count;                        /**< Accumulated sample count. */
} le_median_state_t;

/**
 * @brief Filtered derivative element state.
 */
typedef struct {
    float    alpha;        /**< Lowpass smoothing factor (0.0 to 1.0). */
    float    gain;         /**< Derivative gain scaling (typically 1.0 / dt). */
    float    prev_x;       /**< Previous input sample. */
    float    prev_y;       /**< Previous derivative output. */
    bool     initialized;  /**< Set to true after first cycle. */
} le_derivative_state_t;

/**
 * @brief Zero-crossing detector and frequency counter state.
 */
typedef struct {
    float    hysteresis;               /**< Half-width noise band around zero. */
    float    frequency_hz;             /**< Most recently measured frequency (Hz). */
    uint32_t samples_since_cross;      /**< Accumulated sample counter since last crossing. */
    int8_t   last_state;               /**< Previous sign state (-1 = neg, +1 = pos, 0 = init). */
} le_zero_crossing_state_t;

/**
 * @brief 1D Piecewise linear lookup table state.
 */
typedef struct {
    float    x[LE_MAX_LUT_POINTS];     /**< Monotonically increasing X breakpoint coordinates. */
    float    y[LE_MAX_LUT_POINTS];     /**< Corresponding Y value coordinates. */
    uint16_t num_points;               /**< Active number of breakpoints (2..16). */
} le_lut_1d_state_t;

/**
 * @brief Totalizer / numerical integrator element state.
 */
typedef struct {
    float    accumulator;              /**< High-precision running sum (Kahan-compensated). */
    float    compensation;             /**< Lost low-order bits accumulator for 48-bit equivalent precision. */
    float    time_base_sec;            /**< Time base divisor (1=sec, 60=min, 3600=hr). */
    float    scale_factor;             /**< Output scaling multiplier. */
    float    max_limit;                /**< Clamping upper limit (0 = unlimited). */
    float    prev_x;                   /**< Previous input rate sample for trapezoidal integration. */
    bool     initialized;              /**< Set to true after first cycle. */
} le_totalizer_state_t;

/**
 * @brief Min / Max peak hold element state.
 */
typedef struct {
    float    min_val;                  /**< Lowest recorded value since reset. */
    float    max_val;                  /**< Highest recorded value since reset. */
    uint8_t  mode;                     /**< Output mode: 0 = Max, 1 = Min, 2 = Span (Max - Min). */
    bool     initialized;              /**< Set to true on first cycle. */
} le_min_max_hold_state_t;
#endif

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* LE_ELEMENTS_H */

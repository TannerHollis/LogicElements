/**
 * @file le_process_image.c
 * @brief Implementation of process image memory table.
 */

#include "le_process_image.h"
#include "le_rt.h"
#include <string.h>

void le_process_image_init(le_process_image_t* img)
{
    if (!img) return;
    /* Registers + I/O bitmaps only. Element state lives in the state workspace
     * (le_rt), copied verbatim from the preconfigured .lebin state image at load. */
    memset(img, 0, sizeof(le_process_image_t));
}

le_timer_state_t* le_process_image_timer(const le_process_image_t* img, uint16_t idx)
{
    return (le_timer_state_t*)le_rt_state(LE_BLK_TIMER, idx);
}

le_counter_state_t* le_process_image_counter(const le_process_image_t* img, uint16_t idx)
{
    return (le_counter_state_t*)le_rt_state(LE_BLK_COUNTER, idx);
}

uint8_t* le_process_image_kind_state(const le_process_image_t* img, uint8_t kind, uint16_t idx)
{
    return le_rt_state(kind, idx);
}

/**
 * @brief Applies the per-kind factory defaults to a freshly bound state block,
 * mirroring the defaults that le_process_image_init applies to the fixed arrays
 * (so heap-bound elements behave like their fixed-array counterparts).
 */
bool le_process_image_get_bool(const le_process_image_t* img, uint16_t addr)
{
    if (!img || addr == LE_ADDR_UNUSED) return false;

    if (addr == LE_CONST_FALSE) return false;
    if (addr == LE_CONST_TRUE) return true;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;

    switch (region)
    {
        case LE_REGION_DIN:
            if (idx < LE_MAX_DIGITAL_IN) {
                return (img->din[idx >> 3] & (1U << (idx & 7))) != 0;
            }
            break;

        case LE_REGION_DOUT:
            if (idx < LE_MAX_DIGITAL_OUT) {
                return (img->dout[idx >> 3] & (1U << (idx & 7))) != 0;
            }
            break;

        case LE_REGION_BOOL_REG:
        case LE_REGION_BOOL_REG_EXT: {
            uint16_t bool_idx = addr & 0x1FFF;
            if (bool_idx < LE_MAX_BOOL_REGS) {
                return (img->bool_regs[bool_idx >> 3] & (1U << (bool_idx & 7))) != 0;
            }
            break;
        }

        case LE_REGION_TIMER: {
            le_timer_state_t* t = le_process_image_timer(img, idx);
            if (t) return t->q;
            break;
        }

        case LE_REGION_COUNTER: {
            le_counter_state_t* c = le_process_image_counter(img, idx);
            if (c) return c->qu;
            break;
        }

        default:
            break;
    }

    return false;
}

void le_process_image_set_bool(le_process_image_t* img, uint16_t addr, bool val)
{
    if (!img || addr == LE_ADDR_UNUSED) return;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;

    switch (region)
    {
        case LE_REGION_DIN:
            if (idx < LE_MAX_DIGITAL_IN) {
                if (val) {
                    img->din[idx >> 3] |= (uint8_t)(1U << (idx & 7));
                } else {
                    img->din[idx >> 3] &= (uint8_t)~(1U << (idx & 7));
                }
            }
            break;

        case LE_REGION_DOUT:
            if (idx < LE_MAX_DIGITAL_OUT) {
                if (val) {
                    img->dout[idx >> 3] |= (uint8_t)(1U << (idx & 7));
                } else {
                    img->dout[idx >> 3] &= (uint8_t)~(1U << (idx & 7));
                }
            }
            break;

        case LE_REGION_BOOL_REG:
        case LE_REGION_BOOL_REG_EXT: {
            uint16_t bool_idx = addr & 0x1FFF;
            if (bool_idx < LE_MAX_BOOL_REGS) {
                if (val) {
                    img->bool_regs[bool_idx >> 3] |= (uint8_t)(1U << (bool_idx & 7));
                } else {
                    img->bool_regs[bool_idx >> 3] &= (uint8_t)~(1U << (bool_idx & 7));
                }
            }
            break;
        }

        case LE_REGION_TIMER: {
            le_timer_state_t* t = le_process_image_timer(img, idx);
            if (t) t->q = val;
            break;
        }

        case LE_REGION_COUNTER: {
            le_counter_state_t* c = le_process_image_counter(img, idx);
            if (c) c->qu = val;
            break;
        }

        default:
            break;
    }
}

float le_process_image_get_float(const le_process_image_t* img, uint16_t addr)
{
    if (!img || addr == LE_ADDR_UNUSED) return 0.0f;

    if (addr == LE_CONST_ZERO_F) return 0.0f;
    if (addr == LE_CONST_ONE_F) return 1.0f;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    if (region >= LE_REGION_FLOAT && region <= LE_REGION_FLOAT_EXT3)
    {
        uint16_t idx = addr & 0x3FFF;
        if (idx < LE_MAX_FLOATS) {
            return img->floats[idx];
        }
    }
    else if (region == LE_REGION_AIN)
    {
        uint16_t idx = addr & LE_ADDR_INDEX_MASK;
#if LE_ENABLE_ANALOG
        if (idx < LE_MAX_ANALOG_IN) {
            return img->ain[idx];
        }
#endif
    }

    return 0.0f;
}

void le_process_image_set_float(le_process_image_t* img, uint16_t addr, float val)
{
    if (!img || addr == LE_ADDR_UNUSED) return;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    if (region >= LE_REGION_FLOAT && region <= LE_REGION_FLOAT_EXT3)
    {
        uint16_t idx = addr & 0x3FFF;
        if (idx < LE_MAX_FLOATS) {
            img->floats[idx] = val;
        }
    }
    else if (region == LE_REGION_AIN)
    {
        uint16_t idx = addr & LE_ADDR_INDEX_MASK;
#if LE_ENABLE_ANALOG
        if (idx < LE_MAX_ANALOG_IN) {
            img->ain[idx] = val;
            img->ain_raw[idx] = (int32_t)val;
        }
#endif
    }
}

int32_t le_process_image_get_int(const le_process_image_t* img, uint16_t addr)
{
    if (!img || addr == LE_ADDR_UNUSED) return 0;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;

    if (region == LE_REGION_INT_REG) {
        if (idx < LE_MAX_INT_REGS) {
            return img->int_regs[idx];
        }
    } else if (region == LE_REGION_AIN) {
#if LE_ENABLE_ANALOG
        if (idx < LE_MAX_ANALOG_IN) {
            return img->ain_raw[idx];
        }
#endif
    } else if (region == LE_REGION_COUNTER) {
        le_counter_state_t* c = le_process_image_counter(img, idx);
        if (c) return c->count;
    }
    return 0;
}

void le_process_image_set_int(le_process_image_t* img, uint16_t addr, int32_t val)
{
    if (!img || addr == LE_ADDR_UNUSED) return;

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;

    if (region == LE_REGION_INT_REG) {
        if (idx < LE_MAX_INT_REGS) {
            img->int_regs[idx] = val;
        }
    } else if (region == LE_REGION_AIN) {
#if LE_ENABLE_ANALOG
        if (idx < LE_MAX_ANALOG_IN) {
            img->ain_raw[idx] = val;
            img->ain[idx] = (float)val;
        }
#endif
    } else if (region == LE_REGION_COUNTER) {
        le_counter_state_t* c = le_process_image_counter(img, idx);
        if (c) c->count = val;
    }
}

#if LE_ENABLE_COMPLEX
le_complex_t le_process_image_get_complex(const le_process_image_t* img, uint16_t addr)
{
    if (!img || addr == LE_ADDR_UNUSED) return le_c_make(0.0f, 0.0f);
    if (addr == LE_CONST_ZERO_C) return le_c_make(0.0f, 0.0f);
    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;
    if (region == LE_REGION_CMPLX) {
        if (idx < LE_MAX_COMPLEX) {
            return ((le_process_image_t*)img)->cmplx[idx];
        }
    }
    return le_c_make(0.0f, 0.0f);
}

void le_process_image_set_complex(le_process_image_t* img, uint16_t addr, le_complex_t val)
{
    if (!img || addr == LE_ADDR_UNUSED) return;
    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;
    if (region == LE_REGION_CMPLX) {
        if (idx < LE_MAX_COMPLEX) {
            img->cmplx[idx] = val;
        }
    }
}
#endif

/**
 * @brief Writes an "active" (non-zero) or idle (zero) value to a writable
 * register based on its address region.
 *
 * Bool regions (digital in/out, bool registers) receive the boolean; float and
 * complex registers receive 1.0 / (1+0j) active and 0 active-idle; integer and
 * analog-input registers receive 1 / 0. This is the shared funnel used by the
 * alias/register pulse command so a caller does not need to know the type.
 *
 * @param img Pointer to the process image structure.
 * @param addr Encoded 16-bit process-image address.
 * @param active True to set the active (non-zero) state, false to clear to zero.
 */
void le_process_image_set_active(le_process_image_t* img, uint16_t addr, bool active)
{
    if (!img || addr == LE_ADDR_UNUSED) return;
    uint16_t region = addr & LE_ADDR_REGION_MASK;
    switch (region)
    {
        case LE_REGION_DIN:
        case LE_REGION_DOUT:
        case LE_REGION_BOOL_REG:
        case LE_REGION_BOOL_REG_EXT:
            le_process_image_set_bool(img, addr, active);
            break;
        case LE_REGION_FLOAT:
        case LE_REGION_FLOAT_EXT1:
        case LE_REGION_FLOAT_EXT2:
        case LE_REGION_FLOAT_EXT3:
            le_process_image_set_float(img, addr, active ? 1.0f : 0.0f);
            break;
        case LE_REGION_INT_REG:
        case LE_REGION_AIN:
            le_process_image_set_int(img, addr, active ? 1 : 0);
            break;
#if LE_ENABLE_COMPLEX
        case LE_REGION_CMPLX:
            le_process_image_set_complex(img, addr, le_c_make(active ? 1.0f : 0.0f, 0.0f));
            break;
#endif
        default:
            break;
    }
}

void le_process_image_set_scaler(le_process_image_t* img, uint8_t idx,
                                 float raw_min, float raw_max,
                                 float scale_min, float scale_max,
                                 bool clamp)
{
    if (!img) return;
    le_scale_state_t* s = (le_scale_state_t*)le_process_image_kind_state(img, LE_BLK_SCALER, idx);
    if (!s) return;
    s->raw_min = raw_min;
    s->raw_max = raw_max;
    s->scale_min = scale_min;
    s->scale_max = scale_max;
    s->clamp = clamp;
}

#if LE_ENABLE_SERIAL_BUS
void le_i2c_device_config(le_process_image_t* img, uint8_t idx, uint8_t addr_7bit,
                          const uint8_t* startup_data, uint8_t startup_len,
                          uint32_t poll_rate_ms,
                          const uint8_t* poll_tx_data, uint8_t poll_tx_len,
                          uint8_t poll_rx_len, uint16_t data_dest_addr)
{
    if (!img) return;
    le_i2c_device_state_t* dev = (le_i2c_device_state_t*)le_process_image_kind_state(img, LE_BLK_I2C, idx);
    if (!dev) return;


    memset(dev, 0, sizeof(le_i2c_device_state_t));

    dev->addr_7bit = addr_7bit;
    dev->poll_rate_ms = poll_rate_ms;
    dev->data_dest_addr = data_dest_addr;

    if (startup_data && startup_len > 0) {
        if (startup_len > sizeof(dev->startup_data)) startup_len = sizeof(dev->startup_data);
        memcpy(dev->startup_data, startup_data, startup_len);
        dev->startup_len = startup_len;
    }

    if (poll_tx_data && poll_tx_len > 0) {
        if (poll_tx_len > sizeof(dev->poll_tx_data)) poll_tx_len = sizeof(dev->poll_tx_data);
        memcpy(dev->poll_tx_data, poll_tx_data, poll_tx_len);
        dev->poll_tx_len = poll_tx_len;
    }

    if (poll_rx_len > sizeof(dev->rx_buf)) poll_rx_len = sizeof(dev->rx_buf);
    dev->poll_rx_len = poll_rx_len;
}

void le_spi_device_config(le_process_image_t* img, uint8_t idx, uint8_t cs_pin,
                          const uint8_t* startup_data, uint8_t startup_len,
                          uint32_t poll_rate_ms,
                          const uint8_t* poll_tx_data, uint8_t poll_len,
                          uint16_t data_dest_addr)
{
    if (!img) return;
    le_spi_device_state_t* dev = (le_spi_device_state_t*)le_process_image_kind_state(img, LE_BLK_SPI, idx);
    if (!dev) return;


    memset(dev, 0, sizeof(le_spi_device_state_t));

    dev->cs_pin = cs_pin;
    dev->poll_rate_ms = poll_rate_ms;
    dev->data_dest_addr = data_dest_addr;

    if (startup_data && startup_len > 0) {
        if (startup_len > sizeof(dev->startup_data)) startup_len = sizeof(dev->startup_data);
        memcpy(dev->startup_data, startup_data, startup_len);
        dev->startup_len = startup_len;
    }

    if (poll_tx_data && poll_len > 0) {
        if (poll_len > sizeof(dev->poll_tx_data)) poll_len = sizeof(dev->poll_tx_data);
        memcpy(dev->poll_tx_data, poll_tx_data, poll_len);
        dev->poll_len = poll_len;
    } else {
        dev->poll_len = poll_len;
    }
}
#endif

#if LE_ENABLE_DSP
void le_process_image_set_lpf(le_process_image_t* img, uint8_t idx, float alpha)
{
    if (!img) return;
    le_lpf_state_t* st = (le_lpf_state_t*)le_process_image_kind_state(img, LE_BLK_LPF, idx);
    if (!st) return;

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    st->alpha = alpha;
    st->initialized = false;
    st->prev_y = 0.0f;
}

void le_process_image_set_biquad(le_process_image_t* img, uint8_t idx,
                                 float b0, float b1, float b2, float a1, float a2)
{
    if (!img) return;
    le_biquad_state_t* st = (le_biquad_state_t*)le_process_image_kind_state(img, LE_BLK_BIQUAD, idx);
    if (!st) return;

    st->b0 = b0;
    st->b1 = b1;
    st->b2 = b2;
    st->a1 = a1;
    st->a2 = a2;
    st->w1 = 0.0f;
    st->w2 = 0.0f;
    st->initialized = false;
}

void le_process_image_set_moving_avg(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img) return;
    le_moving_avg_state_t* st = (le_moving_avg_state_t*)le_process_image_kind_state(img, LE_BLK_MOVING_AVG, idx);
    if (!st) return;

    if (window_size == 0) window_size = 1;
    if (window_size > LE_MOVING_AVG_MAX_WINDOW) window_size = LE_MOVING_AVG_MAX_WINDOW;
    memset(st->buffer, 0, sizeof(st->buffer));
    st->window_size = window_size;
    st->write_idx = 0;
    st->count = 0;
    st->sum = 0.0f;
}

void le_process_image_set_rate_limiter(le_process_image_t* img, uint8_t idx,
                                       float rising_rate, float falling_rate)
{
    if (!img) return;
    le_rate_limiter_state_t* st = (le_rate_limiter_state_t*)le_process_image_kind_state(img, LE_BLK_RATE_LIMITER, idx);
    if (!st) return;

    if (rising_rate < 0.0f) rising_rate = -rising_rate;
    if (falling_rate < 0.0f) falling_rate = -falling_rate;
    st->rising_rate = rising_rate;
    st->falling_rate = falling_rate;
    st->prev_y = 0.0f;
    st->initialized = false;
}

void le_process_image_set_deadband(le_process_image_t* img, uint8_t idx,
                                   float threshold, float center)
{
    if (!img) return;
    le_deadband_state_t* st = (le_deadband_state_t*)le_process_image_kind_state(img, LE_BLK_DEADBAND, idx);
    if (!st) return;

    if (threshold < 0.0f) threshold = -threshold;
    st->threshold = threshold;
    st->center = center;
}

void le_process_image_set_washout(le_process_image_t* img, uint8_t idx, float alpha)
{
    if (!img) return;
    le_washout_state_t* st = (le_washout_state_t*)le_process_image_kind_state(img, LE_BLK_WASHOUT, idx);
    if (!st) return;

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    st->alpha = alpha;
    st->prev_x = 0.0f;
    st->prev_y = 0.0f;
    st->initialized = false;
}

void le_process_image_set_peak(le_process_image_t* img, uint8_t idx, float decay_rate)
{
    if (!img) return;
    le_peak_state_t* st = (le_peak_state_t*)le_process_image_kind_state(img, LE_BLK_PEAK, idx);
    if (!st) return;

    if (decay_rate < 0.0f) decay_rate = 0.0f;
    if (decay_rate > 1.0f) decay_rate = 1.0f;
    st->decay_rate = decay_rate;
    st->peak = 0.0f;
    st->initialized = false;
}

void le_process_image_set_rms(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img) return;
    le_rms_state_t* st = (le_rms_state_t*)le_process_image_kind_state(img, LE_BLK_RMS, idx);
    if (!st) return;

    if (window_size == 0) window_size = 1;
    if (window_size > LE_RMS_MAX_WINDOW) window_size = LE_RMS_MAX_WINDOW;
    memset(st->buffer, 0, sizeof(st->buffer));
    st->window_size = window_size;
    st->write_idx = 0;
    st->count = 0;
    st->sum_sq = 0.0f;
}

void le_process_image_set_median(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img) return;
    le_median_state_t* st = (le_median_state_t*)le_process_image_kind_state(img, LE_BLK_MEDIAN, idx);
    if (!st) return;

    if (window_size == 0) window_size = 1;
    if (window_size > LE_MAX_MEDIAN_WINDOW) window_size = LE_MAX_MEDIAN_WINDOW;
    memset(st->buffer, 0, sizeof(st->buffer));
    st->window_size = window_size;
    st->write_idx = 0;
    st->count = 0;
}

void le_process_image_set_derivative(le_process_image_t* img, uint8_t idx, float alpha, float gain)
{
    if (!img) return;
    le_derivative_state_t* st = (le_derivative_state_t*)le_process_image_kind_state(img, LE_BLK_DERIVATIVE, idx);
    if (!st) return;

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    st->alpha = alpha;
    st->gain = gain;
    st->prev_x = 0.0f;
    st->prev_y = 0.0f;
    st->initialized = false;
}

void le_process_image_set_zero_crossing(le_process_image_t* img, uint8_t idx, float hysteresis, float sample_rate_hz)
{
    if (!img) return;
    le_zero_crossing_state_t* st = (le_zero_crossing_state_t*)le_process_image_kind_state(img, LE_BLK_ZERO_CROSSING, idx);
    if (!st) return;

    if (hysteresis < 0.0f) hysteresis = -hysteresis;
    if (sample_rate_hz <= 0.0f) sample_rate_hz = 1000.0f;
    st->hysteresis = hysteresis;
    st->sample_rate_hz = sample_rate_hz;
    st->frequency_hz = 0.0f;
    st->samples_since_cross = 0;
    st->last_state = 0;
}

void le_process_image_set_lut_1d(le_process_image_t* img, uint8_t idx, const float* x, const float* y, uint16_t num_points)
{
    if (!img || !x || !y) return;
    le_lut_1d_state_t* st = (le_lut_1d_state_t*)le_process_image_kind_state(img, LE_BLK_LUT_1D, idx);
    if (!st) return;

    if (num_points < 2) num_points = 2;
    if (num_points > LE_MAX_LUT_POINTS) num_points = LE_MAX_LUT_POINTS;
    st->num_points = num_points;
    for (uint16_t i = 0; i < num_points; i++) {
        st->x[i] = x[i];
        st->y[i] = y[i];
    }
}

void le_process_image_set_totalizer(le_process_image_t* img, uint8_t idx,
                                    float time_base_sec, float scale_factor,
                                    float sample_time_sec, float max_limit)
{
    if (!img) return;
    le_totalizer_state_t* st = (le_totalizer_state_t*)le_process_image_kind_state(img, LE_BLK_TOTALIZER, idx);
    if (!st) return;

    if (time_base_sec <= 0.0f) time_base_sec = 60.0f;
    if (sample_time_sec <= 0.0f) sample_time_sec = 0.001f;
    st->accumulator = 0.0;
    st->time_base_sec = time_base_sec;
    st->scale_factor = scale_factor;
    st->sample_time_sec = sample_time_sec;
    st->max_limit = max_limit;
    st->prev_x = 0.0f;
    st->initialized = false;
}

void le_process_image_set_min_max_hold(le_process_image_t* img, uint8_t idx, uint8_t mode)
{
    if (!img) return;
    le_min_max_hold_state_t* st = (le_min_max_hold_state_t*)le_process_image_kind_state(img, LE_BLK_MIN_MAX_HOLD, idx);
    if (!st) return;

    st->mode = mode;
    st->min_val = 0.0f;
    st->max_val = 0.0f;
    st->initialized = false;
}
#endif

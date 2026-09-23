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
    memset(img, 0, sizeof(le_process_image_t));
    for (int i = 0; i < LE_MAX_SCALERS; i++) {
        img->scalers[i].raw_min = 0.0f;
        img->scalers[i].raw_max = 4095.0f;
        img->scalers[i].scale_min = 0.0f;
        img->scalers[i].scale_max = 100.0f;
        img->scalers[i].clamp = true;
    }
#if LE_ENABLE_DSP
    for (int i = 0; i < LE_MAX_DSP_FILTERS; i++) {
        img->lpfs[i].alpha = 0.1f;
        img->biquads[i].b0 = 1.0f; /* Unity gain passthrough by default */
        img->moving_avgs[i].window_size = 8;
        img->rate_limiters[i].rising_rate = 1.0f;
        img->rate_limiters[i].falling_rate = 1.0f;
        img->washouts[i].alpha = 0.95f;
        img->peaks[i].decay_rate = 0.995f;
        img->rms_meters[i].window_size = 16;
        img->medians[i].window_size = 5;
        img->derivatives[i].alpha = 0.8f;
        img->derivatives[i].gain = 1000.0f;
        img->zero_crossings[i].hysteresis = 0.05f;
        img->zero_crossings[i].sample_rate_hz = 1000.0f;
        img->luts[i].num_points = 2;
        img->luts[i].x[0] = 0.0f; img->luts[i].y[0] = 0.0f;
        img->luts[i].x[1] = 100.0f; img->luts[i].y[1] = 100.0f;
        img->totalizers[i].time_base_sec = 60.0f;
        img->totalizers[i].scale_factor = 1.0f;
        img->totalizers[i].sample_time_sec = 0.001f;
        img->min_max_holds[i].mode = 0;
    }
#endif
}

le_timer_state_t* le_process_image_timer(const le_process_image_t* img, uint16_t idx)
{
    int base = le_rt_group_base(LE_BLK_TIMER);
    if (base >= 0) {
        le_rt_block_t* rb = le_rt_get(base + idx);
        return rb ? (le_timer_state_t*)rb->state : NULL;
    }
    if (idx < LE_MAX_TIMERS) return (le_timer_state_t*)&((le_process_image_t*)img)->timers[idx];
    return NULL;
}

le_counter_state_t* le_process_image_counter(const le_process_image_t* img, uint16_t idx)
{
    int base = le_rt_group_base(LE_BLK_COUNTER);
    if (base >= 0) {
        le_rt_block_t* rb = le_rt_get(base + idx);
        return rb ? (le_counter_state_t*)rb->state : NULL;
    }
    if (idx < LE_MAX_COUNTERS) return (le_counter_state_t*)&((le_process_image_t*)img)->counters[idx];
    return NULL;
}

uint8_t* le_process_image_kind_state(const le_process_image_t* img, uint8_t kind, uint16_t idx)
{
    le_process_image_t* pi = (le_process_image_t*)img;
    int base = le_rt_group_base(kind);
    if (base >= 0) {
        le_rt_block_t* rb = le_rt_get(base + idx);
        return rb ? rb->state : NULL;
    }
    switch (kind) {
#if LE_ENABLE_DSP
        case LE_BLK_LPF:            return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->lpfs[idx] : NULL;
        case LE_BLK_BIQUAD:         return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->biquads[idx] : NULL;
        case LE_BLK_MOVING_AVG:     return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->moving_avgs[idx] : NULL;
        case LE_BLK_RATE_LIMITER:   return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->rate_limiters[idx] : NULL;
        case LE_BLK_DEADBAND:       return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->deadbands[idx] : NULL;
        case LE_BLK_WASHOUT:        return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->washouts[idx] : NULL;
        case LE_BLK_PEAK:           return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->peaks[idx] : NULL;
        case LE_BLK_RMS:            return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->rms_meters[idx] : NULL;
        case LE_BLK_MEDIAN:         return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->medians[idx] : NULL;
        case LE_BLK_DERIVATIVE:     return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->derivatives[idx] : NULL;
        case LE_BLK_ZERO_CROSSING:  return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->zero_crossings[idx] : NULL;
        case LE_BLK_LUT_1D:         return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->luts[idx] : NULL;
        case LE_BLK_TOTALIZER:      return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->totalizers[idx] : NULL;
        case LE_BLK_MIN_MAX_HOLD:   return (idx < LE_MAX_DSP_FILTERS) ? (uint8_t*)&pi->min_max_holds[idx] : NULL;
#endif
        case LE_BLK_SCALER:         return (idx < LE_MAX_SCALERS) ? (uint8_t*)&pi->scalers[idx] : NULL;
        case LE_BLK_PID:            return (idx < LE_MAX_PID) ? (uint8_t*)&pi->pids[idx] : NULL;
        case LE_BLK_OVERCURRENT:    return (idx < LE_MAX_OVERCURRENT) ? (uint8_t*)&pi->overcurrents[idx] : NULL;
        case LE_BLK_PHASOR:         return (idx < LE_MAX_PHASORS) ? (uint8_t*)&pi->phasors[idx] : NULL;
#if LE_ENABLE_PROTECTION
        case LE_BLK_SYMCOMP:        return (idx < (LE_MAX_PHASORS / 3 + 1)) ? (uint8_t*)&pi->symcomps[idx] : NULL;
        case LE_BLK_87:             return (idx < LE_MAX_DIFF87) ? (uint8_t*)&pi->diff87s[idx] : NULL;
        case LE_BLK_21:             return (idx < LE_MAX_DIST21) ? (uint8_t*)&pi->dist21s[idx] : NULL;
#endif
        default: break;
    }
    return NULL;
}

/**
 * @brief Applies the per-kind factory defaults to a freshly bound state block,
 * mirroring the defaults that le_process_image_init applies to the fixed arrays
 * (so heap-bound elements behave like their fixed-array counterparts).
 */
void le_rt_apply_defaults(uint8_t kind, uint8_t* state)
{
    if (!state) return;
#if LE_ENABLE_DSP
    if (kind == LE_BLK_LPF) {
        le_lpf_state_t* s = (le_lpf_state_t*)state; s->alpha = 0.1f;
    } else if (kind == LE_BLK_BIQUAD) {
        le_biquad_state_t* s = (le_biquad_state_t*)state; s->b0 = 1.0f;
    } else if (kind == LE_BLK_MOVING_AVG) {
        le_moving_avg_state_t* s = (le_moving_avg_state_t*)state; s->window_size = 8;
    } else if (kind == LE_BLK_RATE_LIMITER) {
        le_rate_limiter_state_t* s = (le_rate_limiter_state_t*)state; s->rising_rate = 1.0f; s->falling_rate = 1.0f;
    } else if (kind == LE_BLK_WASHOUT) {
        le_washout_state_t* s = (le_washout_state_t*)state; s->alpha = 0.95f;
    } else if (kind == LE_BLK_PEAK) {
        le_peak_state_t* s = (le_peak_state_t*)state; s->decay_rate = 0.995f;
    } else if (kind == LE_BLK_RMS) {
        le_rms_state_t* s = (le_rms_state_t*)state; s->window_size = 16;
    } else if (kind == LE_BLK_MEDIAN) {
        le_median_state_t* s = (le_median_state_t*)state; s->window_size = 5;
    } else if (kind == LE_BLK_DERIVATIVE) {
        le_derivative_state_t* s = (le_derivative_state_t*)state; s->alpha = 0.8f; s->gain = 1000.0f;
    } else if (kind == LE_BLK_ZERO_CROSSING) {
        le_zero_crossing_state_t* s = (le_zero_crossing_state_t*)state; s->hysteresis = 0.05f; s->sample_rate_hz = 1000.0f;
    } else if (kind == LE_BLK_LUT_1D) {
        le_lut_1d_state_t* s = (le_lut_1d_state_t*)state; s->num_points = 2; s->x[0] = 0.0f; s->y[0] = 0.0f; s->x[1] = 100.0f; s->y[1] = 100.0f;
    } else if (kind == LE_BLK_TOTALIZER) {
        le_totalizer_state_t* s = (le_totalizer_state_t*)state; s->time_base_sec = 60.0f; s->scale_factor = 1.0f; s->sample_time_sec = 0.001f;
    } else if (kind == LE_BLK_MIN_MAX_HOLD) {
        le_min_max_hold_state_t* s = (le_min_max_hold_state_t*)state; s->mode = 0;
    }
#endif
    if (kind == LE_BLK_SCALER) {
        le_scale_state_t* s = (le_scale_state_t*)state; s->raw_max = 4095.0f; s->scale_min = 0.0f; s->scale_max = 100.0f; s->clamp = true;
    }
}

/**
 * @brief Applies one per-element config directive to a state block (heap-bound
 * or fixed-array fallback), so compiled circuits honor their schema tuning.
 */
void le_rt_configure(const le_process_image_t* img, uint8_t kind, uint8_t idx, uint8_t slot, float value)
{
    uint8_t* st = le_process_image_kind_state(img, kind, idx);
    if (!st) return;
    switch (kind) {
        case LE_BLK_SCALER: {
            le_scale_state_t* s = (le_scale_state_t*)st;
            if (slot == 0) s->raw_min = value;
            else if (slot == 1) s->raw_max = value;
            else if (slot == 2) s->scale_min = value;
            else if (slot == 3) s->scale_max = value;
            else if (slot == 4) s->clamp = (value != 0.0f);
            break;
        }
        case LE_BLK_LPF: { le_lpf_state_t* s = (le_lpf_state_t*)st; s->alpha = value; break; }
        case LE_BLK_RATE_LIMITER: {
            le_rate_limiter_state_t* s = (le_rate_limiter_state_t*)st;
            if (slot == 0) s->rising_rate = value; else s->falling_rate = value; break;
        }
        case LE_BLK_BIQUAD: {
            le_biquad_state_t* s = (le_biquad_state_t*)st;
            if (slot == 0) s->b0 = value; else if (slot == 1) s->b1 = value;
            else if (slot == 2) s->b2 = value; else if (slot == 3) s->a1 = value; else s->a2 = value;
            break;
        }
        case LE_BLK_MOVING_AVG: { le_moving_avg_state_t* s = (le_moving_avg_state_t*)st; s->window_size = (uint16_t)value; break; }
        case LE_BLK_PEAK: { le_peak_state_t* s = (le_peak_state_t*)st; s->decay_rate = value; break; }
        case LE_BLK_RMS: { le_rms_state_t* s = (le_rms_state_t*)st; s->window_size = (uint16_t)value; break; }
        case LE_BLK_MEDIAN: { le_median_state_t* s = (le_median_state_t*)st; s->window_size = (uint16_t)value; break; }
        case LE_BLK_PID: {
            le_pid_state_t* s = (le_pid_state_t*)st;
            if (slot == 0) s->kp = value; else if (slot == 1) s->ki = value;
            else if (slot == 2) s->kd = value; else if (slot == 3) s->out_min = value; else s->out_max = value;
            break;
        }
        default: break;
    }
}

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
        if (idx < LE_MAX_ANALOG_IN) {
            return img->ain[idx];
        }
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
        if (idx < LE_MAX_ANALOG_IN) {
            img->ain[idx] = val;
            img->ain_raw[idx] = (int32_t)val;
        }
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
        if (idx < LE_MAX_ANALOG_IN) {
            return img->ain_raw[idx];
        }
    } else if (region == LE_REGION_COUNTER) {
        if (idx < LE_MAX_COUNTERS) {
            return img->counters[idx].count;
        }
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
        if (idx < LE_MAX_ANALOG_IN) {
            img->ain_raw[idx] = val;
            img->ain[idx] = (float)val;
        }
    } else if (region == LE_REGION_COUNTER) {
        if (idx < LE_MAX_COUNTERS) {
            img->counters[idx].count = val;
        }
    }
}

void le_process_image_set_scaler(le_process_image_t* img, uint8_t idx,
                                 float raw_min, float raw_max,
                                 float scale_min, float scale_max,
                                 bool clamp)
{
    if (!img || idx >= LE_MAX_SCALERS) return;
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
    if (!img || idx >= LE_MAX_I2C_DEVICES) return;

    le_i2c_device_state_t* dev = &img->i2c_devices[idx];
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
    if (!img || idx >= LE_MAX_SPI_DEVICES) return;

    le_spi_device_state_t* dev = &img->spi_devices[idx];
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
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    img->lpfs[idx].alpha = alpha;
    img->lpfs[idx].initialized = false;
    img->lpfs[idx].prev_y = 0.0f;
}

void le_process_image_set_biquad(le_process_image_t* img, uint8_t idx,
                                 float b0, float b1, float b2, float a1, float a2)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    img->biquads[idx].b0 = b0;
    img->biquads[idx].b1 = b1;
    img->biquads[idx].b2 = b2;
    img->biquads[idx].a1 = a1;
    img->biquads[idx].a2 = a2;
    img->biquads[idx].w1 = 0.0f;
    img->biquads[idx].w2 = 0.0f;
    img->biquads[idx].initialized = false;
}

void le_process_image_set_moving_avg(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (window_size == 0) window_size = 1;
    if (window_size > LE_MOVING_AVG_MAX_WINDOW) window_size = LE_MOVING_AVG_MAX_WINDOW;
    memset(img->moving_avgs[idx].buffer, 0, sizeof(img->moving_avgs[idx].buffer));
    img->moving_avgs[idx].window_size = window_size;
    img->moving_avgs[idx].write_idx = 0;
    img->moving_avgs[idx].count = 0;
    img->moving_avgs[idx].sum = 0.0f;
}

void le_process_image_set_rate_limiter(le_process_image_t* img, uint8_t idx,
                                       float rising_rate, float falling_rate)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (rising_rate < 0.0f) rising_rate = -rising_rate;
    if (falling_rate < 0.0f) falling_rate = -falling_rate;
    img->rate_limiters[idx].rising_rate = rising_rate;
    img->rate_limiters[idx].falling_rate = falling_rate;
    img->rate_limiters[idx].prev_y = 0.0f;
    img->rate_limiters[idx].initialized = false;
}

void le_process_image_set_deadband(le_process_image_t* img, uint8_t idx,
                                   float threshold, float center)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (threshold < 0.0f) threshold = -threshold;
    img->deadbands[idx].threshold = threshold;
    img->deadbands[idx].center = center;
}

void le_process_image_set_washout(le_process_image_t* img, uint8_t idx, float alpha)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    img->washouts[idx].alpha = alpha;
    img->washouts[idx].prev_x = 0.0f;
    img->washouts[idx].prev_y = 0.0f;
    img->washouts[idx].initialized = false;
}

void le_process_image_set_peak(le_process_image_t* img, uint8_t idx, float decay_rate)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (decay_rate < 0.0f) decay_rate = 0.0f;
    if (decay_rate > 1.0f) decay_rate = 1.0f;
    img->peaks[idx].decay_rate = decay_rate;
    img->peaks[idx].peak = 0.0f;
    img->peaks[idx].initialized = false;
}

void le_process_image_set_rms(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (window_size == 0) window_size = 1;
    if (window_size > LE_RMS_MAX_WINDOW) window_size = LE_RMS_MAX_WINDOW;
    memset(img->rms_meters[idx].buffer, 0, sizeof(img->rms_meters[idx].buffer));
    img->rms_meters[idx].window_size = window_size;
    img->rms_meters[idx].write_idx = 0;
    img->rms_meters[idx].count = 0;
    img->rms_meters[idx].sum_sq = 0.0f;
}

void le_process_image_set_median(le_process_image_t* img, uint8_t idx, uint16_t window_size)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (window_size == 0) window_size = 1;
    if (window_size > LE_MAX_MEDIAN_WINDOW) window_size = LE_MAX_MEDIAN_WINDOW;
    memset(img->medians[idx].buffer, 0, sizeof(img->medians[idx].buffer));
    img->medians[idx].window_size = window_size;
    img->medians[idx].write_idx = 0;
    img->medians[idx].count = 0;
}

void le_process_image_set_derivative(le_process_image_t* img, uint8_t idx, float alpha, float gain)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    img->derivatives[idx].alpha = alpha;
    img->derivatives[idx].gain = gain;
    img->derivatives[idx].prev_x = 0.0f;
    img->derivatives[idx].prev_y = 0.0f;
    img->derivatives[idx].initialized = false;
}

void le_process_image_set_zero_crossing(le_process_image_t* img, uint8_t idx, float hysteresis, float sample_rate_hz)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (hysteresis < 0.0f) hysteresis = -hysteresis;
    if (sample_rate_hz <= 0.0f) sample_rate_hz = 1000.0f;
    img->zero_crossings[idx].hysteresis = hysteresis;
    img->zero_crossings[idx].sample_rate_hz = sample_rate_hz;
    img->zero_crossings[idx].frequency_hz = 0.0f;
    img->zero_crossings[idx].samples_since_cross = 0;
    img->zero_crossings[idx].last_state = 0;
}

void le_process_image_set_lut_1d(le_process_image_t* img, uint8_t idx, const float* x, const float* y, uint16_t num_points)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS || !x || !y) return;
    if (num_points < 2) num_points = 2;
    if (num_points > LE_MAX_LUT_POINTS) num_points = LE_MAX_LUT_POINTS;
    img->luts[idx].num_points = num_points;
    for (uint16_t i = 0; i < num_points; i++) {
        img->luts[idx].x[i] = x[i];
        img->luts[idx].y[i] = y[i];
    }
}

void le_process_image_set_totalizer(le_process_image_t* img, uint8_t idx,
                                    float time_base_sec, float scale_factor,
                                    float sample_time_sec, float max_limit)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    if (time_base_sec <= 0.0f) time_base_sec = 60.0f;
    if (sample_time_sec <= 0.0f) sample_time_sec = 0.001f;
    img->totalizers[idx].accumulator = 0.0;
    img->totalizers[idx].time_base_sec = time_base_sec;
    img->totalizers[idx].scale_factor = scale_factor;
    img->totalizers[idx].sample_time_sec = sample_time_sec;
    img->totalizers[idx].max_limit = max_limit;
    img->totalizers[idx].prev_x = 0.0f;
    img->totalizers[idx].initialized = false;
}

void le_process_image_set_min_max_hold(le_process_image_t* img, uint8_t idx, uint8_t mode)
{
    if (!img || idx >= LE_MAX_DSP_FILTERS) return;
    img->min_max_holds[idx].mode = mode;
    img->min_max_holds[idx].min_val = 0.0f;
    img->min_max_holds[idx].max_val = 0.0f;
    img->min_max_holds[idx].initialized = false;
}
#endif

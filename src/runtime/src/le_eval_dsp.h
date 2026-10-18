/**
 * @file le_eval_dsp.h
 * @brief Inline evaluation handlers for DSP elements and digital filters.
 */

#ifndef LE_EVAL_DSP_H
#define LE_EVAL_DSP_H

#include "le_types.h"
#include "le_process_image.h"
#include <math.h>

#if LE_ENABLE_DSP

static inline le_status_t le_eval_lpf_1p(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_lpf_state_t* f = (le_lpf_state_t*)le_process_image_kind_state(img, LE_BLK_LPF, idx);
    if (!f) return LE_ERR_OUT_OF_BOUNDS;
    float alpha = f->alpha;
    if (inst->in_b != LE_ADDR_UNUSED) {
        float dyn_alpha = le_pi_get_float_fast(img, inst->in_b);
        if (dyn_alpha >= 0.0f && dyn_alpha <= 1.0f) alpha = dyn_alpha;
    }
    if (!f->initialized) {
        f->prev_y = in_x;
        f->initialized = true;
        out_y = in_x;
    } else {
        out_y = f->prev_y + alpha * (in_x - f->prev_y);
        f->prev_y = out_y;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_biquad_iir(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_biquad_state_t* b = (le_biquad_state_t*)le_process_image_kind_state(img, LE_BLK_BIQUAD, idx);
    if (!b) return LE_ERR_OUT_OF_BOUNDS;
    /* Direct Form II Canonical Form */
    float w = in_x - (b->a1 * b->w1) - (b->a2 * b->w2);
    out_y = (b->b0 * w) + (b->b1 * b->w1) + (b->b2 * b->w2);
    b->w2 = b->w1;
    b->w1 = w;
    b->initialized = true;
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_moving_avg(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_moving_avg_state_t* m = (le_moving_avg_state_t*)le_process_image_kind_state(img, LE_BLK_MOVING_AVG, idx);
    if (!m) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t wsz = (m->window_size > 0 && m->window_size <= LE_MOVING_AVG_MAX_WINDOW) ? m->window_size : 8;
    if (m->count < wsz) {
        m->buffer[m->write_idx] = in_x;
        m->sum += in_x;
        m->count++;
        m->write_idx++;
        if (m->write_idx >= wsz) m->write_idx = 0;
        out_y = m->sum / (float)m->count;
    } else {
        m->sum = m->sum - m->buffer[m->write_idx] + in_x;
        m->buffer[m->write_idx] = in_x;
        m->write_idx++;
        if (m->write_idx >= wsz) m->write_idx = 0;
        out_y = m->sum / (float)wsz;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_rate_limiter(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_rate_limiter_state_t* r = (le_rate_limiter_state_t*)le_process_image_kind_state(img, LE_BLK_RATE_LIMITER, idx);
    if (!r) return LE_ERR_OUT_OF_BOUNDS;
    if (!r->initialized) {
        r->prev_y = in_x;
        r->initialized = true;
        out_y = in_x;
    } else {
        float delta = in_x - r->prev_y;
        if (delta > r->rising_rate) {
            out_y = r->prev_y + r->rising_rate;
        } else if (delta < -r->falling_rate) {
            out_y = r->prev_y - r->falling_rate;
        } else {
            out_y = in_x;
        }
        r->prev_y = out_y;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_deadband(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    const le_deadband_state_t* d = (le_deadband_state_t*)le_process_image_kind_state(img, LE_BLK_DEADBAND, idx);
    if (!d) return LE_ERR_OUT_OF_BOUNDS;
    float diff = in_x - d->center;
    if (fabsf(diff) <= d->threshold) {
        out_y = d->center;
    } else if (diff > d->threshold) {
        out_y = d->center + (diff - d->threshold);
    } else {
        out_y = d->center + (diff + d->threshold);
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_washout(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_washout_state_t* w = (le_washout_state_t*)le_process_image_kind_state(img, LE_BLK_WASHOUT, idx);
    if (!w) return LE_ERR_OUT_OF_BOUNDS;
    if (!w->initialized) {
        w->prev_x = in_x;
        w->prev_y = 0.0f;
        w->initialized = true;
        out_y = 0.0f;
    } else {
        out_y = w->alpha * (w->prev_y + in_x - w->prev_x);
        w->prev_x = in_x;
        w->prev_y = out_y;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_peak_detector(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_peak_state_t* p = (le_peak_state_t*)le_process_image_kind_state(img, LE_BLK_PEAK, idx);
    if (!p) return LE_ERR_OUT_OF_BOUNDS;
    float mag = fabsf(in_x);
    if (!p->initialized) {
        p->peak = mag;
        p->initialized = true;
        out_y = mag;
    } else {
        if (mag >= p->peak) {
            p->peak = mag;
        } else {
            p->peak = p->peak * p->decay_rate;
        }
        out_y = p->peak;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_rms(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_rms_state_t* rms = (le_rms_state_t*)le_process_image_kind_state(img, LE_BLK_RMS, idx);
    if (!rms) return LE_ERR_OUT_OF_BOUNDS;
    float sq = in_x * in_x;
    uint16_t wsz = (rms->window_size > 0 && rms->window_size <= LE_RMS_MAX_WINDOW) ? rms->window_size : 16;
    if (rms->count < wsz) {
        rms->buffer[rms->write_idx] = sq;
        rms->sum_sq += sq;
        rms->count++;
        rms->write_idx++;
        if (rms->write_idx >= wsz) rms->write_idx = 0;
        out_y = sqrtf(rms->sum_sq / (float)rms->count);
    } else {
        rms->sum_sq = rms->sum_sq - rms->buffer[rms->write_idx] + sq;
        if (rms->sum_sq < 0.0f) rms->sum_sq = 0.0f;
        rms->buffer[rms->write_idx] = sq;
        rms->write_idx++;
        if (rms->write_idx >= wsz) rms->write_idx = 0;
        out_y = sqrtf(rms->sum_sq / (float)wsz);
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_median(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_median_state_t* m = (le_median_state_t*)le_process_image_kind_state(img, LE_BLK_MEDIAN, idx);
    if (!m) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t wsz = (m->window_size > 0 && m->window_size <= LE_MAX_MEDIAN_WINDOW) ? m->window_size : 5;
    m->buffer[m->write_idx] = in_x;
    m->write_idx++;
    if (m->write_idx >= wsz) m->write_idx = 0;
    if (m->count < wsz) m->count++;

    float sort_buf[LE_MAX_MEDIAN_WINDOW];
    uint16_t n = m->count;
    for (uint16_t i = 0; i < n; i++) {
        sort_buf[i] = m->buffer[i];
    }
    for (uint16_t i = 1; i < n; i++) {
        float key = sort_buf[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sort_buf[j] > key) {
            sort_buf[j + 1] = sort_buf[j];
            j--;
        }
        sort_buf[j + 1] = key;
    }
    out_y = sort_buf[n / 2];
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_derivative(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img, float scan_srate)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_derivative_state_t* d = (le_derivative_state_t*)le_process_image_kind_state(img, LE_BLK_DERIVATIVE, idx);
    if (!d) return LE_ERR_OUT_OF_BOUNDS;
    float alpha = d->alpha;
    if (inst->in_b != LE_ADDR_UNUSED) {
        float dyn_alpha = le_pi_get_float_fast(img, inst->in_b);
        if (dyn_alpha >= 0.0f && dyn_alpha <= 1.0f) alpha = dyn_alpha;
    }
    if (!d->initialized) {
        d->prev_x = in_x;
        d->prev_y = 0.0f;
        d->initialized = true;
        out_y = 0.0f;
    } else {
        float dx = in_x - d->prev_x;
        float raw_deriv = dx * scan_srate * d->gain;
        out_y = d->prev_y + alpha * (raw_deriv - d->prev_y);
        d->prev_x = in_x;
        d->prev_y = out_y;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_zero_crossing(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img, float scan_srate)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_zero_crossing_state_t* zc = (le_zero_crossing_state_t*)le_process_image_kind_state(img, LE_BLK_ZERO_CROSSING, idx);
    if (!zc) return LE_ERR_OUT_OF_BOUNDS;
    if (inst->in_b != LE_ADDR_UNUSED && le_pi_get_bool_fast(img, inst->in_b)) {
        zc->frequency_hz = 0.0f;
        zc->samples_since_cross = 0;
        zc->last_state = 0;
    } else {
        float srate = scan_srate;
        zc->samples_since_cross++;
        int8_t current_state = zc->last_state;
        if (in_x > zc->hysteresis) {
            current_state = 1;
        } else if (in_x < -zc->hysteresis) {
            current_state = -1;
        }

        if (zc->last_state == -1 && current_state == 1) {
            if (zc->samples_since_cross > 0) {
                zc->frequency_hz = srate / (float)zc->samples_since_cross;
            }
            zc->samples_since_cross = 0;
        }

        if (zc->samples_since_cross > (uint32_t)(srate * 2.0f)) {
            zc->frequency_hz = 0.0f;
        }

        zc->last_state = current_state;
    }
    out_y = zc->frequency_hz;
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_lut_1d(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    const le_lut_1d_state_t* lut = (const le_lut_1d_state_t*)le_process_image_kind_state(img, LE_BLK_LUT_1D, idx);
    if (!lut) return LE_ERR_OUT_OF_BOUNDS;
    if (lut->num_points < 2) {
        out_y = (lut->num_points == 1) ? lut->y[0] : in_x;
    } else if (in_x <= lut->x[0]) {
        out_y = lut->y[0];
    } else if (in_x >= lut->x[lut->num_points - 1]) {
        out_y = lut->y[lut->num_points - 1];
    } else {
        out_y = lut->y[lut->num_points - 1];
        for (uint16_t i = 0; i < lut->num_points - 1; i++) {
            if (in_x >= lut->x[i] && in_x <= lut->x[i + 1]) {
                float dx = lut->x[i + 1] - lut->x[i];
                if (fabsf(dx) > 1e-9f) {
                    float t = (in_x - lut->x[i]) / dx;
                    out_y = lut->y[i] + t * (lut->y[i + 1] - lut->y[i]);
                } else {
                    out_y = lut->y[i];
                }
                break;
            }
        }
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_totalizer(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img, float scan_dt)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_totalizer_state_t* tot = (le_totalizer_state_t*)le_process_image_kind_state(img, LE_BLK_TOTALIZER, idx);
    if (!tot) return LE_ERR_OUT_OF_BOUNDS;
    bool reset = false;
    if (inst->in_b != LE_ADDR_UNUSED) {
        reset = le_pi_get_bool_fast(img, inst->in_b);
    }
    if (reset) {
        tot->accumulator = 0.0f;
        tot->compensation = 0.0f;
        tot->prev_x = in_x;
        tot->initialized = true;
    } else if (!tot->initialized) {
        tot->prev_x = in_x;
        tot->initialized = true;
    } else {
        float dt = scan_dt;
        float tb = tot->time_base_sec;
        if (tb <= 0.0f) tb = 1.0f;
        float avg_rate = 0.5f * (in_x + tot->prev_x);
        float delta = (avg_rate * (dt / tb)) * tot->scale_factor;

        /* Kahan compensated summation */
        float y = delta - tot->compensation;
        float t = tot->accumulator + y;
        tot->compensation = (t - tot->accumulator) - y;
        tot->accumulator = t;

        if (tot->max_limit > 0.0f && tot->accumulator > tot->max_limit) {
            tot->accumulator = tot->max_limit;
            tot->compensation = 0.0f;
        }
        tot->prev_x = in_x;
    }
    out_y = tot->accumulator;
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

static inline le_status_t le_eval_min_max_hold(const le_instruction_t* inst, uint8_t mod, le_process_image_t* img)
{
    uint8_t idx = mod & 0xFF;
    float in_x = le_pi_get_float_fast(img, inst->in_a);
    float out_y = 0.0f;
    le_min_max_hold_state_t* h = (le_min_max_hold_state_t*)le_process_image_kind_state(img, LE_BLK_MIN_MAX_HOLD, idx);
    if (!h) return LE_ERR_OUT_OF_BOUNDS;
    bool reset = false;
    if (inst->in_b != LE_ADDR_UNUSED) {
        reset = le_pi_get_bool_fast(img, inst->in_b);
    }
    if (reset || !h->initialized) {
        h->min_val = in_x;
        h->max_val = in_x;
        h->initialized = true;
    } else {
        if (in_x < h->min_val) h->min_val = in_x;
        if (in_x > h->max_val) h->max_val = in_x;
    }
    if (h->mode == 1) {
        out_y = h->min_val;
    } else if (h->mode == 2) {
        out_y = h->max_val - h->min_val;
    } else {
        out_y = h->max_val;
    }
    le_pi_set_float_fast(img, inst->out, out_y);
    return LE_OK;
}

#endif /* LE_ENABLE_DSP */

#endif /* LE_EVAL_DSP_H */

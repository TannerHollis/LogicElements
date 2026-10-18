/**
 * @file le_eval_relay.h
 * @brief Inline evaluation handlers for control, protective relaying, and serial communication.
 */

#ifndef LE_EVAL_RELAY_H
#define LE_EVAL_RELAY_H

#include "le_types.h"
#include "le_process_image.h"
#include "le_hal.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#if LE_ENABLE_DSP
static inline le_status_t le_eval_pid(const le_instruction_t* inst, uint8_t mod,
                                      le_process_image_t* img, uint32_t now_ms, float scan_dt)
{
    (void)mod;
    uint16_t pid_idx = inst->modifier & 0xFF;
    le_pid_state_t* p = (le_pid_state_t*)le_process_image_kind_state(img, LE_BLK_PID, pid_idx);
    if (!p) return LE_ERR_OUT_OF_BOUNDS;
    float sp = le_pi_get_float_fast(img, inst->in_a);
    float pv = le_pi_get_float_fast(img, inst->in_b);
    float error = sp - pv;

    float dt = (now_ms - p->last_time_ms) * 0.001f;
    if (dt <= 0.0f || dt > 1.0f) dt = scan_dt; /* default to the enforced scan period */
    p->last_time_ms = now_ms;

    /* Proportional */
    float p_out = p->kp * error;

    /* Integral */
    p->integral += p->ki * error * dt;
    /* Anti-windup */
    if (p->integral > p->out_max) p->integral = p->out_max;
    if (p->integral < p->out_min) p->integral = p->out_min;

    /* Derivative */
    float d_out = p->kd * (error - p->prev_error) / dt;
    p->prev_error = error;

    float output = p_out + p->integral + d_out;
    if (output > p->out_max) output = p->out_max;
    if (output < p->out_min) output = p->out_min;

    le_pi_set_float_fast(img, inst->out, output);
    return LE_OK;
}
#endif /* LE_ENABLE_DSP */

#if LE_ENABLE_PROTECTION
static inline le_status_t le_eval_sym_comp(const le_instruction_t* inst, uint8_t mod,
                                          le_process_image_t* img)
{
    (void)mod;
    uint16_t sc_idx = inst->modifier & 0x07;
    uint16_t base_p = (inst->in_a != LE_ADDR_UNUSED) ? (inst->in_a & 0xFF) : 0;

    le_symcomp_state_t* sc = (le_symcomp_state_t*)le_process_image_kind_state(img, LE_BLK_SYMCOMP, sc_idx);
    if (!sc) return LE_ERR_OUT_OF_BOUNDS;
    le_phasor_state_t* pa = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, base_p);
    le_phasor_state_t* pb = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, base_p + 1);
    le_phasor_state_t* pc = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, base_p + 2);
    sc->phase_a = pa ? pa->phasor : le_c_make(0, 0);
    sc->phase_b = pb ? pb->phasor : le_c_make(0, 0);
    sc->phase_c = pc ? pc->phasor : le_c_make(0, 0);

    /* alpha = 1 < 120 deg = -0.5 + j(sqrt(3)/2) */
    const le_complex_t alpha  = { -0.5f,  0.8660254f };
    /* alpha^2 = 1 < 240 deg = -0.5 - j(sqrt(3)/2) */
    const le_complex_t alpha2 = { -0.5f, -0.8660254f };

    /* I0 = (A + B + C) / 3 */
    le_complex_t sum0 = le_c_add(le_c_add(sc->phase_a, sc->phase_b), sc->phase_c);
    sc->seq_0 = le_c_scale(sum0, 1.0f / 3.0f);

    /* I1 = (A + alpha*B + alpha2*C) / 3 */
    le_complex_t sum1 = le_c_add(sc->phase_a, le_c_add(le_c_mul(alpha, sc->phase_b), le_c_mul(alpha2, sc->phase_c)));
    sc->seq_1 = le_c_scale(sum1, 1.0f / 3.0f);

    /* I2 = (A + alpha2*B + alpha*C) / 3 */
    le_complex_t sum2 = le_c_add(sc->phase_a, le_c_add(le_c_mul(alpha2, sc->phase_b), le_c_mul(alpha, sc->phase_c)));
    sc->seq_2 = le_c_scale(sum2, 1.0f / 3.0f);

    /* Write positive sequence magnitude to out */
    le_pi_set_float_fast(img, inst->out, le_c_mag(sc->seq_1));
    return LE_OK;
}
#endif /* LE_ENABLE_PROTECTION */

#if LE_ENABLE_SERIAL_BUS
static inline le_status_t le_eval_i2c(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms)
{
    uint8_t idx = inst->modifier;
    le_i2c_device_state_t* dev =
        (le_i2c_device_state_t*)le_process_image_kind_state(img, LE_BLK_I2C, idx);
    if (!dev) return LE_ERR_OUT_OF_BOUNDS;

    /* 1. Startup Code: execute on first scan if not initialized */
    if (!dev->initialized) {
        if (dev->startup_len > 0 && g_le_hal && g_le_hal->i2c_write) {
            dev->initialized = g_le_hal->i2c_write(dev->addr_7bit, dev->startup_data, dev->startup_len);
        } else {
            dev->initialized = true;
        }
    }

    /* 2. Rising-edge trigger evaluation on in_a */
    bool trigger = le_process_image_get_bool(img, inst->in_a);
    bool rising_edge = (trigger && !dev->prev_trigger);
    dev->prev_trigger = trigger;

    /* 3. Determine if poll should occur: rising edge OR polling rate expired */
    bool do_poll = false;
    if (rising_edge) {
        do_poll = true;
    } else if (dev->poll_rate_ms > 0 && (now_ms - dev->last_poll_ms >= dev->poll_rate_ms)) {
        do_poll = true;
    }

    bool success = false;
    if (do_poll) {
        dev->last_poll_ms = now_ms;
        if (g_le_hal) {
            if (dev->poll_tx_len > 0 && g_le_hal->i2c_write_read) {
                success = g_le_hal->i2c_write_read(dev->addr_7bit, dev->poll_tx_data, dev->poll_tx_len,
                                                   dev->rx_buf, dev->poll_rx_len);
            } else if (g_le_hal->i2c_read && dev->poll_rx_len > 0) {
                success = g_le_hal->i2c_read(dev->addr_7bit, dev->rx_buf, dev->poll_rx_len);
            }
        }

        if (success) {
            dev->poll_count++;
            dev->last_success = true;

            /* Store received data in process image if destination configured */
            if (dev->data_dest_addr != LE_ADDR_UNUSED) {
                if (LE_ADDR_IS_FLOAT(dev->data_dest_addr)) {
                    float val = 0.0f;
                    if (dev->poll_rx_len >= 4) {
                        memcpy(&val, dev->rx_buf, 4);
                    } else if (dev->poll_rx_len >= 2) {
                        int16_t raw16 = (int16_t)(((uint16_t)dev->rx_buf[0] << 8) | dev->rx_buf[1]);
                        val = (float)raw16;
                    } else if (dev->poll_rx_len == 1) {
                        val = (float)dev->rx_buf[0];
                    }
                    le_process_image_set_float(img, dev->data_dest_addr, val);
                } else {
                    bool b = (dev->rx_buf[0] != 0);
                    le_process_image_set_bool(img, dev->data_dest_addr, b);
                }
            }
        } else {
            dev->last_success = false;
        }
    }

    /* 4. Output status / strobe */
    if (inst->out != LE_ADDR_UNUSED) {
        le_process_image_set_bool(img, inst->out, do_poll ? success : dev->last_success);
    }
    return LE_OK;
}

static inline le_status_t le_eval_spi(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms)
{
    uint8_t idx = inst->modifier;
    le_spi_device_state_t* dev =
        (le_spi_device_state_t*)le_process_image_kind_state(img, LE_BLK_SPI, idx);
    if (!dev) return LE_ERR_OUT_OF_BOUNDS;

    /* 1. Startup Code: execute on first scan if not initialized */
    if (!dev->initialized) {
        if (dev->startup_len > 0 && g_le_hal && g_le_hal->spi_transfer) {
            dev->initialized = g_le_hal->spi_transfer(dev->cs_pin, dev->startup_data, NULL, dev->startup_len);
        } else {
            dev->initialized = true;
        }
    }

    /* 2. Rising-edge trigger evaluation on in_a */
    bool trigger = le_process_image_get_bool(img, inst->in_a);
    bool rising_edge = (trigger && !dev->prev_trigger);
    dev->prev_trigger = trigger;

    /* 3. Determine if poll should occur */
    bool do_poll = false;
    if (rising_edge) {
        do_poll = true;
    } else if (dev->poll_rate_ms > 0 && (now_ms - dev->last_poll_ms >= dev->poll_rate_ms)) {
        do_poll = true;
    }

    bool success = false;
    if (do_poll) {
        dev->last_poll_ms = now_ms;
        if (g_le_hal && g_le_hal->spi_transfer) {
            success = g_le_hal->spi_transfer(dev->cs_pin, dev->poll_tx_data, dev->rx_buf, dev->poll_len);
        }

        if (success) {
            dev->poll_count++;
            dev->last_success = true;

            if (dev->data_dest_addr != LE_ADDR_UNUSED) {
                if (LE_ADDR_IS_FLOAT(dev->data_dest_addr)) {
                    float val = 0.0f;
                    if (dev->poll_len >= 4) {
                        memcpy(&val, dev->rx_buf, 4);
                    } else if (dev->poll_len >= 2) {
                        int16_t raw16 = (int16_t)(((uint16_t)dev->rx_buf[0] << 8) | dev->rx_buf[1]);
                        val = (float)raw16;
                    } else if (dev->poll_len == 1) {
                        val = (float)dev->rx_buf[0];
                    }
                    le_process_image_set_float(img, dev->data_dest_addr, val);
                } else {
                    bool b = (dev->rx_buf[0] != 0);
                    le_process_image_set_bool(img, dev->data_dest_addr, b);
                }
            }
        } else {
            dev->last_success = false;
        }
    }

    if (inst->out != LE_ADDR_UNUSED) {
        le_process_image_set_bool(img, inst->out, do_poll ? success : dev->last_success);
    }
    return LE_OK;
}
#endif /* LE_ENABLE_SERIAL_BUS */

#if LE_ENABLE_PROTECTION
static inline le_status_t le_eval_relay_phasor_1p(const le_block_desc_t* desc, const uint16_t* args,
                                                  le_process_image_t* img, float scan_srate)
{
    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t p_idx = desc->reserved;
    le_phasor_state_t* p = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, p_idx);
    if (!p) p = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, 0);
    if (!p) return LE_ERR_OUT_OF_BOUNDS;

    float sample = le_pi_get_float_fast(img, args[0]);
    le_complex_t sync = le_pi_get_complex_fast(img, args[1]);
    float freq_hz = 60.0f;
    uint16_t out_addr = args[2];
    if (desc->in_count >= 3) {
        freq_hz = le_pi_get_float_fast(img, args[2]);
        out_addr = args[3];
    }

    float sample_rate = scan_srate;
    if (freq_hz <= 0.0f) freq_hz = 60.0f;
    if (freq_hz > sample_rate / 2.0f) freq_hz = sample_rate / 2.0f;

    float samples_per_cycle = sample_rate / freq_hz;
    if (samples_per_cycle < 4.0f) samples_per_cycle = 4.0f;
    if (samples_per_cycle > (float)LE_MAX_RAW_SAMPLES) samples_per_cycle = (float)LE_MAX_RAW_SAMPLES;

    uint16_t N = (uint16_t)(samples_per_cycle + 0.5f);
    if (N < 4) N = 4;
    if (N > LE_MAX_RAW_SAMPLES) N = LE_MAX_RAW_SAMPLES;
    p->samples_per_cycle = N;

    if (p->raw_write_idx >= N) p->raw_write_idx = 0;
    p->raw_samples[p->raw_write_idx] = sample;
    p->raw_write_idx++;
    if (p->raw_write_idx >= N) p->raw_write_idx = 0;

    float sum_cos = 0.0f, sum_sin = 0.0f;
    float angle_step = 2.0f * (float)M_PI / (float)N;
    float sin_step, cos_step;
    le_sincos(angle_step, &sin_step, &cos_step);
    float c = 1.0f, s = 0.0f;

    uint16_t s_idx = p->raw_write_idx;
    for (uint16_t k = 0; k < N; k++) {
        float s_val = p->raw_samples[s_idx];
        sum_cos += s_val * c;
        sum_sin -= s_val * s;
        s_idx++;
        if (s_idx >= N) s_idx = 0;
        float c_next = c * cos_step - s * sin_step;
        float s_next = s * cos_step + c * sin_step;
        c = c_next;
        s = s_next;
    }

    float real = (2.0f / (float)N) * sum_cos;
    float imag = (2.0f / (float)N) * sum_sin;
    le_complex_t ph = le_c_make(real, imag);

    le_complex_t out;
    float mag, ang;
    if (p->self_sync) {
        mag = le_c_mag(ph);
        ang = 0.0f;
        out = le_c_make(mag, 0.0f);
    } else {
        float sync_sq = sync.r * sync.r + sync.i * sync.i;
        if (sync_sq > 1e-12f) {
            float inv_sq = 1.0f / sync_sq;
            float out_r = (ph.r * sync.r + ph.i * sync.i) * inv_sq;
            float out_i = (ph.i * sync.r - ph.r * sync.i) * inv_sq;
            out = le_c_make(out_r, out_i);
            mag = le_fast_cmplx_mag(out_r, out_i);
            ang = le_fast_atan2(out_i, out_r);
        } else {
            out = ph;
            mag = le_c_mag(ph);
            ang = le_c_ang(ph);
        }
    }

    p->magnitude = mag;
    p->angle_rad = ang;
    p->phasor = out;
    le_pi_set_complex_fast(img, out_addr, out);
    return LE_OK;
}

static inline le_status_t le_eval_relay_phasor_3p(const le_block_desc_t* desc, const uint16_t* args,
                                                  le_process_image_t* img, float scan_srate)
{
    if (desc->in_count < 5 || desc->out_count < 3) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t p_idx = desc->reserved;
    le_phasor3_state_t* p = (le_phasor3_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR3, p_idx);
    if (!p) p = (le_phasor3_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR3, 0);
    if (!p) return LE_ERR_OUT_OF_BOUNDS;

    le_complex_t sync = le_pi_get_complex_fast(img, args[3]);
    float freq_hz = le_pi_get_float_fast(img, args[4]);

    float sample_rate = scan_srate;
    if (freq_hz <= 0.0f) freq_hz = 60.0f;
    if (freq_hz > sample_rate / 2.0f) freq_hz = sample_rate / 2.0f;

    float samples_per_cycle = sample_rate / freq_hz;
    if (samples_per_cycle < 4.0f) samples_per_cycle = 4.0f;
    if (samples_per_cycle > (float)LE_MAX_RAW_SAMPLES) samples_per_cycle = (float)LE_MAX_RAW_SAMPLES;

    uint16_t N = (uint16_t)(samples_per_cycle + 0.5f);
    if (N < 4) N = 4;
    if (N > LE_MAX_RAW_SAMPLES) N = LE_MAX_RAW_SAMPLES;
    p->samples_per_cycle = N;

    float sample_a = le_pi_get_float_fast(img, args[0]);
    float sample_b = le_pi_get_float_fast(img, args[1]);
    float sample_c = le_pi_get_float_fast(img, args[2]);

    if (p->raw_write_idx_a >= N) p->raw_write_idx_a = 0;
    p->raw_samples_a[p->raw_write_idx_a] = sample_a;
    p->raw_write_idx_a++;
    if (p->raw_write_idx_a >= N) p->raw_write_idx_a = 0;

    if (p->raw_write_idx_b >= N) p->raw_write_idx_b = 0;
    p->raw_samples_b[p->raw_write_idx_b] = sample_b;
    p->raw_write_idx_b++;
    if (p->raw_write_idx_b >= N) p->raw_write_idx_b = 0;

    if (p->raw_write_idx_c >= N) p->raw_write_idx_c = 0;
    p->raw_samples_c[p->raw_write_idx_c] = sample_c;
    p->raw_write_idx_c++;
    if (p->raw_write_idx_c >= N) p->raw_write_idx_c = 0;

    float angle_step = 2.0f * (float)M_PI / (float)N;
    float sin_step, cos_step;
    le_sincos(angle_step, &sin_step, &cos_step);

    float sum_cos_a = 0.0f, sum_sin_a = 0.0f;
    float sum_cos_b = 0.0f, sum_sin_b = 0.0f;
    float sum_cos_c = 0.0f, sum_sin_c = 0.0f;
    float c = 1.0f, s = 0.0f;

    uint16_t idx_a = p->raw_write_idx_a;
    uint16_t idx_b = p->raw_write_idx_b;
    uint16_t idx_c = p->raw_write_idx_c;

    for (uint16_t k = 0; k < N; k++) {
        float sa = p->raw_samples_a[idx_a];
        float sb = p->raw_samples_b[idx_b];
        float sc = p->raw_samples_c[idx_c];

        sum_cos_a += sa * c; sum_sin_a -= sa * s;
        sum_cos_b += sb * c; sum_sin_b -= sb * s;
        sum_cos_c += sc * c; sum_sin_c -= sc * s;

        idx_a++; if (idx_a >= N) idx_a = 0;
        idx_b++; if (idx_b >= N) idx_b = 0;
        idx_c++; if (idx_c >= N) idx_c = 0;

        float c_next = c * cos_step - s * sin_step;
        float s_next = s * cos_step + c * sin_step;
        c = c_next;
        s = s_next;
    }

    float scale = 2.0f / (float)N;
    le_complex_t ph_a = le_c_make(scale * sum_cos_a, scale * sum_sin_a);
    le_complex_t ph_b = le_c_make(scale * sum_cos_b, scale * sum_sin_b);
    le_complex_t ph_c = le_c_make(scale * sum_cos_c, scale * sum_sin_c);

    le_complex_t out_a, out_b, out_c;
    float mag_a, mag_b, mag_c;
    float ang_a, ang_b, ang_c;

    if (p->self_sync) {
        mag_a = le_c_mag(ph_a); ang_a = 0.0f; out_a = le_c_make(mag_a, 0.0f);
        mag_b = le_c_mag(ph_b); ang_b = 0.0f; out_b = le_c_make(mag_b, 0.0f);
        mag_c = le_c_mag(ph_c); ang_c = 0.0f; out_c = le_c_make(mag_c, 0.0f);
    } else {
        float sync_sq = sync.r * sync.r + sync.i * sync.i;
        if (sync_sq > 1e-12f) {
            float inv_sq = 1.0f / sync_sq;
            float out_ar = (ph_a.r * sync.r + ph_a.i * sync.i) * inv_sq;
            float out_ai = (ph_a.i * sync.r - ph_a.r * sync.i) * inv_sq;
            out_a = le_c_make(out_ar, out_ai);
            mag_a = le_fast_cmplx_mag(out_ar, out_ai);
            ang_a = le_fast_atan2(out_ai, out_ar);

            float out_br = (ph_b.r * sync.r + ph_b.i * sync.i) * inv_sq;
            float out_bi = (ph_b.i * sync.r - ph_b.r * sync.i) * inv_sq;
            out_b = le_c_make(out_br, out_bi);
            mag_b = le_fast_cmplx_mag(out_br, out_bi);
            ang_b = le_fast_atan2(out_bi, out_br);

            float out_cr = (ph_c.r * sync.r + ph_c.i * sync.i) * inv_sq;
            float out_ci = (ph_c.i * sync.r - ph_c.r * sync.i) * inv_sq;
            out_c = le_c_make(out_cr, out_ci);
            mag_c = le_fast_cmplx_mag(out_cr, out_ci);
            ang_c = le_fast_atan2(out_ci, out_cr);
        } else {
            out_a = ph_a; mag_a = le_c_mag(ph_a); ang_a = le_c_ang(ph_a);
            out_b = ph_b; mag_b = le_c_mag(ph_b); ang_b = le_c_ang(ph_b);
            out_c = ph_c; mag_c = le_c_mag(ph_c); ang_c = le_c_ang(ph_c);
        }
    }

    p->magnitude_a = mag_a; p->angle_rad_a = ang_a; p->phasor_a = out_a;
    p->magnitude_b = mag_b; p->angle_rad_b = ang_b; p->phasor_b = out_b;
    p->magnitude_c = mag_c; p->angle_rad_c = ang_c; p->phasor_c = out_c;

    le_pi_set_complex_fast(img, args[5], out_a);
    le_pi_set_complex_fast(img, args[6], out_b);
    le_pi_set_complex_fast(img, args[7], out_c);
    return LE_OK;
}

static inline le_status_t le_eval_relay_freq_est(const le_block_desc_t* desc, const uint16_t* args,
                                                 le_process_image_t* img, float scan_srate)
{
    if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t f_idx = desc->reserved;
    le_freq_est_state_t* p = (le_freq_est_state_t*)le_process_image_kind_state(img, LE_BLK_FREQ_EST, f_idx);
    if (!p) p = (le_freq_est_state_t*)le_process_image_kind_state(img, LE_BLK_FREQ_EST, 0);
    if (!p) return LE_ERR_OUT_OF_BOUNDS;

    float sample = le_pi_get_float_fast(img, args[0]);
    float fs = scan_srate;

    int8_t cur = p->armed_state;
    bool fresh_cross = false;
    if (sample <= -p->hysteresis) {
        cur = -1;
    } else if (sample >= p->hysteresis && p->armed_state == -1) {
        cur = 1;
        fresh_cross = true;
    }

    p->samples_since_cross++;

    bool measured = false;
    if (fresh_cross) {
        float frac = 0.5f;
        float denom = sample - p->prev_sample;
        if (denom > 1e-12f) {
            frac = (0.0f - p->prev_sample) / denom;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
        }
        if (p->initialized) {
            float n_period = (float)p->samples_since_cross + frac - p->prev_cross_frac;
            if (n_period > 1.0f) {
                float f_raw = fs / n_period;
                float prev_out = p->tracked_freq_hz;
                float f_out = (p->filter_alpha > 0.0f)
                    ? (prev_out + p->filter_alpha * (f_raw - prev_out))
                    : f_raw;
                if (f_raw >= p->min_freq_hz && f_raw <= p->max_freq_hz) {
                    p->tracked_freq_hz = f_out;
                    p->valid = true;
                } else {
                    p->tracked_freq_hz = f_out;
                    p->valid = false;
                }
                measured = true;
            }
        }
        p->prev_cross_frac = frac;
        p->samples_since_cross = 0;
        p->initialized = true;
    }
    p->armed_state = cur;
    p->prev_sample = sample;

    float n_timeout = 1.25f * (fs / p->min_freq_hz);
    if (!measured && p->samples_since_cross > (uint32_t)n_timeout) {
        p->tracked_freq_hz = p->nominal_freq_hz;
        p->valid = false;
    }

    le_pi_set_float_fast(img, args[1], p->tracked_freq_hz);
    le_pi_set_bool_fast(img, args[2], p->valid);
    return LE_OK;
}

static inline le_status_t le_eval_relay_phase_comp(const le_block_desc_t* desc, const uint16_t* args,
                                                   le_process_image_t* img)
{
    if (desc->in_count < 3 || desc->out_count < 3) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t c_idx = desc->reserved;
    le_comp33_state_t* c33 = (le_comp33_state_t*)le_process_image_kind_state(img, LE_BLK_PHASE_COMP, c_idx);
    if (!c33) c33 = (le_comp33_state_t*)le_process_image_kind_state(img, LE_BLK_PHASE_COMP, 0);
    if (!c33) return LE_ERR_OUT_OF_BOUNDS;
    int k = (int)c33->comp;
    if (k < 1) k = 1;
    if (k > 12) k = 12;
    float s = (k & 1) ? (1.0f / sqrtf(3.0f)) : (1.0f / 3.0f);

    le_complex_t ia = le_pi_get_complex_fast(img, args[0]);
    le_complex_t ib = le_pi_get_complex_fast(img, args[1]);
    le_complex_t ic = le_pi_get_complex_fast(img, args[2]);

    int8_t m[3][3];
    switch (k) {
        case 1:  m[0][0]=1;m[0][1]=-1;m[0][2]=0;  m[1][0]=0;m[1][1]=1;m[1][2]=-1; m[2][0]=-1;m[2][1]=0;m[2][2]=1; break;
        case 2:  m[0][0]=1;m[0][1]=-2;m[0][2]=1;  m[1][0]=1;m[1][1]=1;m[1][2]=-2; m[2][0]=-2;m[2][1]=1;m[2][2]=1; break;
        case 3:  m[0][0]=0;m[0][1]=-1;m[0][2]=1;  m[1][0]=1;m[1][1]=0;m[1][2]=-1; m[2][0]=-1;m[2][1]=1;m[2][2]=0; break;
        case 4:  m[0][0]=-1;m[0][1]=-1;m[0][2]=2;  m[1][0]=2;m[1][1]=-1;m[1][2]=-1; m[2][0]=-1;m[2][1]=2;m[2][2]=-1; break;
        case 5:  m[0][0]=-1;m[0][1]=0;m[0][2]=1;  m[1][0]=1;m[1][1]=-1;m[1][2]=0; m[2][0]=0;m[2][1]=1;m[2][2]=-1; break;
        case 6:  m[0][0]=-2;m[0][1]=1;m[0][2]=1;  m[1][0]=1;m[1][1]=-2;m[1][2]=1; m[2][0]=1;m[2][1]=1;m[2][2]=-2; break;
        case 7:  m[0][0]=-1;m[0][1]=1;m[0][2]=0;  m[1][0]=0;m[1][1]=-1;m[1][2]=1; m[2][0]=1;m[2][1]=0;m[2][2]=-1; break;
        case 8:  m[0][0]=-1;m[0][1]=2;m[0][2]=-1;  m[1][0]=-1;m[1][1]=-1;m[1][2]=2; m[2][0]=2;m[2][1]=-1;m[2][2]=-1; break;
        case 9:  m[0][0]=0;m[0][1]=1;m[0][2]=-1;  m[1][0]=-1;m[1][1]=0;m[1][2]=1; m[2][0]=1;m[2][1]=-1;m[2][2]=0; break;
        case 10: m[0][0]=1;m[0][1]=1;m[0][2]=-2;  m[1][0]=-2;m[1][1]=1;m[1][2]=1; m[2][0]=1;m[2][1]=-2;m[2][2]=1; break;
        case 11: m[0][0]=1;m[0][1]=0;m[0][2]=-1;  m[1][0]=-1;m[1][1]=1;m[1][2]=0; m[2][0]=0;m[2][1]=-1;m[2][2]=1; break;
        default: m[0][0]=2;m[0][1]=-1;m[0][2]=-1;  m[1][0]=-1;m[1][1]=2;m[1][2]=-1; m[2][0]=-1;m[2][1]=-1;m[2][2]=2; break; /* k=12 */
    }

    le_complex_t oa = le_c_scale(le_c_add(le_c_add(le_c_scale(ia, (float)m[0][0]), le_c_scale(ib, (float)m[0][1])), le_c_scale(ic, (float)m[0][2])), s);
    le_complex_t ob = le_c_scale(le_c_add(le_c_add(le_c_scale(ia, (float)m[1][0]), le_c_scale(ib, (float)m[1][1])), le_c_scale(ic, (float)m[1][2])), s);
    le_complex_t oc = le_c_scale(le_c_add(le_c_add(le_c_scale(ia, (float)m[2][0]), le_c_scale(ib, (float)m[2][1])), le_c_scale(ic, (float)m[2][2])), s);
    le_pi_set_complex_fast(img, args[3], oa);
    le_pi_set_complex_fast(img, args[4], ob);
    le_pi_set_complex_fast(img, args[5], oc);
    return LE_OK;
}

static inline le_status_t le_eval_relay_diff_87(const le_block_desc_t* desc, const uint16_t* args,
                                                le_process_image_t* img)
{
    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
    int cin = desc->in_count;
    uint16_t d_idx = desc->reserved;
    le_diff87_state_t* d87 = (le_diff87_state_t*)le_process_image_kind_state(img, LE_BLK_DIFF_87, d_idx);
    if (!d87) d87 = (le_diff87_state_t*)le_process_image_kind_state(img, LE_BLK_DIFF_87, 0);
    if (!d87) return LE_ERR_OUT_OF_BOUNDS;
    le_complex_t op_sum = le_c_make(0.0f, 0.0f);
    float rt_sum = 0.0f;
    for (int k = 0; k < cin; k++) {
        le_complex_t c = le_pi_get_complex_fast(img, args[k]);
        op_sum = le_c_add(op_sum, c);
        rt_sum += le_c_mag(c);
    }
    float i_op = le_c_mag(op_sum);
    float i_rt = rt_sum;
    d87->operate = i_op;
    d87->restraint = i_rt;

    float o87p = (d87->o87p > 0.0f) ? d87->o87p : 0.3f;
    float slp1 = (d87->slp1 > 0.0f) ? d87->slp1 : 0.25f;
    float irs1 = d87->irs1;
    float slp2 = (d87->slp2 > 0.0f) ? d87->slp2 : 0.60f;
    float trip_threshold = o87p;
    if (i_rt <= irs1) trip_threshold += slp1 * i_rt;
    else trip_threshold += slp1 * irs1 + slp2 * (i_rt - irs1);

    d87->tripped = (i_op > trip_threshold);
    le_pi_set_bool_fast(img, args[cin], d87->tripped);
    return LE_OK;
}

static inline le_status_t le_eval_relay_dist_21(const le_block_desc_t* desc, const uint16_t* args,
                                                le_process_image_t* img, uint32_t now_ms)
{
    if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t d_idx = desc->reserved;
    le_dist21_state_t* dist = (le_dist21_state_t*)le_process_image_kind_state(img, LE_BLK_21, d_idx);
    if (!dist) dist = (le_dist21_state_t*)le_process_image_kind_state(img, LE_BLK_21, 0);
    if (!dist) return LE_ERR_OUT_OF_BOUNDS;

    le_complex_t v = le_pi_get_complex_fast(img, args[0]);
    le_complex_t i = le_pi_get_complex_fast(img, args[1]);
    bool offset_on = le_pi_get_bool_fast(img, args[2]);

    float v_mag = le_c_mag(v);
    bool faulted = (dist->prefault_v_threshold > 0.0f) && (v_mag < dist->prefault_v_threshold);
    if (!faulted) {
        dist->prefault_v = v;
        dist->prefault_armed = false;
        dist->prefault_arm_ms = 0;
    } else if (!dist->prefault_armed) {
        dist->prefault_armed = true;
        dist->prefault_arm_ms = now_ms;
    }

    le_complex_t v_use = v;
    if (dist->prefault_armed) {
        if (now_ms - dist->prefault_arm_ms < dist->prefault_duration_ms) {
            v_use = dist->prefault_v;
        } else {
            dist->prefault_armed = false;
            v_use = v;
        }
    }

    float i_mag = le_c_mag(i);
    if (i_mag < 0.05f) {
        le_pi_set_bool_fast(img, args[3], false);
        return LE_OK;
    }

    le_complex_t z = le_c_div(v_use, i);
    dist->r_meas = z.r;
    dist->x_meas = z.i;

    if (!dist->geom_cached) {
        float la = dist->line_angle_deg * (float)M_PI / 180.0f;
        float sla, cla;
        le_sincos(la, &sla, &cla);
        float radius = dist->reach_ohms * 0.5f;
        dist->radius_sq = radius * radius;
        dist->r_center_norm = radius * cla;
        dist->x_center_norm = radius * sla;

        if (dist->offset_mag > 0.0f) {
            float oa = dist->offset_angle_deg * (float)M_PI / 180.0f;
            float soa, coa;
            le_sincos(oa, &soa, &coa);
            float oc = dist->offset_mag * coa;
            float os = dist->offset_mag * soa;
            dist->r_center_off = dist->r_center_norm + oc;
            dist->x_center_off = dist->x_center_norm + os;
        } else {
            dist->r_center_off = dist->r_center_norm;
            dist->x_center_off = dist->x_center_norm;
        }
        dist->geom_cached = true;
    }

    float r_center = (offset_on && dist->offset_mag > 0.0f) ? dist->r_center_off : dist->r_center_norm;
    float x_center = (offset_on && dist->offset_mag > 0.0f) ? dist->x_center_off : dist->x_center_norm;
    float dr = dist->r_meas - r_center;
    float dx = dist->x_meas - x_center;
    dist->tripped = ((dr * dr + dx * dx) <= dist->radius_sq);
    le_pi_set_bool_fast(img, args[3], dist->tripped);
    return LE_OK;
}

static inline le_status_t le_eval_relay_overcurrent_51(const le_block_desc_t* desc, const uint16_t* args,
                                                      le_process_image_t* img, float scan_dt)
{
    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
    uint16_t oc_idx = desc->reserved;
    le_overcurrent_state_t* oc = (le_overcurrent_state_t*)le_process_image_kind_state(img, LE_BLK_OVERCURRENT, oc_idx);
    if (!oc) oc = (le_overcurrent_state_t*)le_process_image_kind_state(img, LE_BLK_OVERCURRENT, 0);
    if (!oc) return LE_ERR_OUT_OF_BOUNDS;

    if (oc->pickup <= 0.0f) oc->pickup = 1.0f;
    if (oc->time_dial <= 0.0f) oc->time_dial = 1.0f;

    if (oc->a_coeff == 0.0f && oc->b_coeff == 0.0f && oc->p_coeff == 0.0f) {
        float A = 13.5f, B = 0.0f, P = 1.0f;
        switch (oc->curve_type) {
            case LE_CURVE_IEC_NORMAL:      A = 0.14f;     B = 0.0f;    P = 0.02f; break;
            case LE_CURVE_IEC_VERY:        A = 13.5f;     B = 0.0f;    P = 1.0f;  break;
            case LE_CURVE_IEC_EXTREME:     A = 80.0f;     B = 0.0f;    P = 2.0f;  break;
            case LE_CURVE_IEEE_MODERATELY: A = 0.0515f;   B = 0.1140f; P = 0.02f; break;
            case LE_CURVE_IEEE_VERY:       A = 19.61f;    B = 0.491f;  P = 2.0f;  break;
            case LE_CURVE_IEEE_EXTREME:    A = 28.2f;     B = 0.1217f; P = 2.0f;  break;
            case LE_CURVE_IEEE_SHORT:      A = 0.00342f;  B = 0.00262f;P = 0.02f; break;
            case LE_CURVE_IEEE_LONG:       A = 26.13f;    B = 0.349f;  P = 2.0f;  break;
            default: break;
        }
        oc->a_coeff = A; oc->b_coeff = B; oc->p_coeff = P;
    }
    if (!(oc->a_coeff > 0.0f) || !(oc->p_coeff > 0.0f)) {
        oc->a_coeff = 13.5f; oc->b_coeff = 0.0f; oc->p_coeff = 1.0f;
    }

    le_complex_t i_c = le_pi_get_complex_fast(img, args[0]);
    bool enable = le_pi_get_bool_fast(img, args[1]);
    float M = le_c_mag(i_c) / oc->pickup;
    float time_dial = oc->time_dial;

    float dt = (scan_dt > 0.0f) ? scan_dt : 0.01f;

    if (enable && M > 1.0f) {
        float mp;
        if (oc->p_coeff == 1.0f) {
            mp = M;
        } else if (oc->p_coeff == 2.0f) {
            mp = M * M;
        } else {
            mp = powf(M, oc->p_coeff);
        }
        float denom = mp - 1.0f;
        if (denom < 1e-6f) denom = 1e-6f;
        float t_operate = time_dial * (oc->a_coeff / denom + oc->b_coeff);
        if (!(t_operate > 1e-6f)) t_operate = 1e-6f;
        oc->accumulator += dt / t_operate;
        if (oc->accumulator >= 1.0f) oc->accumulator = 1.0f;
    } else {
        float Mr = M;
        if (Mr < 0.0f) Mr = 0.0f;
        if (Mr >= 0.999f) Mr = 0.999f;
        float denom_r = 1.0f - Mr * Mr;
        if (denom_r < 1e-6f) denom_r = 1e-6f;
        float t_reset = time_dial * (4.6f / denom_r);
        if (!(t_reset > 1e-6f)) t_reset = 1e-6f;
        oc->accumulator -= dt / t_reset;
        if (oc->accumulator < 0.0f) oc->accumulator = 0.0f;
    }

    oc->tripped = (oc->accumulator >= 1.0f);
    le_pi_set_bool_fast(img, args[2], oc->tripped);
    return LE_OK;
}
#endif /* LE_ENABLE_PROTECTION */

#endif /* LE_EVAL_RELAY_H */

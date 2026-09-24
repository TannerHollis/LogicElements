/**
 * @file le_opcodes.c
 * @brief Implementation of opcode execution handlers.
 */

#include "le_opcodes.h"
#include "le_hal.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

le_status_t le_exec_instruction(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms)
{
    return le_exec_instruction_ex(inst, img, now_ms, NULL, 0);
}

le_status_t le_exec_instruction_ex(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms,
                                   const le_block_desc_t* blocks, uint16_t block_count)
{
    if (!inst || !img) return LE_ERR_NULL_PTR;

    uint8_t op = inst->opcode;
    uint8_t mod = inst->modifier;

    /* ---------------------------------------------------------------------- */
    /* Digital Logic Gates                                                    */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_MOVE && op <= LE_OP_MUX)
    {
        bool a = le_process_image_get_bool(img, inst->in_a);
        bool b = le_process_image_get_bool(img, inst->in_b);

        if (mod & LE_MOD_INVERT_A) a = !a;
        if (mod & LE_MOD_INVERT_B) b = !b;

        bool res = false;
        switch (op)
        {
            case LE_OP_MOVE: res = a; break;
            case LE_OP_NOT:  res = !a; break;
            case LE_OP_AND:  res = a && b; break;
            case LE_OP_OR:   res = a || b; break;
            case LE_OP_XOR:  res = a ^ b; break;
            case LE_OP_NAND: res = !(a && b); break;
            case LE_OP_NOR:  res = !(a || b); break;
            case LE_OP_MUX: {
                /* For MUX, in_a is input 0, in_b is input 1, and modifier can hold sel bit address or coil */
                res = a; /* fallback */
                break;
            }
            default: break;
        }

        if (mod & LE_MOD_INVERT_OUT) res = !res;
        le_process_image_set_bool(img, inst->out, res);
        return LE_OK;
    }

    /* ---------------------------------------------------------------------- */
    /* Edge Detection & Latches                                               */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_RTRIG && op <= LE_OP_RS)
    {
        switch (op)
        {
            case LE_OP_RTRIG: {
                /* in_a = input signal, in_b = history coil, out = pulse out */
                bool cur = le_process_image_get_bool(img, inst->in_a);
                bool prev = le_process_image_get_bool(img, inst->in_b);
                bool pulse = cur && !prev;
                le_process_image_set_bool(img, inst->out, pulse);
                le_process_image_set_bool(img, inst->in_b, cur);
                return LE_OK;
            }

            case LE_OP_FTRIG: {
                /* in_a = input signal, in_b = history coil, out = pulse out */
                bool cur = le_process_image_get_bool(img, inst->in_a);
                bool prev = le_process_image_get_bool(img, inst->in_b);
                bool pulse = !cur && prev;
                le_process_image_set_bool(img, inst->out, pulse);
                le_process_image_set_bool(img, inst->in_b, cur);
                return LE_OK;
            }

            case LE_OP_SR: {
                /* Set-dominant latch: in_a = Set, in_b = Reset, out = Q */
                bool s = le_process_image_get_bool(img, inst->in_a);
                bool r = le_process_image_get_bool(img, inst->in_b);
                bool q = le_process_image_get_bool(img, inst->out);
                if (s) {
                    q = true;
                } else if (r) {
                    q = false;
                }
                le_process_image_set_bool(img, inst->out, q);
                return LE_OK;
            }

            case LE_OP_RS: {
                /* Reset-dominant latch: in_a = Set, in_b = Reset, out = Q */
                bool s = le_process_image_get_bool(img, inst->in_a);
                bool r = le_process_image_get_bool(img, inst->in_b);
                bool q = le_process_image_get_bool(img, inst->out);
                if (r) {
                    q = false;
                } else if (s) {
                    q = true;
                }
                le_process_image_set_bool(img, inst->out, q);
                return LE_OK;
            }

            default: break;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* Timers                                                                 */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_TON && op <= LE_OP_TP)
    {
        /* The timer instance is identified by its state/output address in
         * inst->out (LE_REGION_TIMER | index), matching the compiler's
         * allocation. inst->in_a carries the enable signal. */
        uint16_t t_idx = inst->out & LE_ADDR_INDEX_MASK;
        le_timer_state_t* t = le_process_image_timer(img, t_idx);
        if (!t) return LE_ERR_OUT_OF_BOUNDS;

        bool in_val = le_process_image_get_bool(img, inst->in_a);

        switch (op)
        {
            case LE_OP_TON: {
                if (in_val) {
                    if (!t->prev_in) {
                        t->start_time_ms = now_ms;
                        t->q = false;
                    } else {
                        if (!t->q && (now_ms - t->start_time_ms >= t->preset_ms)) {
                            t->q = true;
                        }
                    }
                } else {
                    t->q = false;
                }
                t->prev_in = in_val;
                le_process_image_set_bool(img, inst->out, t->q);
                return LE_OK;
            }

            case LE_OP_TOF: {
                if (in_val) {
                    t->q = true;
                } else {
                    if (t->prev_in) {
                        t->start_time_ms = now_ms;
                    }
                    if (t->q && (now_ms - t->start_time_ms >= t->preset_ms)) {
                        t->q = false;
                    }
                }
                t->prev_in = in_val;
                le_process_image_set_bool(img, inst->out, t->q);
                return LE_OK;
            }

            case LE_OP_TP: {
                if (in_val && !t->prev_in && !t->q) {
                    t->start_time_ms = now_ms;
                    t->q = true;
                }
                if (t->q && (now_ms - t->start_time_ms >= t->preset_ms)) {
                    t->q = false;
                }
                t->prev_in = in_val;
                le_process_image_set_bool(img, inst->out, t->q);
                return LE_OK;
            }

            default: break;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* Counters (CTU, CTD, CTUD)                                             */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_CTU && op <= LE_OP_CTUD)
    {
        /* Counter instance is identified by its state/output address in
         * inst->out (LE_REGION_COUNTER | index), matching the compiler's
         * allocation. inst->in_a is the primary count trigger (cu for CTU /
         * cd for CTD / cu for CTUD); inst->in_b, when wired, is the reset
         * (CTU/CTD) or the count-down trigger (CTUD). */
        uint16_t c_idx = inst->out & LE_ADDR_INDEX_MASK;
        le_counter_state_t* c = le_process_image_counter(img, c_idx);
        if (!c) return LE_ERR_OUT_OF_BOUNDS;

        bool en = le_process_image_get_bool(img, inst->in_a);
        bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
        bool aux = has_aux ? le_process_image_get_bool(img, inst->in_b) : false;

        switch (op)
        {
            case LE_OP_CTU: {
                if (en && !c->prev_cu) c->count++;
                if (has_aux && aux && !c->prev_cd) c->count = 0;          /* reset on rising edge */
                if (c->count > c->preset) c->count = c->preset;           /* clamp at preset */
                c->qu = (c->count >= c->preset);
                c->prev_cu = en;
                c->prev_cd = aux;
                le_process_image_set_bool(img, inst->out, c->qu);
                return LE_OK;
            }
            case LE_OP_CTD: {
                if (en && !c->prev_cu) { if (c->count > 0) c->count--; }  /* count down, floor at 0 */
                if (has_aux && aux && !c->prev_cd) c->count = c->preset;  /* reset restores preset */
                c->qd = (c->count <= 0);
                c->prev_cu = en;
                c->prev_cd = aux;
                le_process_image_set_bool(img, inst->out, c->qd);
                return LE_OK;
            }
            case LE_OP_CTUD: {
                if (en && !c->prev_cu) c->count++;                         /* header is cu */
                if (has_aux && aux && !c->prev_cd) { if (c->count > 0) c->count--; }  /* in_b is cd */
                if (c->count > c->preset) c->count = c->preset;
                c->qu = (c->count >= c->preset);
                c->qd = (c->count <= 0);
                c->prev_cu = en;
                c->prev_cd = aux;
                le_process_image_set_bool(img, inst->out, c->qu);
                return LE_OK;
            }
            default: break;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* Float Arithmetic                                                       */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_MOVE_F && op <= LE_OP_SCALE_F)
    {
        float a = le_process_image_get_float(img, inst->in_a);
        float b = le_process_image_get_float(img, inst->in_b);
        float res = 0.0f;

        switch (op)
        {
            case LE_OP_MOVE_F: res = a; break;
            case LE_OP_ADD_F:  res = a + b; break;
            case LE_OP_SUB_F:  res = a - b; break;
            case LE_OP_MUL_F:  res = a * b; break;
            case LE_OP_DIV_F:  res = (fabsf(b) > 1e-9f) ? (a / b) : 0.0f; break;
            case LE_OP_NEG_F:  res = -a; break;
            case LE_OP_ABS_F:  res = fabsf(a); break;
            case LE_OP_MIN_F:  res = (a < b) ? a : b; break;
            case LE_OP_MAX_F:  res = (a > b) ? a : b; break;
            case LE_OP_SCALE_F: {
                uint8_t s_idx = inst->modifier & 0xFF;
                float in_val;
                uint16_t in_reg = inst->in_a & LE_ADDR_REGION_MASK;
                if (in_reg == LE_REGION_AIN) {
                    in_val = (float)le_process_image_get_int(img, inst->in_a);
                } else if (in_reg == LE_REGION_INT_REG) {
                    in_val = (float)le_process_image_get_int(img, inst->in_a);
                } else {
                    in_val = a;
                }

                {
                    le_scale_state_t* s = (le_scale_state_t*)le_process_image_kind_state(img, LE_BLK_SCALER, s_idx);
                    if (!s) return LE_ERR_OUT_OF_BOUNDS;
                    float span = s->raw_max - s->raw_min;
                    if (fabsf(span) > 1e-9f) {
                        float norm = (in_val - s->raw_min) / span;
                        res = s->scale_min + norm * (s->scale_max - s->scale_min);
                    } else {
                        res = s->scale_min;
                    }
                    if (s->clamp) {
                        float lower = (s->scale_min < s->scale_max) ? s->scale_min : s->scale_max;
                        float upper = (s->scale_min > s->scale_max) ? s->scale_min : s->scale_max;
                        if (res < lower) res = lower;
                        if (res > upper) res = upper;
                    }
                }
                break;
            }
            default: break;
        }

        le_process_image_set_float(img, inst->out, res);
        return LE_OK;
    }

#if LE_ENABLE_COMPLEX
/* Complex Arithmetic (T_CMPLX operands -> T_CMPLX out) */
    if (op >= LE_OP_CADD_F && op <= LE_OP_MOVE_C)
    {
        le_complex_t a = le_process_image_get_complex(img, inst->in_a);
        le_complex_t b = le_process_image_get_complex(img, inst->in_b);
        le_complex_t res = le_c_make(0.0f, 0.0f);

        switch (op)
        {
            case LE_OP_MOVE_C: res = a; break;
            case LE_OP_CADD_F: res = le_c_add(a, b); break;
            case LE_OP_CSUB_F: res = le_c_sub(a, b); break;
            case LE_OP_CMUL_F: res = le_c_mul(a, b); break;
            case LE_OP_CDIV_F: res = le_c_div(a, b); break;
            default: break;
        }

        le_process_image_set_complex(img, inst->out, res);
        return LE_OK;
    }
#endif
    /* ---------------------------------------------------------------------- */
    /* Float Comparisons (Float -> Boolean)                                   */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_CMP_GT && op <= LE_OP_CMP_NE)
    {
        float a = le_process_image_get_float(img, inst->in_a);
        float b = le_process_image_get_float(img, inst->in_b);
        bool res = false;

        switch (op)
        {
            case LE_OP_CMP_GT: res = (a > b); break;
            case LE_OP_CMP_LT: res = (a < b); break;
            case LE_OP_CMP_GE: res = (a >= b); break;
            case LE_OP_CMP_LE: res = (a <= b); break;
            case LE_OP_CMP_EQ: res = (fabsf(a - b) < 1e-6f); break;
            case LE_OP_CMP_NE: res = (fabsf(a - b) >= 1e-6f); break;
            default: break;
        }

        if (mod & LE_MOD_INVERT_OUT) res = !res;
        le_process_image_set_bool(img, inst->out, res);
        return LE_OK;
    }

    #if LE_ENABLE_PROTECTION
    /* ---------------------------------------------------------------------- */
    /* Control, Protection & Conversions                                      */
    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /* Protection: IEC/IEEE Inverse-Time Overcurrent (ANSI 51)                 */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_OVERCURRENT)
    {
        uint16_t oc_idx = inst->modifier & 0xFF;
        le_overcurrent_state_t* oc = (le_overcurrent_state_t*)le_process_image_kind_state(img, LE_BLK_OVERCURRENT, oc_idx);
        if (!oc) return LE_ERR_OUT_OF_BOUNDS;

        if (oc->pickup <= 0.0f) oc->pickup = 1.0f;       /* default 1.0 pu pickup */
        if (oc->time_dial <= 0.0f) oc->time_dial = 1.0f;

        float i = le_process_image_get_float(img, inst->in_a);
        float i_pu = i / oc->pickup;
        float dt = 0.01f;

        /* Simple integrating inverse-time accumulator: trip once the
         * overcurrent is sustained past the time dial. */
        if (i_pu > 1.0f) {
            oc->accumulator += dt * (i_pu - 1.0f);
        } else {
            oc->accumulator -= dt;
            if (oc->accumulator < 0.0f) oc->accumulator = 0.0f;
        }
        oc->tripped = (oc->accumulator >= oc->time_dial);
        le_process_image_set_bool(img, inst->out, oc->tripped);
        return LE_OK;
    }

    if (op == LE_OP_RECT2POLAR)
    {
        /* in_a = real (x), in_b = imag (y), out = magnitude */
        float x = le_process_image_get_float(img, inst->in_a);
        float y = le_process_image_get_float(img, inst->in_b);
        float mag = sqrtf(x * x + y * y);
        le_process_image_set_float(img, inst->out, mag);
        return LE_OK;
    }

    if (op == LE_OP_POLAR2RECT)
    {
        /* in_a = mag, in_b = angle (rad), out = real (x) */
        float mag = le_process_image_get_float(img, inst->in_a);
        float angle = le_process_image_get_float(img, inst->in_b);
        float x = mag * cosf(angle);
        le_process_image_set_float(img, inst->out, x);
        return LE_OK;
    }

    if (op == LE_OP_PHASOR_SHIFT)
    {
        /* Rotates the phasor (re, im) counter-clockwise by the angle encoded in
         * the modifier byte (0..255 degrees) and writes the rotated phasor's
         * real component to out. in_a = real(x), in_b = imag(y). */
        float x = le_process_image_get_float(img, inst->in_a);
        float y = le_process_image_get_float(img, inst->in_b);
        float delta_rad = (float)(mod & 0xFF) * (float)M_PI / 180.0f;
        float cs = cosf(delta_rad);
        float sn = sinf(delta_rad);
        float xr = x * cs - y * sn;
        le_process_image_set_float(img, inst->out, xr);
        return LE_OK;
    }

    if (op == LE_OP_PID)
    {
        uint16_t pid_idx = inst->modifier & 0xFF;

        le_pid_state_t* p = (le_pid_state_t*)le_process_image_kind_state(img, LE_BLK_PID, pid_idx);
        if (!p) return LE_ERR_OUT_OF_BOUNDS;
        float sp = le_process_image_get_float(img, inst->in_a);
        float pv = le_process_image_get_float(img, inst->in_b);
        float error = sp - pv;

        float dt = (now_ms - p->last_time_ms) * 0.001f;
        if (dt <= 0.0f || dt > 1.0f) dt = 0.01f; /* default 10ms */
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

        le_process_image_set_float(img, inst->out, output);
        return LE_OK;
    }

    /* ---------------------------------------------------------------------- */
    /* Protection: 1P Phasor Extraction (DFT / Cosine Filter)                 */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_PHASOR_1P)
    {
        uint16_t p_idx = inst->modifier & 0xFF;

        le_phasor_state_t* p = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, p_idx);
        if (!p) return LE_ERR_OUT_OF_BOUNDS;
        if (p->samples_per_cycle == 0 || p->samples_per_cycle > LE_MAX_SAMPLES_PER_CYCLE) {
            p->samples_per_cycle = 16; /* Default 16 samples per cycle (960 Hz @ 60Hz) */
        }

        /* Read raw instantaneous sample */
        float raw = le_process_image_get_float(img, inst->in_a);
        p->samples[p->write_idx] = raw;
        p->write_idx = (p->write_idx + 1) % p->samples_per_cycle;

        /* Full-cycle Discrete Fourier Transform */
        uint16_t N = p->samples_per_cycle;
        float sum_cos = 0.0f;
        float sum_sin = 0.0f;
        float angle_step = 2.0f * (float)M_PI / (float)N;

        for (uint16_t k = 0; k < N; k++)
        {
            uint16_t s_idx = (p->write_idx + k) % N;
            float angle = angle_step * (float)k;
            float s_val = p->samples[s_idx];
            sum_cos += s_val * cosf(angle);
            sum_sin -= s_val * sinf(angle);
        }

        float real = (2.0f / (float)N) * sum_cos;
        float imag = (2.0f / (float)N) * sum_sin;
        p->phasor = le_c_make(real, imag);
        p->magnitude = le_c_mag(p->phasor);
        p->angle_rad = le_c_ang(p->phasor);

        /* Optional reference angle adjustment */
        if (inst->in_b != LE_ADDR_UNUSED) {
            float ref_angle = le_process_image_get_float(img, inst->in_b);
            p->angle_rad -= ref_angle;
            while (p->angle_rad > (float)M_PI) p->angle_rad -= 2.0f * (float)M_PI;
            while (p->angle_rad < -(float)M_PI) p->angle_rad += 2.0f * (float)M_PI;
            p->phasor = le_c_polar(p->magnitude, p->angle_rad);
        }

        le_process_image_set_float(img, inst->out, p->magnitude);
        return LE_OK;
    }

    /* ---------------------------------------------------------------------- */
    /* Protection: 3-Phase Symmetrical Components                             */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_SYM_COMP)
    {
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
        le_process_image_set_float(img, inst->out, le_c_mag(sc->seq_1));
        return LE_OK;
    }

    /* ---------------------------------------------------------------------- */
    /* Protection: Mho Distance Relaying (21) is a variable-arity block        */
    /* (LE_FUNC_DIST_21). See the LE_OP_BLOCK dispatch below.                  */
    /* ---------------------------------------------------------------------- */
#endif

#if LE_ENABLE_SERIAL_BUS
    /* ---------------------------------------------------------------------- */
    /* Serial Bus: I2C Master Transaction Block                               */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_I2C)
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
                    if ((dev->data_dest_addr & LE_ADDR_REGION_MASK) >= LE_REGION_FLOAT &&
                        (dev->data_dest_addr & LE_ADDR_REGION_MASK) <= LE_REGION_FLOAT_EXT3)
                    {
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

    /* ---------------------------------------------------------------------- */
    /* Serial Bus: SPI Master Transaction Block                               */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_SPI)
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
            if (g_le_hal && g_le_hal->spi_transfer && dev->poll_len > 0) {
                success = g_le_hal->spi_transfer(dev->cs_pin, dev->poll_tx_data, dev->rx_buf, dev->poll_len);
            }

            if (success) {
                dev->poll_count++;
                dev->last_success = true;

                /* Store received data in process image if destination configured */
                if (dev->data_dest_addr != LE_ADDR_UNUSED) {
                    if ((dev->data_dest_addr & LE_ADDR_REGION_MASK) >= LE_REGION_FLOAT &&
                        (dev->data_dest_addr & LE_ADDR_REGION_MASK) <= LE_REGION_FLOAT_EXT3)
                    {
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

        /* 4. Output status / strobe */
        if (inst->out != LE_ADDR_UNUSED) {
            le_process_image_set_bool(img, inst->out, do_poll ? success : dev->last_success);
        }
        return LE_OK;
    }
#endif

#if LE_ENABLE_DSP
    /* ---------------------------------------------------------------------- */
    /* Digital Signal Processing (DSP) & Filters                              */
    /* ---------------------------------------------------------------------- */
    if (op >= LE_OP_LPF_1P && op <= LE_OP_MIN_MAX_HOLD)
    {
        uint8_t idx = mod & 0xFF;

        float in_x = le_process_image_get_float(img, inst->in_a);
        float out_y = 0.0f;

        switch (op)
        {
            case LE_OP_LPF_1P: {
                le_lpf_state_t* f = (le_lpf_state_t*)le_process_image_kind_state(img, LE_BLK_LPF, idx);
                if (!f) return LE_ERR_OUT_OF_BOUNDS;
                float alpha = f->alpha;
                if (inst->in_b != LE_ADDR_UNUSED) {
                    float dyn_alpha = le_process_image_get_float(img, inst->in_b);
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
                break;
            }

            case LE_OP_BIQUAD_IIR: {
                le_biquad_state_t* b = (le_biquad_state_t*)le_process_image_kind_state(img, LE_BLK_BIQUAD, idx);
                if (!b) return LE_ERR_OUT_OF_BOUNDS;
                /* Direct Form II Canonical Form */
                float w = in_x - (b->a1 * b->w1) - (b->a2 * b->w2);
                out_y = (b->b0 * w) + (b->b1 * b->w1) + (b->b2 * b->w2);
                b->w2 = b->w1;
                b->w1 = w;
                b->initialized = true;
                break;
            }

            case LE_OP_MOVING_AVG: {
                le_moving_avg_state_t* m = (le_moving_avg_state_t*)le_process_image_kind_state(img, LE_BLK_MOVING_AVG, idx);
                if (!m) return LE_ERR_OUT_OF_BOUNDS;
                uint16_t wsz = (m->window_size > 0 && m->window_size <= LE_MOVING_AVG_MAX_WINDOW) ? m->window_size : 8;
                if (m->count < wsz) {
                    m->buffer[m->write_idx] = in_x;
                    m->sum += in_x;
                    m->count++;
                    m->write_idx = (m->write_idx + 1) % wsz;
                    out_y = m->sum / (float)m->count;
                } else {
                    m->sum = m->sum - m->buffer[m->write_idx] + in_x;
                    m->buffer[m->write_idx] = in_x;
                    m->write_idx = (m->write_idx + 1) % wsz;
                    out_y = m->sum / (float)wsz;
                }
                break;
            }

            case LE_OP_RATE_LIMITER: {
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
                break;
            }

            case LE_OP_DEADBAND: {
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
                break;
            }

            case LE_OP_WASHOUT: {
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
                break;
            }

            case LE_OP_PEAK_DETECTOR: {
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
                break;
            }

            case LE_OP_RMS: {
                le_rms_state_t* rms = (le_rms_state_t*)le_process_image_kind_state(img, LE_BLK_RMS, idx);
                if (!rms) return LE_ERR_OUT_OF_BOUNDS;
                float sq = in_x * in_x;
                uint16_t wsz = (rms->window_size > 0 && rms->window_size <= LE_RMS_MAX_WINDOW) ? rms->window_size : 16;
                if (rms->count < wsz) {
                    rms->buffer[rms->write_idx] = sq;
                    rms->sum_sq += sq;
                    rms->count++;
                    rms->write_idx = (rms->write_idx + 1) % wsz;
                    out_y = sqrtf(rms->sum_sq / (float)rms->count);
                } else {
                    rms->sum_sq = rms->sum_sq - rms->buffer[rms->write_idx] + sq;
                    if (rms->sum_sq < 0.0f) rms->sum_sq = 0.0f;
                    rms->buffer[rms->write_idx] = sq;
                    rms->write_idx = (rms->write_idx + 1) % wsz;
                    out_y = sqrtf(rms->sum_sq / (float)wsz);
                }
                break;
            }

            case LE_OP_MEDIAN: {
                le_median_state_t* m = (le_median_state_t*)le_process_image_kind_state(img, LE_BLK_MEDIAN, idx);
                if (!m) return LE_ERR_OUT_OF_BOUNDS;
                uint16_t wsz = (m->window_size > 0 && m->window_size <= LE_MAX_MEDIAN_WINDOW) ? m->window_size : 5;
                m->buffer[m->write_idx] = in_x;
                m->write_idx = (m->write_idx + 1) % wsz;
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
                break;
            }

            case LE_OP_DERIVATIVE: {
                le_derivative_state_t* d = (le_derivative_state_t*)le_process_image_kind_state(img, LE_BLK_DERIVATIVE, idx);
                if (!d) return LE_ERR_OUT_OF_BOUNDS;
                float alpha = d->alpha;
                if (inst->in_b != LE_ADDR_UNUSED) {
                    float dyn_alpha = le_process_image_get_float(img, inst->in_b);
                    if (dyn_alpha >= 0.0f && dyn_alpha <= 1.0f) alpha = dyn_alpha;
                }
                if (!d->initialized) {
                    d->prev_x = in_x;
                    d->prev_y = 0.0f;
                    d->initialized = true;
                    out_y = 0.0f;
                } else {
                    float dx = in_x - d->prev_x;
                    float raw_deriv = dx * d->gain;
                    out_y = d->prev_y + alpha * (raw_deriv - d->prev_y);
                    d->prev_x = in_x;
                    d->prev_y = out_y;
                }
                break;
            }

            case LE_OP_ZERO_CROSSING: {
                le_zero_crossing_state_t* zc = (le_zero_crossing_state_t*)le_process_image_kind_state(img, LE_BLK_ZERO_CROSSING, idx);
                if (!zc) return LE_ERR_OUT_OF_BOUNDS;
                if (inst->in_b != LE_ADDR_UNUSED && le_process_image_get_bool(img, inst->in_b)) {
                    zc->frequency_hz = 0.0f;
                    zc->samples_since_cross = 0;
                    zc->last_state = 0;
                } else {
                    zc->samples_since_cross++;
                    int8_t current_state = zc->last_state;
                    if (in_x > zc->hysteresis) {
                        current_state = 1;
                    } else if (in_x < -zc->hysteresis) {
                        current_state = -1;
                    }

                    if (zc->last_state == -1 && current_state == 1) {
                        if (zc->samples_since_cross > 0 && zc->sample_rate_hz > 0.0f) {
                            zc->frequency_hz = zc->sample_rate_hz / (float)zc->samples_since_cross;
                        }
                        zc->samples_since_cross = 0;
                    }

                    if (zc->sample_rate_hz > 0.0f && zc->samples_since_cross > (uint32_t)(zc->sample_rate_hz * 2.0f)) {
                        zc->frequency_hz = 0.0f;
                    }

                    zc->last_state = current_state;
                }
                out_y = zc->frequency_hz;
                break;
            }

            case LE_OP_LUT_1D: {
                const le_lut_1d_state_t* lut = (le_lut_1d_state_t*)le_process_image_kind_state(img, LE_BLK_LUT_1D, idx);
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
                break;
            }

            case LE_OP_TOTALIZER: {
                le_totalizer_state_t* tot = (le_totalizer_state_t*)le_process_image_kind_state(img, LE_BLK_TOTALIZER, idx);
                if (!tot) return LE_ERR_OUT_OF_BOUNDS;
                bool reset = false;
                if (inst->in_b != LE_ADDR_UNUSED) {
                    reset = le_process_image_get_bool(img, inst->in_b);
                }
                if (reset) {
                    tot->accumulator = 0.0;
                    tot->prev_x = in_x;
                    tot->initialized = true;
                } else if (!tot->initialized) {
                    tot->prev_x = in_x;
                    tot->initialized = true;
                } else {
                    double dt = (double)tot->sample_time_sec;
                    double tb = (double)tot->time_base_sec;
                    if (tb <= 0.0) tb = 1.0;
                    double avg_rate = 0.5 * ((double)in_x + (double)tot->prev_x);
                    double delta = (avg_rate * (dt / tb)) * (double)tot->scale_factor;
                    tot->accumulator += delta;
                    if (tot->max_limit > 0.0f && tot->accumulator > (double)tot->max_limit) {
                        tot->accumulator = (double)tot->max_limit;
                    }
                    tot->prev_x = in_x;
                }
                out_y = (float)tot->accumulator;
                break;
            }

            case LE_OP_MIN_MAX_HOLD: {
                le_min_max_hold_state_t* h = (le_min_max_hold_state_t*)le_process_image_kind_state(img, LE_BLK_MIN_MAX_HOLD, idx);
                if (!h) return LE_ERR_OUT_OF_BOUNDS;
                bool reset = false;
                if (inst->in_b != LE_ADDR_UNUSED) {
                    reset = le_process_image_get_bool(img, inst->in_b);
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
                break;
            }

            default:
                return LE_ERR_UNKNOWN_OPCODE;
        }

        le_process_image_set_float(img, inst->out, out_y);
        return LE_OK;
    }
#endif

    /* ---------------------------------------------------------------------- */
    /* External Hardware / Board Custom Node Call                             */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_EXT_CALL)
    {
        uint8_t func_id = mod; /* function ID passed in modifier */
        if (g_le_hal && g_le_hal->ext_call)
        {
            /* Legacy two-operand call: extract the 2-in/1-out operands. */
            const uint16_t args[3] = { inst->in_a, inst->in_b, inst->out };
            return g_le_hal->ext_call(func_id, args, 2, 1, img);
        }
        /* Default fallback: NOP if no board handler registered */
        return LE_OK;
    }

    /* ---------------------------------------------------------------------- */
    /* Variable-arity block call (MUX builtin, N-in/M-out custom nodes)        */
    /* ---------------------------------------------------------------------- */
    if (op == LE_OP_BLOCK)
    {
        if (inst->in_a >= block_count || !blocks) {
            return LE_ERR_OUT_OF_BOUNDS;
        }

        /* Block descriptors are variable-size (4-byte header + in/out arg
         * addresses appended after each header). Walk to the requested index
         * rather than indexing with a fixed stride. */
        const le_block_desc_t* desc = blocks;
        for (uint16_t w = 0; w < inst->in_a; w++) {
            uint16_t wc = (uint16_t)desc->in_count + (uint16_t)desc->out_count;
            desc = (const le_block_desc_t*)((const uint8_t*)desc + LE_BLOCK_DESC_HEADER_BYTES +
                                            (size_t)wc * sizeof(uint16_t));
        }
        uint16_t argc = (uint16_t)desc->in_count + (uint16_t)desc->out_count;
        const uint16_t* args = (const uint16_t*)(desc + 1); /* args follow the header */

        if (mod < LE_FUNC_CUSTOM_BASE) {
            /* Builtin function dispatch. */
            switch (mod) {
                case LE_FUNC_MUX_SELECT: {
                    /* args = [sel, in0, in1, out] (3-in/1-out) */
                    if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    bool sel = le_process_image_get_bool(img, args[0]);
                    bool in0 = le_process_image_get_bool(img, args[1]);
                    bool in1 = le_process_image_get_bool(img, args[2]);
                    le_process_image_set_bool(img, args[3], sel ? in1 : in0);
                    return LE_OK;
                }

                #if LE_ENABLE_PROTECTION
case LE_FUNC_RECT2POLAR: {
                    /* args = [real, imag, out_mag, out_angle_rad] (2-in/2-out) */
                    if (desc->in_count < 2 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                    float x = le_process_image_get_float(img, args[0]);
                    float y = le_process_image_get_float(img, args[1]);
                    float mag = sqrtf(x * x + y * y);
                    float ang = atan2f(y, x);
                    le_process_image_set_float(img, args[2], mag);
                    le_process_image_set_float(img, args[3], ang);
                    return LE_OK;
                }
#endif


                #if LE_ENABLE_PROTECTION
case LE_FUNC_POLAR2RECT: {
                    /* args = [mag, angle_rad, out_real, out_imag] (2-in/2-out) */
                    if (desc->in_count < 2 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                    float mag = le_process_image_get_float(img, args[0]);
                    float ang = le_process_image_get_float(img, args[1]);
                    le_process_image_set_float(img, args[2], mag * cosf(ang));
                    le_process_image_set_float(img, args[3], mag * sinf(ang));
                    return LE_OK;
                }
#endif


                #if LE_ENABLE_PROTECTION
case LE_FUNC_COMPLEX2POLAR: {
                    /* complex in -> {mag, angle} (1-in/2-out) */
                    if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                    le_complex_t c = le_process_image_get_complex(img, args[0]);
                    le_process_image_set_float(img, args[1], le_c_mag(c));
                    le_process_image_set_float(img, args[2], le_c_ang(c));
                    return LE_OK;
                }
#endif


                #if LE_ENABLE_PROTECTION
case LE_FUNC_COMPLEX2RECT: {
                    /* complex in -> {real, imag} (1-in/2-out) */
                    if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                    le_complex_t c = le_process_image_get_complex(img, args[0]);
                    le_process_image_set_float(img, args[1], c.r);
                    le_process_image_set_float(img, args[2], c.i);
                    return LE_OK;
                }
#endif


                #if LE_ENABLE_PROTECTION
case LE_FUNC_RECT2COMPLEX: {
                    /* {real, imag} floats -> complex out (2-in/1-out) */
                    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    float re = le_process_image_get_float(img, args[0]);
                    float im = le_process_image_get_float(img, args[1]);
                    le_process_image_set_complex(img, args[2], le_c_make(re, im));
                    return LE_OK;
                }
#endif


                #if LE_ENABLE_PROTECTION
case LE_FUNC_POLAR2COMPLEX: {
                    /* {mag, angle} floats -> complex out (2-in/1-out) */
                    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    float mag = le_process_image_get_float(img, args[0]);
                    float ang = le_process_image_get_float(img, args[1]);
                    le_process_image_set_complex(img, args[2], le_c_polar(mag, ang));
                    return LE_OK;
                }
#endif


                case LE_FUNC_CLAMP_F: {
                    /* [value, min, max] -> [out] (3-in/1-out) */
                    if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    float v = le_process_image_get_float(img, args[0]);
                    float mn = le_process_image_get_float(img, args[1]);
                    float mx = le_process_image_get_float(img, args[2]);
                    float lo = (mn < mx) ? mn : mx;   /* robust to swapped bounds */
                    float hi = (mn < mx) ? mx : mn;
                    float r = (v < lo) ? lo : ((v > hi) ? hi : v);
                    le_process_image_set_float(img, args[3], r);
                    return LE_OK;
                }
                #if LE_ENABLE_PROTECTION
case LE_FUNC_PHASE_COMP: {
                    /* 3-phase transformer phase-shift compensation (ANSI 87T).
                     * args = [c_a, c_b, c_c, out_a, out_b, out_c]  (3-in/3-out).
                     * Applies the SEL delta/wye compensation matrix M(k) (k = comp,
                     * 1..12, baked in le_comp33_state_t) to the three winding phasors:
                     *   I'_x = s * sum_j M[k][x][j] * I_j
                     * with scalar s = 1/sqrt(3) for odd k, 1/3 for even k. This
                     * cancels the winding phase displacement so operate = 0 under
                     * through-load. */
                    if (desc->in_count < 3 || desc->out_count < 3) return LE_ERR_OUT_OF_BOUNDS;
                    le_comp33_state_t* c33 = (le_comp33_state_t*)le_process_image_kind_state(img, LE_BLK_PHASE_COMP, inst->in_a & 0xFF);
                    if (!c33) return LE_ERR_OUT_OF_BOUNDS;
                    int k = (int)c33->comp;
                    if (k < 1) k = 1;
                    if (k > 12) k = 12;
                    /* scalar multiplier: odd k -> 1/sqrt(3), even k -> 1/3 */
                    float s = (k & 1) ? (1.0f / sqrtf(3.0f)) : (1.0f / 3.0f);

                    le_complex_t ia = le_process_image_get_complex(img, args[0]);
                    le_complex_t ib = le_process_image_get_complex(img, args[1]);
                    le_complex_t ic = le_process_image_get_complex(img, args[2]);

                    /* row-major 3x3 matrix for index k */
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
                    le_process_image_set_complex(img, args[3], oa);
                    le_process_image_set_complex(img, args[4], ob);
                    le_process_image_set_complex(img, args[5], oc);
                    return LE_OK;
                }
#endif

                #if LE_ENABLE_PROTECTION
case LE_FUNC_DIFF_87: {
                    /* N-complex dual-slope differential protection (ANSI 87).
                     * args = [cph0, cph1, ..., cphN-1, out_bool]  (N-in/1-out)
                     * Operate current = |vector sum of all phasors|; restraint
                     * current = SUM of phasor magnitudes (per large-bus differential
                     * convention, no averaging). Trips when operate exceeds the
                     * dual-slope threshold from the baked le_diff87_state_t:
                     *   th = o87p + slp1*I_rt                 for I_rt <= irs1
                     *   th = o87p + slp1*irs1 + slp2*(I_rt-irs1) for I_rt > irs1  */
                    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    int cin = desc->in_count;
                    le_diff87_state_t* d87 = (le_diff87_state_t*)le_process_image_kind_state(img, LE_BLK_DIFF_87, inst->in_a & 0xFF);
                    if (!d87) return LE_ERR_OUT_OF_BOUNDS;
                    le_complex_t op_sum = le_c_make(0.0f, 0.0f);
                    float rt_sum = 0.0f;
                    for (int k = 0; k < cin; k++) {
                        le_complex_t c = le_process_image_get_complex(img, args[k]);
                        op_sum = le_c_add(op_sum, c);
                        rt_sum += le_c_mag(c);
                    }
                    float i_op = le_c_mag(op_sum);
                    float i_rt = rt_sum;   /* restraint = sum of magnitudes (no averaging) */
                    d87->operate = i_op;
                    d87->restraint = i_rt;

                    /* SEL-style dual-slope percentage threshold (O87P + SLP1/SLP2). */
                    float o87p = (d87->o87p > 0.0f) ? d87->o87p : 0.3f;
                    float slp1 = (d87->slp1 > 0.0f) ? d87->slp1 : 0.25f;
                    float irs1 = d87->irs1;
                    float slp2 = (d87->slp2 > 0.0f) ? d87->slp2 : 0.60f;
                    float trip_threshold = o87p;
                    if (i_rt <= irs1) trip_threshold += slp1 * i_rt;
                    else trip_threshold += slp1 * irs1 + slp2 * (i_rt - irs1);

                    d87->tripped = (i_op > trip_threshold);
                    le_process_image_set_bool(img, args[cin], d87->tripped);
                    return LE_OK;
                }
#endif

                #if LE_ENABLE_PROTECTION
case LE_FUNC_DIST_21: {
                    /* Mho distance (21) block with prefault voltage memory.
                     * args = [v_c, i_c, offset_on, out_bool] (3-in/1-out)
                     * v_c, i_c are complex phasors; offset_on is a boolean that gates
                     * the mho-offset circle shift. Computes Z = V/I and trips when Z is
                     * inside the offset-mho circle (center reach/2 at line angle, shifted
                     * by the offset phasor; radius reach/2). */
                    if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    uint16_t d_idx = (uint16_t)(inst->in_a & 0xFF);
                    le_dist21_state_t* dist = (le_dist21_state_t*)le_process_image_kind_state(img, LE_BLK_21, d_idx);
                    if (!dist) return LE_ERR_OUT_OF_BOUNDS;

                    le_complex_t v = le_process_image_get_complex(img, args[0]);
                    le_complex_t i = le_process_image_get_complex(img, args[1]);
                    bool offset_on = le_process_image_get_bool(img, args[2]);

                    /* Prefault voltage memory. */
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
                        le_process_image_set_bool(img, args[3], false);
                        return LE_OK;
                    }

                    le_complex_t z = le_c_div(v_use, i);
                    dist->r_meas = z.r;
                    dist->x_meas = z.i;

                    float la = dist->line_angle_deg * (float)M_PI / 180.0f;
                    float oc = 0.0f, os = 0.0f;
                    if (offset_on && dist->offset_mag > 0.0f) {
                        float oa = dist->offset_angle_deg * (float)M_PI / 180.0f;
                        oc = dist->offset_mag * cosf(oa);
                        os = dist->offset_mag * sinf(oa);
                    }

                    float r_center = (dist->reach_ohms * 0.5f) * cosf(la) + oc;
                    float x_center = (dist->reach_ohms * 0.5f) * sinf(la) + os;
                    float radius   = dist->reach_ohms * 0.5f;
                    float dr = dist->r_meas - r_center;
                    float dx = dist->x_meas - x_center;
                    dist->tripped = ((dr * dr + dx * dx) <= (radius * radius));
                    le_process_image_set_bool(img, args[3], dist->tripped);
                    return LE_OK;
                }
#endif

                #if LE_ENABLE_PROTECTION
case LE_FUNC_PHASOR_SHIFT: {
                    /* args = [real, imag, delta_rad, out_real, out_imag] (3-in/2-out) */
                    if (desc->in_count < 3 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                    float x = le_process_image_get_float(img, args[0]);
                    float y = le_process_image_get_float(img, args[1]);
                    float delta = le_process_image_get_float(img, args[2]);
                    float cs = cosf(delta);
                    float sn = sinf(delta);
                    le_process_image_set_float(img, args[3], x * cs - y * sn);
                    le_process_image_set_float(img, args[4], x * sn + y * cs);
                    return LE_OK;
                }
#endif


#if LE_ENABLE_PROTECTION
                case LE_FUNC_PHASOR_1P: {
                    /* Synced phasor extractor (emits a complex phasor).
                     * args = [sample, sync_complex, out_complex] (2-in/1-out)
                     * The phasor instance is identified by the descriptor index. The raw
                     * phasor is synchronized to the sync phasor: its angle is referenced to
                     * the sync angle and its magnitude normalized by the sync magnitude, so
                     * the result stays stable relative to the sync phasor instead of
                     * rotating with the system frequency. */
                    if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                    uint16_t p_idx = inst->in_a;
                    le_phasor_state_t* p = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, p_idx);
                    if (!p) return LE_ERR_OUT_OF_BOUNDS;
                    if (p->samples_per_cycle == 0 || p->samples_per_cycle > LE_MAX_SAMPLES_PER_CYCLE) {
                        p->samples_per_cycle = 16;
                    }
                    float sample = le_process_image_get_float(img, args[0]);
                    p->samples[p->write_idx] = sample;
                    p->write_idx = (p->write_idx + 1) % p->samples_per_cycle;

                    uint16_t N = p->samples_per_cycle;
                    float sum_cos = 0.0f, sum_sin = 0.0f;
                    float angle_step = 2.0f * (float)M_PI / (float)N;
                    for (uint16_t k = 0; k < N; k++) {
                        uint16_t s_idx = (p->write_idx + k) % N;
                        float a = angle_step * (float)k;
                        float s_val = p->samples[s_idx];
                        sum_cos += s_val * cosf(a);
                        sum_sin -= s_val * sinf(a);
                    }
                    float real = (2.0f / (float)N) * sum_cos;
                    float imag = (2.0f / (float)N) * sum_sin;
                    le_complex_t ph = le_c_make(real, imag);

                    /* Synchronize to the reference phasor (complex). */
                    le_complex_t sync = le_process_image_get_complex(img, args[1]);
                    float sync_mag = le_c_mag(sync);
                    float sync_ang = le_c_ang(sync);
                    float mag = le_c_mag(ph);
                    float ang = le_c_ang(ph) - sync_ang;
                    while (ang > (float)M_PI) ang -= 2.0f * (float)M_PI;
                    while (ang < -(float)M_PI) ang += 2.0f * (float)M_PI;
                    if (fabsf(sync_mag) > 1e-6f) mag = (mag / sync_mag);

                    le_complex_t out = le_c_polar(mag, ang);
                    p->magnitude = mag;
                    p->angle_rad = ang;
                    p->phasor = out;
                    le_process_image_set_complex(img, args[2], out);
                    return LE_OK;
                }
#endif

                default:
                    return LE_ERR_UNKNOWN_OPCODE;
            }
        }

        /* Custom node: hand the whole operand list to the board HAL. */
        if (g_le_hal && g_le_hal->ext_call) {
            return g_le_hal->ext_call(mod, args, desc->in_count, desc->out_count, img);
        }
        return LE_OK; /* no board handler registered */
    }

    if (op == LE_OP_NOP)
    {
        return LE_OK;
    }

    return LE_ERR_UNKNOWN_OPCODE;
}

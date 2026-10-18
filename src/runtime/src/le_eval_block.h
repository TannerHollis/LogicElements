/**
 * @file le_eval_block.h
 * @brief Inline evaluation handlers for variable-arity blocks (LE_OP_BLOCK).
 */

#ifndef LE_EVAL_BLOCK_H
#define LE_EVAL_BLOCK_H

#include "le_types.h"
#include "le_process_image.h"
#include "le_eval_relay.h"
#include "le_hal.h"
#include "le_custom_node.h"

static inline le_status_t le_eval_block(uint8_t func_id,
                                        const le_block_desc_t* desc,
                                        const uint16_t* args,
                                        le_process_image_t* img,
                                        uint32_t now_ms,
                                        float scan_dt,
                                        float scan_srate)
{
    if (func_id < LE_FUNC_CUSTOM_BASE) {
        switch (func_id) {
            case LE_FUNC_MUX_SELECT: {
                /* args = [sel, in0, in1, out] (3-in/1-out, boolean) */
                if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                bool sel = le_pi_get_bool_fast(img, args[0]);
                bool in0 = le_pi_get_bool_fast(img, args[1]);
                bool in1 = le_pi_get_bool_fast(img, args[2]);
                le_pi_set_bool_fast(img, args[3], sel ? in1 : in0);
                return LE_OK;
            }

            case LE_FUNC_MUX_SELECT_F: {
                /* args = [sel, in0, in1, out] (3-in/1-out, float) */
                if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                bool sel = le_pi_get_bool_fast(img, args[0]);
                float in0 = le_pi_get_float_fast(img, args[1]);
                float in1 = le_pi_get_float_fast(img, args[2]);
                le_pi_set_float_fast(img, args[3], sel ? in1 : in0);
                return LE_OK;
            }

            case LE_FUNC_MUX_SELECT_I: {
                /* args = [sel, in0, in1, out] (3-in/1-out, int32) */
                if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                bool sel = le_pi_get_bool_fast(img, args[0]);
                int32_t in0 = le_pi_get_int_fast(img, args[1]);
                int32_t in1 = le_pi_get_int_fast(img, args[2]);
                le_pi_set_int_fast(img, args[3], sel ? in1 : in0);
                return LE_OK;
            }

#if LE_ENABLE_COMPLEX
            case LE_FUNC_MUX_SELECT_C: {
                /* args = [sel, in0, in1, out] (3-in/1-out, complex) */
                if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                bool sel = le_pi_get_bool_fast(img, args[0]);
                le_complex_t in0 = le_pi_get_complex_fast(img, args[1]);
                le_complex_t in1 = le_pi_get_complex_fast(img, args[2]);
                le_pi_set_complex_fast(img, args[3], sel ? in1 : in0);
                return LE_OK;
            }

            case LE_FUNC_RECT2POLAR: {
                /* args = [real, imag, out_mag, out_angle_rad] (2-in/2-out) */
                if (desc->in_count < 2 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                float x = le_pi_get_float_fast(img, args[0]);
                float y = le_pi_get_float_fast(img, args[1]);
                float mag = le_fast_cmplx_mag(x, y);
                float ang = le_fast_atan2(y, x);
                le_pi_set_float_fast(img, args[2], mag);
                le_pi_set_float_fast(img, args[3], ang);
                return LE_OK;
            }

            case LE_FUNC_POLAR2RECT: {
                /* args = [mag, angle_rad, out_real, out_imag] (2-in/2-out) */
                if (desc->in_count < 2 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                float mag = le_pi_get_float_fast(img, args[0]);
                float ang = le_pi_get_float_fast(img, args[1]);
                float s, c;
                le_sincos(ang, &s, &c);
                le_pi_set_float_fast(img, args[2], mag * c);
                le_pi_set_float_fast(img, args[3], mag * s);
                return LE_OK;
            }

            case LE_FUNC_COMPLEX2POLAR: {
                /* complex in -> {mag, angle} (1-in/2-out) */
                if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                le_complex_t c = le_pi_get_complex_fast(img, args[0]);
                le_pi_set_float_fast(img, args[1], le_c_mag(c));
                le_pi_set_float_fast(img, args[2], le_c_ang(c));
                return LE_OK;
            }

            case LE_FUNC_COMPLEX2RECT: {
                /* complex in -> {real, imag} (1-in/2-out) */
                if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                le_complex_t c = le_pi_get_complex_fast(img, args[0]);
                le_pi_set_float_fast(img, args[1], c.r);
                le_pi_set_float_fast(img, args[2], c.i);
                return LE_OK;
            }

            case LE_FUNC_RECT2COMPLEX: {
                /* {real, imag} floats -> complex out (2-in/1-out) */
                if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                float re = le_pi_get_float_fast(img, args[0]);
                float im = le_pi_get_float_fast(img, args[1]);
                le_pi_set_complex_fast(img, args[2], le_c_make(re, im));
                return LE_OK;
            }

            case LE_FUNC_POLAR2COMPLEX: {
                /* {mag, angle} floats -> complex out (2-in/1-out) */
                if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                float mag = le_pi_get_float_fast(img, args[0]);
                float ang = le_pi_get_float_fast(img, args[1]);
                le_pi_set_complex_fast(img, args[2], le_c_polar(mag, ang));
                return LE_OK;
            }

            case LE_FUNC_PHASOR_SHIFT: {
                /* args = [real, imag, delta_rad, out_real, out_imag] (3-in/2-out) */
                if (desc->in_count < 3 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                float x = le_pi_get_float_fast(img, args[0]);
                float y = le_pi_get_float_fast(img, args[1]);
                float delta = le_pi_get_float_fast(img, args[2]);
                float sn, cs;
                le_sincos(delta, &sn, &cs);
                le_pi_set_float_fast(img, args[3], x * cs - y * sn);
                le_pi_set_float_fast(img, args[4], x * sn + y * cs);
                return LE_OK;
            }
#endif /* LE_ENABLE_COMPLEX */

            case LE_FUNC_CLAMP_F: {
                /* [value, min, max] -> [out] (3-in/1-out) */
                if (desc->in_count < 3 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                float v = le_pi_get_float_fast(img, args[0]);
                float mn = le_pi_get_float_fast(img, args[1]);
                float mx = le_pi_get_float_fast(img, args[2]);
                float lo = (mn < mx) ? mn : mx;
                float hi = (mn < mx) ? mx : mn;
                float r = (v < lo) ? lo : ((v > hi) ? hi : v);
                le_pi_set_float_fast(img, args[3], r);
                return LE_OK;
            }

#if LE_ENABLE_PROTECTION
            case LE_FUNC_PHASOR_1P:
                return le_eval_relay_phasor_1p(desc, args, img, scan_srate);

            case LE_FUNC_PHASOR_3P:
                return le_eval_relay_phasor_3p(desc, args, img, scan_srate);

            case LE_FUNC_FREQ_EST:
                return le_eval_relay_freq_est(desc, args, img, scan_srate);

            case LE_FUNC_PHASE_COMP:
                return le_eval_relay_phase_comp(desc, args, img);

            case LE_FUNC_DIFF_87:
                return le_eval_relay_diff_87(desc, args, img);

            case LE_FUNC_DIST_21:
                return le_eval_relay_dist_21(desc, args, img, now_ms);

            case LE_FUNC_OVERCURRENT_51:
                return le_eval_relay_overcurrent_51(desc, args, img, scan_dt);
#endif /* LE_ENABLE_PROTECTION */

            default:
                return LE_ERR_UNKNOWN_OPCODE;
        }
    }

    /* Custom node: hand the whole operand list to the board HAL */
    if (g_le_hal && g_le_hal->ext_call) {
        return g_le_hal->ext_call(func_id, args, desc->in_count, desc->out_count, img);
    }
    return le_custom_nodes_dispatch(func_id, args, desc->in_count, desc->out_count, img);
}

#endif /* LE_EVAL_BLOCK_H */

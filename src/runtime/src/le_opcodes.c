/**
 * @file le_opcodes.c
 * @brief Implementation of opcode execution handlers.
 */

#include "le_opcodes.h"
#include "le_rt.h"
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

    /* Single flat dispatch: every opcode is a labeled case (reserved placeholders
     * included) so the compiler can emit one dense O(1) jump table. Feature-switch
     * #if guards wrap only the case BODIES, never the labels, so disabling a
     * subsystem cannot punch holes in the sequence. Reserved (unassigned) values
     * return LE_ERR_UNKNOWN_OPCODE so they are loudly rejected but cheap. */
    switch (op)
    {
        case LE_OP_NOP:
            return LE_OK;

        case LE_OP_MOVE: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            if (mod & LE_MOD_INVERT_A) a = !a;
            bool res = a;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_NOT: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            if (mod & LE_MOD_INVERT_A) a = !a;
            bool res = !a;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_AND: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a && b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_OR: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a || b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_XOR: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a ^ b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_NAND: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = !(a && b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_NOR: {
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = !(a || b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_MUX: {
            /* MUX: in_a is input 0, in_b is input 1; the active select is
             * handled by the LE_FUNC_MUX_SELECT block builtin. */
            bool a = le_process_image_get_bool(img, inst->in_a);
            bool b = le_process_image_get_bool(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a; /* fallback */
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }

        case LE_OP_RESERVED_09:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_0F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_10:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_11:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_12:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_13:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_14:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_15:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_16:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_17:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_18:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_19:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_1F:
            return LE_ERR_UNKNOWN_OPCODE;
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
            if (s) { q = true; } else if (r) { q = false; }
            le_process_image_set_bool(img, inst->out, q);
            return LE_OK;
        }
        case LE_OP_RS: {
            /* Reset-dominant latch: in_a = Set, in_b = Reset, out = Q */
            bool s = le_process_image_get_bool(img, inst->in_a);
            bool r = le_process_image_get_bool(img, inst->in_b);
            bool q = le_process_image_get_bool(img, inst->out);
            if (r) { q = false; } else if (s) { q = true; }
            le_process_image_set_bool(img, inst->out, q);
            return LE_OK;
        }
        case LE_OP_RESERVED_24:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_25:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_26:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_27:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_28:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_29:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_2F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_TON: {
            uint16_t t_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            bool in_val = le_process_image_get_bool(img, inst->in_a);
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
            uint16_t t_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            bool in_val = le_process_image_get_bool(img, inst->in_a);
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
            uint16_t t_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            bool in_val = le_process_image_get_bool(img, inst->in_a);
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

        case LE_OP_CTU: {
            uint16_t c_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_process_image_get_bool(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_process_image_get_bool(img, inst->in_b) : false;
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
            uint16_t c_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_process_image_get_bool(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_process_image_get_bool(img, inst->in_b) : false;
            if (en && !c->prev_cu) { if (c->count > 0) c->count--; }  /* count down, floor at 0 */
            if (has_aux && aux && !c->prev_cd) c->count = c->preset;  /* reset restores preset */
            c->qd = (c->count <= 0);
            c->prev_cu = en;
            c->prev_cd = aux;
            le_process_image_set_bool(img, inst->out, c->qd);
            return LE_OK;
        }
        case LE_OP_CTUD: {
            uint16_t c_idx = inst->out & LE_ADDR_INDEX_MASK;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_process_image_get_bool(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_process_image_get_bool(img, inst->in_b) : false;
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
        case LE_OP_RESERVED_36:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_37:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_38:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_39:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_3F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_MOVE_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            le_process_image_set_float(img, inst->out, a);
            return LE_OK;
        }
        case LE_OP_ADD_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, a + b);
            return LE_OK;
        }
        case LE_OP_SUB_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, a - b);
            return LE_OK;
        }
        case LE_OP_MUL_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, a * b);
            return LE_OK;
        }
        case LE_OP_DIV_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, (fabsf(b) > 1e-9f) ? (a / b) : 0.0f);
            return LE_OK;
        }
        case LE_OP_NEG_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            le_process_image_set_float(img, inst->out, -a);
            return LE_OK;
        }
        case LE_OP_ABS_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            le_process_image_set_float(img, inst->out, fabsf(a));
            return LE_OK;
        }
        case LE_OP_MIN_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, (a < b) ? a : b);
            return LE_OK;
        }
        case LE_OP_MAX_F: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            le_process_image_set_float(img, inst->out, (a > b) ? a : b);
            return LE_OK;
        }
        case LE_OP_CLAMP_F:
            /* Scalar clamp opcode is unused (the CLAMP element emits the
             * LE_FUNC_CLAMP_F block builtin); reject loudly if ever emitted. */
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_SCALE_F: {
            uint8_t s_idx = inst->modifier & 0xFF;
            float a = le_process_image_get_float(img, inst->in_a);
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
                float res;
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
                le_process_image_set_float(img, inst->out, res);
            }
            return LE_OK;
        }
        case LE_OP_CADD_F:
        case LE_OP_CSUB_F:
        case LE_OP_CMUL_F:
        case LE_OP_CDIV_F:
        case LE_OP_MOVE_C:
#if LE_ENABLE_COMPLEX
        {
            le_complex_t a = le_process_image_get_complex(img, inst->in_a);
            le_complex_t b = le_process_image_get_complex(img, inst->in_b);
            le_complex_t res = le_c_make(0.0f, 0.0f);
            switch (op) {
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
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif

        case LE_OP_RESERVED_50:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_51:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_52:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_53:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_54:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_55:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_56:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_57:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_58:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_59:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_5F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_CMP_GT: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (a > b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_CMP_LT: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (a < b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_CMP_GE: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (a >= b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_CMP_LE: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (a <= b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_CMP_EQ: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (fabsf(a - b) < 1e-6f);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_CMP_NE: {
            float a = le_process_image_get_float(img, inst->in_a);
            float b = le_process_image_get_float(img, inst->in_b);
            bool res = (fabsf(a - b) >= 1e-6f);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_process_image_set_bool(img, inst->out, res);
            return LE_OK;
        }
        case LE_OP_RESERVED_66:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_67:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_68:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_69:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_6F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_PID:
#if LE_ENABLE_DSP
        {
            uint16_t pid_idx = inst->modifier & 0xFF;
            le_pid_state_t* p = (le_pid_state_t*)le_process_image_kind_state(img, LE_BLK_PID, pid_idx);
            if (!p) return LE_ERR_OUT_OF_BOUNDS;
            float sp = le_process_image_get_float(img, inst->in_a);
            float pv = le_process_image_get_float(img, inst->in_b);
            float error = sp - pv;

            float dt = (now_ms - p->last_time_ms) * 0.001f;
            if (dt <= 0.0f || dt > 1.0f) dt = le_rt_scan_dt(); /* default to the enforced scan period */
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
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif

        case LE_OP_RESERVED_71:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_SYM_COMP:
#if LE_ENABLE_PROTECTION
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
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_DIST_21:
            /* Marker: the active DIST_21 path is the variable-arity BLOCK
             * builtin LE_FUNC_DIST_21. */
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_I2C:
#if LE_ENABLE_SERIAL_BUS
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
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_SPI:
#if LE_ENABLE_SERIAL_BUS
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
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_RESERVED_76:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_77:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_78:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_79:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_7F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_EXT_CALL: {
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

        case LE_OP_RESERVED_81:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_82:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_83:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_84:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_85:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_86:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_87:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_88:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_89:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8A:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8B:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8C:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8D:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_8F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_LPF_1P:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_BIQUAD_IIR:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
            le_biquad_state_t* b = (le_biquad_state_t*)le_process_image_kind_state(img, LE_BLK_BIQUAD, idx);
            if (!b) return LE_ERR_OUT_OF_BOUNDS;
            /* Direct Form II Canonical Form */
            float w = in_x - (b->a1 * b->w1) - (b->a2 * b->w2);
            out_y = (b->b0 * w) + (b->b1 * b->w1) + (b->b2 * b->w2);
            b->w2 = b->w1;
            b->w1 = w;
            b->initialized = true;
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_MOVING_AVG:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_RATE_LIMITER:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_DEADBAND:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_WASHOUT:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_PEAK_DETECTOR:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_RMS:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_MEDIAN:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_DERIVATIVE:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
                /* Cadence derived strictly from the VM scan period; `gain` remains
                 * an optional engineering-unit multiplier (default 1.0). */
                float raw_deriv = dx * (1.0f / le_rt_scan_dt()) * d->gain;
                out_y = d->prev_y + alpha * (raw_deriv - d->prev_y);
                d->prev_x = in_x;
                d->prev_y = out_y;
            }
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_ZERO_CROSSING:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
            le_zero_crossing_state_t* zc = (le_zero_crossing_state_t*)le_process_image_kind_state(img, LE_BLK_ZERO_CROSSING, idx);
            if (!zc) return LE_ERR_OUT_OF_BOUNDS;
            if (inst->in_b != LE_ADDR_UNUSED && le_process_image_get_bool(img, inst->in_b)) {
                zc->frequency_hz = 0.0f;
                zc->samples_since_cross = 0;
                zc->last_state = 0;
            } else {
                /* Cadence derived strictly from the VM scan period; the legacy
                 * sample_rate_hz state field is deprecated/ignored. */
                float srate = 1.0f / le_rt_scan_dt();
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_LUT_1D:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_TOTALIZER:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
                /* Cadence derived strictly from the VM scan period; the legacy
                 * sample_time_sec state field is deprecated/ignored. */
                double dt = (double)le_rt_scan_dt();
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_MIN_MAX_HOLD:
#if LE_ENABLE_DSP
        {
            uint8_t idx = mod & 0xFF;
            float in_x = le_process_image_get_float(img, inst->in_a);
            float out_y = 0.0f;
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
            le_process_image_set_float(img, inst->out, out_y);
            return LE_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        case LE_OP_RESERVED_9E:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_9F:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_BLOCK: {
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
                /* Builtin function dispatch (modifier-domain table). */
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

                    case LE_FUNC_RECT2POLAR:
#if LE_ENABLE_COMPLEX
                    {
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
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_POLAR2RECT:
#if LE_ENABLE_COMPLEX
                    {
                        /* args = [mag, angle_rad, out_real, out_imag] (2-in/2-out) */
                        if (desc->in_count < 2 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                        float mag = le_process_image_get_float(img, args[0]);
                        float ang = le_process_image_get_float(img, args[1]);
                        le_process_image_set_float(img, args[2], mag * cosf(ang));
                        le_process_image_set_float(img, args[3], mag * sinf(ang));
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_COMPLEX2POLAR:
#if LE_ENABLE_COMPLEX
                    {
                        /* complex in -> {mag, angle} (1-in/2-out) */
                        if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                        le_complex_t c = le_process_image_get_complex(img, args[0]);
                        le_process_image_set_float(img, args[1], le_c_mag(c));
                        le_process_image_set_float(img, args[2], le_c_ang(c));
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_COMPLEX2RECT:
#if LE_ENABLE_COMPLEX
                    {
                        /* complex in -> {real, imag} (1-in/2-out) */
                        if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                        le_complex_t c = le_process_image_get_complex(img, args[0]);
                        le_process_image_set_float(img, args[1], c.r);
                        le_process_image_set_float(img, args[2], c.i);
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_RECT2COMPLEX:
#if LE_ENABLE_COMPLEX
                    {
                        /* {real, imag} floats -> complex out (2-in/1-out) */
                        if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                        float re = le_process_image_get_float(img, args[0]);
                        float im = le_process_image_get_float(img, args[1]);
                        le_process_image_set_complex(img, args[2], le_c_make(re, im));
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_POLAR2COMPLEX:
#if LE_ENABLE_COMPLEX
                    {
                        /* {mag, angle} floats -> complex out (2-in/1-out) */
                        if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                        float mag = le_process_image_get_float(img, args[0]);
                        float ang = le_process_image_get_float(img, args[1]);
                        le_process_image_set_complex(img, args[2], le_c_polar(mag, ang));
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_PHASOR_SHIFT:
#if LE_ENABLE_COMPLEX
                    {
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
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
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
                    case LE_FUNC_PHASOR_1P:
#if LE_ENABLE_PROTECTION
                    {
                        /* Dynamic frequency phasor extractor with self-sync support.
                         * args = [sample, sync_complex, freq_hz, out_complex] (3-in/1-out)
                         * 
                         * The freq_hz input determines the DFT window:
                         * - Higher freq = fewer samples per cycle in the buffer
                         * - Lower freq = more samples per cycle in the buffer
                         * - DFT window size = sample_rate_hz / freq_hz
                         * 
                         * Self-sync mode (state property):
                         * - true: Output angle is 0-degree referenced (self-synchronized)
                         * - false: Output angle referenced to sync phasor
                         */
                        if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                        uint16_t p_idx = inst->in_a;
                        le_phasor_state_t* p = (le_phasor_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR, p_idx);
                        if (!p) return LE_ERR_OUT_OF_BOUNDS;
                        
                        // Get inputs: [sample, sync] (2-in) or [sample, sync, freq_hz] (3-in)
                        float sample = le_process_image_get_float(img, args[0]);
                        le_complex_t sync = le_process_image_get_complex(img, args[1]);
                        float freq_hz = 60.0f;
                        uint16_t out_addr = args[2];
                        if (desc->in_count >= 3) {
                            freq_hz = le_process_image_get_float(img, args[2]);
                            out_addr = args[3];
                        }
                        
                        // Determine sample rate: if explicitly set (>0) on state use it, else derive from enforced scan dt
                        float sample_rate = (p->sample_rate_hz > 0.0f) ? p->sample_rate_hz :
                                            ((le_rt_scan_dt() > 0.0f) ? (1.0f / le_rt_scan_dt()) : 2400.0f);
                        
                        // Validate freq_hz
                        if (freq_hz <= 0.0f) freq_hz = 60.0f;  // fallback to 60 Hz
                        if (freq_hz > sample_rate / 2.0f) freq_hz = sample_rate / 2.0f;  // Nyquist
                        
                        // Compute samples needed for one cycle at this frequency
                        float samples_per_cycle = sample_rate / freq_hz;
                        if (samples_per_cycle < 4.0f) samples_per_cycle = 4.0f;
                        if (samples_per_cycle > (float)LE_MAX_RAW_SAMPLES) samples_per_cycle = (float)LE_MAX_RAW_SAMPLES;
                        
                        uint16_t N = (uint16_t)(samples_per_cycle + 0.5f);
                        if (N < 4) N = 4;
                        if (N > LE_MAX_RAW_SAMPLES) N = LE_MAX_RAW_SAMPLES;
                        p->samples_per_cycle = N;
                        
                        // Store sample in circular buffer of length N
                        if (p->raw_write_idx >= N) p->raw_write_idx = 0;
                        p->raw_samples[p->raw_write_idx] = sample;
                        p->raw_write_idx = (p->raw_write_idx + 1) % N;
                        
                        // Full-cycle Discrete Fourier Transform
                        float sum_cos = 0.0f, sum_sin = 0.0f;
                        float angle_step = 2.0f * (float)M_PI / (float)N;
                        for (uint16_t k = 0; k < N; k++) {
                            uint16_t s_idx = (p->raw_write_idx + k) % N;
                            float a = angle_step * (float)k;
                            float s_val = p->raw_samples[s_idx];
                            sum_cos += s_val * cosf(a);
                            sum_sin -= s_val * sinf(a);
                        }
                        
                        float real = (2.0f / (float)N) * sum_cos;
                        float imag = (2.0f / (float)N) * sum_sin;
                        le_complex_t ph = le_c_make(real, imag);
                        
                        // Synchronization logic
                        float sync_mag = le_c_mag(sync);
                        float sync_ang = le_c_ang(sync);
                        float mag = le_c_mag(ph);
                        float ang = le_c_ang(ph);
                        
                        if (p->self_sync) {
                            // Self-synchronized: output angle is 0 degrees
                            ang = 0.0f;
                        } else if (fabsf(sync_mag) > 1e-6f) {
                            // Reference to sync phasor
                            ang = ang - sync_ang;
                            while (ang > (float)M_PI) ang -= 2.0f * (float)M_PI;
                            while (ang < -(float)M_PI) ang += 2.0f * (float)M_PI;
                            mag = mag / sync_mag;
                        }
                        
                        le_complex_t out = le_c_polar(mag, ang);
                        p->magnitude = mag;
                        p->angle_rad = ang;
                        p->phasor = out;
                        le_process_image_set_complex(img, out_addr, out);
                        
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_PHASOR_3P:
#if LE_ENABLE_PROTECTION
                    {
                        /* Three-phase dynamic frequency phasor extractor (PHASOR_3P).
                         * args = [sample_a, sample_b, sample_c, sync_cplx, freq_hz,
                         *         out_a, out_b, out_c]  (5-in/3-out)
                         * Runs three independent single-phase phasor extractors (a, b, c),
                         * each with its own circular sample buffer and DFT accumulator,
                         * all sharing a single bus reference phasor (sync) and a single
                         * system frequency input (freq_hz). The samples_per_cycle /
                         * sample_rate_hz / self_sync properties apply identically to every
                         * phase, exactly as PHASOR_1P applies them to one phase. */
                        if (desc->in_count < 5 || desc->out_count < 3) return LE_ERR_OUT_OF_BOUNDS;
                        uint16_t p_idx = (uint16_t)(inst->in_a & 0xFF);
                        le_phasor3_state_t* p = (le_phasor3_state_t*)le_process_image_kind_state(img, LE_BLK_PHASOR3, p_idx);
                        if (!p) return LE_ERR_OUT_OF_BOUNDS;

                        // Shared inputs: sync phasor + system frequency
                        le_complex_t sync = le_process_image_get_complex(img, args[3]);
                        float freq_hz = le_process_image_get_float(img, args[4]);

                        // Determine sample rate: if explicitly set (>0) on state use it, else derive from enforced scan dt
                        float sample_rate = (p->sample_rate_hz > 0.0f) ? p->sample_rate_hz :
                                            ((le_rt_scan_dt() > 0.0f) ? (1.0f / le_rt_scan_dt()) : 2400.0f);

                        // Validate freq_hz (shared across phases)
                        if (freq_hz <= 0.0f) freq_hz = 60.0f;  // fallback to 60 Hz
                        if (freq_hz > sample_rate / 2.0f) freq_hz = sample_rate / 2.0f;  // Nyquist

                        // Compute samples needed for one cycle at this frequency (shared DFT window)
                        float samples_per_cycle = sample_rate / freq_hz;
                        if (samples_per_cycle < 4.0f) samples_per_cycle = 4.0f;
                        if (samples_per_cycle > (float)LE_MAX_RAW_SAMPLES) samples_per_cycle = (float)LE_MAX_RAW_SAMPLES;

                        uint16_t N = (uint16_t)(samples_per_cycle + 0.5f);
                        if (N < 4) N = 4;
                        if (N > LE_MAX_RAW_SAMPLES) N = LE_MAX_RAW_SAMPLES;
                        p->samples_per_cycle = N;

                        float sync_mag = le_c_mag(sync);
                        float sync_ang = le_c_ang(sync);
                        float angle_step = 2.0f * (float)M_PI / (float)N;

                        /* Per-phase extractor: buffer, write idx, phasor, magnitude, angle. */
                        for (int phase = 0; phase < 3; phase++) {
                            float sample = le_process_image_get_float(img, args[phase]);

                            float*        raw     = NULL;
                            uint16_t*     wr      = NULL;
                            float*        mag_out = NULL;
                            float*        ang_out = NULL;
                            le_complex_t* ph_out  = NULL;

                            if (phase == 0)      { raw = p->raw_samples_a; wr = &p->raw_write_idx_a; ph_out = &p->phasor_a; mag_out = &p->magnitude_a; ang_out = &p->angle_rad_a; }
                            else if (phase == 1) { raw = p->raw_samples_b; wr = &p->raw_write_idx_b; ph_out = &p->phasor_b; mag_out = &p->magnitude_b; ang_out = &p->angle_rad_b; }
                            else                  { raw = p->raw_samples_c; wr = &p->raw_write_idx_c; ph_out = &p->phasor_c; mag_out = &p->magnitude_c; ang_out = &p->angle_rad_c; }

                            // Store sample in this phase's circular buffer of length N
                            if (*wr >= N) *wr = 0;
                            raw[*wr] = sample;
                            *wr = (uint16_t)((*wr + 1) % N);

                            // Full-cycle Discrete Fourier Transform
                            float sum_cos = 0.0f, sum_sin = 0.0f;
                            for (uint16_t k = 0; k < N; k++) {
                                uint16_t s_idx = (uint16_t)((*wr + k) % N);
                                float a = angle_step * (float)k;
                                float s_val = raw[s_idx];
                                sum_cos += s_val * cosf(a);
                                sum_sin -= s_val * sinf(a);
                            }

                            float real = (2.0f / (float)N) * sum_cos;
                            float imag = (2.0f / (float)N) * sum_sin;
                            le_complex_t phc = le_c_make(real, imag);

                            // Synchronization logic (shared sync reference)
                            float mag = le_c_mag(phc);
                            float ang = le_c_ang(phc);

                            if (p->self_sync) {
                                ang = 0.0f;                       // 0-degree self-reference
                            } else if (fabsf(sync_mag) > 1e-6f) {
                                ang = ang - sync_ang;             // reference to bus sync phasor
                                while (ang > (float)M_PI) ang -= 2.0f * (float)M_PI;
                                while (ang < -(float)M_PI) ang += 2.0f * (float)M_PI;
                                mag = mag / sync_mag;
                            }

                            le_complex_t out = le_c_polar(mag, ang);
                            *mag_out = mag;
                            *ang_out = ang;
                            *ph_out  = out;
                            le_process_image_set_complex(img, args[5 + phase], out);
                        }

                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_FREQ_EST:
#if LE_ENABLE_PROTECTION
                    {
                        /* ANSI 81 dynamic frequency estimator (FREQ_EST).
                         * args = [sample, out_freq_hz, out_valid]  (1-in/2-out)
                         * Tracks fundamental frequency from instantaneous samples with
                         * hysteresis noise rejection and fractional sub-sample
                         * zero-crossing interpolation. Sampling rate is strictly
                         * derived from the enforced VM cadence (le_rt_scan_dt()).
                         *
                         * Semantics (see le_freq_est_state_t):
                         *  - Healthy in-bounds measurement  -> freq_hz = measured, valid = true
                         *  - Out-of-bounds but measured      -> freq_hz = measured, valid = false
                         *    (so 81U/81O still see a real under/over-frequency event)
                         *  - Loss of potential / stall        -> freq_hz = nominal,   valid = false */
                        if (desc->in_count < 1 || desc->out_count < 2) return LE_ERR_OUT_OF_BOUNDS;
                        uint16_t f_idx = (uint16_t)(inst->in_a & 0xFF);
                        le_freq_est_state_t* p = (le_freq_est_state_t*)le_process_image_kind_state(img, LE_BLK_FREQ_EST, f_idx);
                        if (!p) return LE_ERR_OUT_OF_BOUNDS;

                        float sample = le_process_image_get_float(img, args[0]);
                        float fs = 1.0f / le_rt_scan_dt();

                        /* 1. Hysteresis Schmitt-trigger arming state machine. */
                        int8_t cur = p->armed_state;
                        bool fresh_cross = false;
                        if (sample <= -p->hysteresis) {
                            cur = -1;
                        } else if (sample >= p->hysteresis && p->armed_state == -1) {
                            cur = 1;          /* positive-going zero crossing */
                            fresh_cross = true;
                        }

                        /* 2. Advance the sample counter. */
                        p->samples_since_cross++;

                        /* 3. Measure the period ONLY on the actual transition sample. */
                        bool measured = false;
                        if (fresh_cross) {
                            /* Fractional sub-sample offset of this crossing (0..1). */
                            float frac = 0.5f;
                            float denom = sample - p->prev_sample;
                            if (denom > 1e-12f) {
                                frac = (0.0f - p->prev_sample) / denom;
                                if (frac < 0.0f) frac = 0.0f;
                                if (frac > 1.0f) frac = 1.0f;
                            }
                            if (p->initialized) {
                                /* One complete period between the previous and this crossing. */
                                float n_period = (float)p->samples_since_cross + frac - p->prev_cross_frac;
                                if (n_period > 1.0f) {
                                    float f_raw = fs / n_period;
                                    float prev_out = p->tracked_freq_hz;
                                    /* Optional EWMA smoothing against the prior output. */
                                    float f_out = (p->filter_alpha > 0.0f)
                                        ? (prev_out + p->filter_alpha * (f_raw - prev_out))
                                        : f_raw;
                                    if (f_raw >= p->min_freq_hz && f_raw <= p->max_freq_hz) {
                                        p->tracked_freq_hz = f_out;   /* measured & healthy */
                                        p->valid = true;
                                    } else {
                                        p->tracked_freq_hz = f_out;   /* measured but out of bounds */
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

                        /* 4. Loss-of-potential / dead-signal timeout -> nominal fallback. */
                        float n_timeout = 1.25f * (fs / p->min_freq_hz);
                        if (!measured && p->samples_since_cross > (uint32_t)n_timeout) {
                            p->tracked_freq_hz = p->nominal_freq_hz;
                            p->valid = false;
                        }

                        /* 5. Publish outputs. */
                        le_process_image_set_float(img, args[1], p->tracked_freq_hz);
                        le_process_image_set_bool(img, args[2], p->valid);
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_PHASE_COMP:
#if LE_ENABLE_PROTECTION
                    {
                        /* 3-phase transformer phase-shift compensation (ANSI 87T).
                         * args = [c_a, c_b, c_c, out_a, out_b, out_c]  (3-in/3-out).
                         * Applies the SEL delta/wye compensation matrix M(k) (k = comp,
                         * 1..12, baked in le_comp33_state_t) to the three winding phasors:
                         *   I'_x = s * sum_j M[k][x][j] * I_j
                         * with scalar s = 1/sqrt(3) for odd k, 1/3 for even k. */
                        if (desc->in_count < 3 || desc->out_count < 3) return LE_ERR_OUT_OF_BOUNDS;
                        le_comp33_state_t* c33 = (le_comp33_state_t*)le_process_image_kind_state(img, LE_BLK_PHASE_COMP, inst->in_a & 0xFF);
                        if (!c33) return LE_ERR_OUT_OF_BOUNDS;
                        int k = (int)c33->comp;
                        if (k < 1) k = 1;
                        if (k > 12) k = 12;
                        float s = (k & 1) ? (1.0f / sqrtf(3.0f)) : (1.0f / 3.0f);

                        le_complex_t ia = le_process_image_get_complex(img, args[0]);
                        le_complex_t ib = le_process_image_get_complex(img, args[1]);
                        le_complex_t ic = le_process_image_get_complex(img, args[2]);

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
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_DIFF_87:
#if LE_ENABLE_PROTECTION
                    {
                        /* N-complex dual-slope differential protection (ANSI 87).
                         * args = [cph0, cph1, ..., cphN-1, out_bool]  (N-in/1-out)
                         * Operate current = |vector sum of all phasors|; restraint
                         * current = SUM of phasor magnitudes (large-bus convention,
                         * no averaging). Trips when operate exceeds the dual-slope
                         * threshold from the baked le_diff87_state_t:
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
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif
                    case LE_FUNC_DIST_21:
#if LE_ENABLE_PROTECTION
                    {
                        /* Mho distance (21) block with prefault voltage memory.
                         * args = [v_c, i_c, offset_on, out_bool] (3-in/1-out)
                         * v_c, i_c are complex phasors; offset_on gates the mho-offset
                         * circle shift. Computes Z = V/I and trips when Z is inside the
                         * offset-mho circle. */
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
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
                    }
#endif

                    case LE_FUNC_OVERCURRENT_51:
#if LE_ENABLE_PROTECTION
                    {
                        /* IEC/IEEE inverse-time overcurrent (ANSI 51) as a block.
                         * args = [i_c (complex phasor), enable (bool), out_bool]
                         * (2-in/1-out). The phasor MAGNITUDE drives the inverse-time
                         * accumulator. The boolean ENABLE is AND'd with the pickup
                         * evaluation BEFORE the timing function: while disabled, the
                         * element never accumulates a trip (it decays), so wiring a
                         * directionality boolean to it yields a ground-directional
                         * overcurrent element, and other booleans simply enable/
                         * disable the element. Unconnected enable = LE_CONST_TRUE. */
                        if (desc->in_count < 2 || desc->out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
                        uint16_t oc_idx = (uint16_t)(inst->in_a & 0xFF);
                        le_overcurrent_state_t* oc = (le_overcurrent_state_t*)le_process_image_kind_state(img, LE_BLK_OVERCURRENT, oc_idx);
                        if (!oc) return LE_ERR_OUT_OF_BOUNDS;

                        /* Defaults if not baked by the compiler/loader. */
                        if (oc->pickup <= 0.0f) oc->pickup = 1.0f;       /* default 1.0 pu pickup */
                        if (oc->time_dial <= 0.0f) oc->time_dial = 1.0f;

                        /* Curve coefficients: the compiler BAKES a/b/p (and custom
                         * curves) into the state, so a loaded program skips the table.
                         * This table only resolves coefficients for hand-assembled /
                         * legacy images that set curve_type but left a/b/p = 0. The
                         * curve_type value (0 = IEC Normal ... 8 = Custom) is used
                         * verbatim — a 0 is a real curve, never "unset". */
                        if (oc->a_coeff == 0.0f && oc->b_coeff == 0.0f && oc->p_coeff == 0.0f) {
                            float A = 13.5f, B = 0.0f, P = 1.0f;   /* IEC Very Inverse default */
                            switch (oc->curve_type) {
                                case LE_CURVE_IEC_NORMAL:      A = 0.14f;     B = 0.0f;    P = 0.02f; break;
                                case LE_CURVE_IEC_VERY:        A = 13.5f;     B = 0.0f;    P = 1.0f;  break;
                                case LE_CURVE_IEC_EXTREME:     A = 80.0f;     B = 0.0f;    P = 2.0f;  break;
                                case LE_CURVE_IEEE_MODERATELY: A = 0.0515f;   B = 0.1140f; P = 0.02f; break;
                                case LE_CURVE_IEEE_VERY:       A = 19.61f;    B = 0.491f;  P = 2.0f;  break;
                                case LE_CURVE_IEEE_EXTREME:    A = 28.2f;     B = 0.1217f; P = 2.0f;  break;
                                case LE_CURVE_IEEE_SHORT:      A = 0.00342f;  B = 0.00262f;P = 0.02f; break;
                                case LE_CURVE_IEEE_LONG:       A = 26.13f;    B = 0.349f;  P = 2.0f;  break;
                                default: break;               /* stays IEC Very Inverse (covers CUSTOM w/ no coeffs) */
                            }
                            oc->a_coeff = A; oc->b_coeff = B; oc->p_coeff = P;
                        }
                        /* Sanitize: a degenerate/custom curve with no meaningful
                         * coefficients falls back to IEC Very Inverse. */
                        if (!(oc->a_coeff > 0.0f) || !(oc->p_coeff > 0.0f)) {
                            oc->a_coeff = 13.5f; oc->b_coeff = 0.0f; oc->p_coeff = 1.0f;
                        }

                        le_complex_t i_c = le_process_image_get_complex(img, args[0]);
                        bool enable = le_process_image_get_bool(img, args[1]);
                        float M = le_c_mag(i_c) / oc->pickup;   /* M = I / pickup in pu */
                        float time_dial = oc->time_dial;

                        /* Time-dependent protection uses the fixed scan period (the
                         * enforced cadence determines dt so the inverse-time curve is
                         * deterministic). */
                        float dt = (le_rt_scan_dt() > 0.0f) ? le_rt_scan_dt() : 0.01f;

                        /* Enable is AND'd with pickup BEFORE the timing: only an
                         * ENABLED element with M > 1 accrues operate time. Off-pickup
                         * (or disabled-over-fault, treated as resting in pickup) cools
                         * along the reset curve, so it can never magically charge. */
                        if (enable && M > 1.0f) {
                            /* Inverse-time operate:
                             *   t_operate = time_dial * (A / (M^p - 1) + B)
                             * accumulator advances by dt / t_operate and trips at 1.0
                             * (= the full operate time has elapsed). */
                            float mp = powf(M, oc->p_coeff);
                            float denom = mp - 1.0f;       /* > 0 whenever M > 1 */
                            if (denom < 1e-6f) denom = 1e-6f;   /* guard M ~ 1 */
                            float t_operate = time_dial * (oc->a_coeff / denom + oc->b_coeff);
                            if (!(t_operate > 1e-6f)) t_operate = 1e-6f;
                            oc->accumulator += dt / t_operate;
                            if (oc->accumulator >= 1.0f) oc->accumulator = 1.0f;
                        } else {
                            /* Cooling reset curve:
                             *   t_reset = time_dial * (4.6 / (1 - M_r^2))
                             * M_r is clamped below 1 so the denominator stays positive
                             * even when M >= 1 (a disabled element resting "at pickup"). */
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
                        le_process_image_set_bool(img, args[2], oc->tripped);
                        return LE_OK;
                    }
#else
                    {
                        (void)argc;
                        return LE_ERR_UNKNOWN_OPCODE;
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
        case LE_OP_RESERVED_A1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_A9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AD:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AE:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_AF:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B0:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_B9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BD:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BE:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_BF:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C0:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_C9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CD:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CE:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_CF:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D0:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_D9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DD:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DE:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_DF:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E0:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_E9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_EA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_EB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_EC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_ED:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_EE:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_EF:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F0:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F1:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F2:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F3:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F4:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F5:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F6:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F7:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F8:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_F9:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_FA:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_FB:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_FC:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_FD:
            return LE_ERR_UNKNOWN_OPCODE;
        case LE_OP_RESERVED_FE:
            return LE_ERR_UNKNOWN_OPCODE;
        default:
            return LE_ERR_UNKNOWN_OPCODE;
    }
}
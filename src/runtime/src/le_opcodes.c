/**
 * @file le_opcodes.c
 * @brief Implementation of opcode execution handlers.
 */

#include "le_opcodes.h"
#include "le_rt.h"
#include "le_hal.h"
#include "le_eval_dsp.h"
#include "le_eval_relay.h"
#include "le_eval_block.h"
#include "le_custom_node.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef fmaf
#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L && !defined(_MSC_VER) && !defined(__GNUC__))
#define fmaf(a, b, c) (((a) * (b)) + (c))
#endif
#endif

le_status_t le_exec_instruction(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms)
{
    return le_exec_instruction_ex(inst, img, now_ms, NULL, 0);
}

le_status_t le_exec_instruction_ex(const le_instruction_t* inst, le_process_image_t* img, uint32_t now_ms,
                                   const le_block_desc_t* blocks, uint16_t block_count)
{
    if (!inst) return LE_ERR_NULL_PTR;
    return le_exec_program(inst, 1, img, now_ms, blocks, block_count);
}

#ifndef LE_USE_THREADED_DISPATCH
#define LE_USE_THREADED_DISPATCH 0
#endif

volatile uint16_t g_last_exec_pc = 0;
volatile uint8_t  g_last_exec_op = 0;
volatile uint8_t  g_last_exec_mod = 0;

#if LE_USE_THREADED_DISPATCH
#define LE_OP_LBL(op) op_lbl_##op:
#define LE_STEP_OK do { \
    if (++vm_pc >= count) return LE_OK; \
    inst = &instructions[vm_pc]; \
    op = inst->opcode; \
    mod = inst->modifier; \
    g_last_exec_pc = vm_pc; \
    g_last_exec_op = op; \
    g_last_exec_mod = mod; \
    goto *s_dispatch_table[op]; \
} while (0)
#else
#define LE_OP_LBL(op)
#define LE_STEP_OK goto next_instruction
#endif

static inline float le_safe_div_f(float num, float den)
{
    if (fabsf(den) <= 1e-9f) {
#if LE_DIV_BY_ZERO_POLICY == LE_DIV_BY_ZERO_SATURATE
        return (num >= 0.0f) ? 3.402823466e+38F : -3.402823466e+38F;
#elif LE_DIV_BY_ZERO_POLICY == LE_DIV_BY_ZERO_INFINITY
        return (num >= 0.0f) ? (float)INFINITY : (float)-INFINITY;
#else
        return 0.0f;
#endif
    }
    float r = num / den;
#if LE_FLUSH_NAN_TO_ZERO
    if (isnan(r)) return 0.0f;
#endif
    return r;
}

le_status_t le_exec_program(const le_instruction_t* instructions, uint16_t count,
                            le_process_image_t* img, uint32_t now_ms,
                            const le_block_desc_t* blocks, uint16_t block_count)
{
    if (!img) return LE_ERR_NULL_PTR;
    if (count == 0) return LE_OK;
    if (!instructions) return LE_ERR_NULL_PTR;

    const float scan_dt = le_rt_scan_dt();
    const float scan_srate = (scan_dt > 0.0f) ? (1.0f / scan_dt) : 1000.0f;

#if LE_USE_THREADED_DISPATCH
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
#endif

    static const void* const s_dispatch_table[256] = {
        [0 ... 255] = &&op_lbl_unknown,
#define LE_DISP_ENTRY(opcode) [opcode] = &&op_lbl_##opcode,
        LE_ACTIVE_OPCODES(LE_DISP_ENTRY)
#undef LE_DISP_ENTRY
    };

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    uint16_t vm_pc = 0;
    const le_instruction_t* inst = &instructions[0];
    uint8_t op = inst->opcode;
    uint8_t mod = inst->modifier;
    g_last_exec_pc = 0;
    g_last_exec_op = op;
    g_last_exec_mod = mod;
    goto *s_dispatch_table[op];

    while (0) {
        switch (0)
        {
#else
    for (uint16_t vm_pc = 0; vm_pc < count; vm_pc++)
    {
        const le_instruction_t* inst = &instructions[vm_pc];
        uint8_t op = inst->opcode;
        uint8_t mod = inst->modifier;
        g_last_exec_pc = vm_pc;
        g_last_exec_op = op;
        g_last_exec_mod = mod;

        /* Single flat dispatch: every opcode is a labeled case (reserved placeholders
         * included) so the compiler can emit one dense O(1) jump table. Feature-switch
         * #if guards wrap only the case BODIES, never the labels, so disabling a
         * subsystem cannot punch holes in the sequence. Reserved (unassigned) values
         * return LE_ERR_UNKNOWN_OPCODE so they are loudly rejected but cheap. */
        switch (op)
        {
#endif
            LE_OP_LBL(LE_OP_NOP) case LE_OP_NOP:
                LE_STEP_OK;

        LE_OP_LBL(LE_OP_MOVE) case LE_OP_MOVE: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            if (mod & LE_MOD_INVERT_A) a = !a;
            bool res = a;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_NOT) case LE_OP_NOT: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            if (mod & LE_MOD_INVERT_A) a = !a;
            bool res = !a;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_AND) case LE_OP_AND: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a && b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_OR) case LE_OP_OR: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a || b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_XOR) case LE_OP_XOR: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a ^ b;
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_NAND) case LE_OP_NAND: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = !(a && b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_NOR) case LE_OP_NOR: {
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = !(a || b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_MUX) case LE_OP_MUX: {
            /* MUX: in_a is input 0, in_b is input 1; the active select is
             * handled by the LE_FUNC_MUX_SELECT block builtin. */
            bool a = le_pi_get_bool_fast(img, inst->in_a);
            bool b = le_pi_get_bool_fast(img, inst->in_b);
            if (mod & LE_MOD_INVERT_A) a = !a;
            if (mod & LE_MOD_INVERT_B) b = !b;
            bool res = a; /* fallback */
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }

        LE_OP_LBL(LE_OP_WORD_AND) case LE_OP_WORD_AND: {
            uint32_t a = le_pi_get_word32_fast(img, inst->in_a);
            uint32_t b = le_pi_get_word32_fast(img, inst->in_b);
            le_pi_set_word32_fast(img, inst->out, a & b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_WORD_OR) case LE_OP_WORD_OR: {
            uint32_t a = le_pi_get_word32_fast(img, inst->in_a);
            uint32_t b = le_pi_get_word32_fast(img, inst->in_b);
            le_pi_set_word32_fast(img, inst->out, a | b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_WORD_XOR) case LE_OP_WORD_XOR: {
            uint32_t a = le_pi_get_word32_fast(img, inst->in_a);
            uint32_t b = le_pi_get_word32_fast(img, inst->in_b);
            le_pi_set_word32_fast(img, inst->out, a ^ b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_WORD_NOT) case LE_OP_WORD_NOT: {
            uint32_t a = le_pi_get_word32_fast(img, inst->in_a);
            le_pi_set_word32_fast(img, inst->out, ~a);
            LE_STEP_OK;
        }
        case LE_OP_RESERVED_0D:
        case LE_OP_RESERVED_0E:
        case LE_OP_RESERVED_0F:
        case LE_OP_RESERVED_10:
        case LE_OP_RESERVED_11:
        case LE_OP_RESERVED_12:
        case LE_OP_RESERVED_13:
        case LE_OP_RESERVED_14:
        case LE_OP_RESERVED_15:
        case LE_OP_RESERVED_16:
        case LE_OP_RESERVED_17:
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_MOVE_I) case LE_OP_MOVE_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            le_pi_set_int_fast(img, inst->out, a);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ADD_I) case LE_OP_ADD_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            int32_t b = le_pi_get_int_fast(img, inst->in_b);
            le_pi_set_int_fast(img, inst->out, a + b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_SUB_I) case LE_OP_SUB_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            int32_t b = le_pi_get_int_fast(img, inst->in_b);
            le_pi_set_int_fast(img, inst->out, a - b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_MUL_I) case LE_OP_MUL_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            int32_t b = le_pi_get_int_fast(img, inst->in_b);
            le_pi_set_int_fast(img, inst->out, a * b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_DIV_I) case LE_OP_DIV_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            int32_t b = le_pi_get_int_fast(img, inst->in_b);
            le_pi_set_int_fast(img, inst->out, (b != 0) ? (a / b) : 0);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_NEG_I) case LE_OP_NEG_I: {
            int32_t a = le_pi_get_int_fast(img, inst->in_a);
            le_pi_set_int_fast(img, inst->out, -a);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_LOAD_I) case LE_OP_LOAD_I: {
            int32_t val = (int32_t)(((uint32_t)inst->in_b << 16) | (uint32_t)inst->in_a);
            le_pi_set_int_fast(img, inst->out, val);
            LE_STEP_OK;
        }
        case LE_OP_RESERVED_1F:
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_RTRIG) case LE_OP_RTRIG: {
            /* in_a = input signal, in_b = history coil, out = pulse out */
            bool cur = le_pi_get_bool_fast(img, inst->in_a);
            bool prev = le_pi_get_bool_fast(img, inst->in_b);
            bool pulse = cur && !prev;
            le_pi_set_bool_fast(img, inst->out, pulse);
            le_pi_set_bool_fast(img, inst->in_b, cur);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_FTRIG) case LE_OP_FTRIG: {
            bool cur = le_pi_get_bool_fast(img, inst->in_a);
            bool prev = le_pi_get_bool_fast(img, inst->in_b);
            bool pulse = !cur && prev;
            le_pi_set_bool_fast(img, inst->out, pulse);
            le_pi_set_bool_fast(img, inst->in_b, cur);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_SR) case LE_OP_SR: {
            /* Set-dominant latch: in_a = Set, in_b = Reset, out = Q */
            bool s = le_pi_get_bool_fast(img, inst->in_a);
            bool r = le_pi_get_bool_fast(img, inst->in_b);
            bool q = le_pi_get_bool_fast(img, inst->out);
            if (s) { q = true; } else if (r) { q = false; }
            le_pi_set_bool_fast(img, inst->out, q);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_RS) case LE_OP_RS: {
            /* Reset-dominant latch: in_a = Set, in_b = Reset, out = Q */
            bool s = le_pi_get_bool_fast(img, inst->in_a);
            bool r = le_pi_get_bool_fast(img, inst->in_b);
            bool q = le_pi_get_bool_fast(img, inst->out);
            if (r) { q = false; } else if (s) { q = true; }
            le_pi_set_bool_fast(img, inst->out, q);
            LE_STEP_OK;
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
        LE_OP_LBL(LE_OP_TON) case LE_OP_TON: {
            uint16_t t_idx = inst->modifier;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            if (inst->in_b != LE_ADDR_UNUSED) {
                int32_t pt = le_pi_get_int_fast(img, inst->in_b);
                if (pt >= 0) t->preset_ms = (uint32_t)pt;
            }
            bool in_val = le_pi_get_bool_fast(img, inst->in_a);
            if (in_val) {
                if (!t->prev_in) {
                    t->start_time_ms = now_ms;
                    t->elapsed_ms = 0;
                    t->q = (t->preset_ms == 0);
                } else if (!t->q) {
                    uint32_t elapsed = now_ms - t->start_time_ms;
                    if (elapsed >= t->preset_ms) {
                        t->elapsed_ms = t->preset_ms;
                        t->q = true;
                    } else {
                        t->elapsed_ms = elapsed;
                    }
                } else {
                    t->elapsed_ms = t->preset_ms;
                }
            } else {
                t->elapsed_ms = 0;
                t->q = false;
            }
            t->prev_in = in_val;
            le_pi_set_bool_fast(img, inst->out, t->q);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_TOF) case LE_OP_TOF: {
            uint16_t t_idx = inst->modifier;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            if (inst->in_b != LE_ADDR_UNUSED) {
                int32_t pt = le_pi_get_int_fast(img, inst->in_b);
                if (pt >= 0) t->preset_ms = (uint32_t)pt;
            }
            bool in_val = le_pi_get_bool_fast(img, inst->in_a);
            if (in_val) {
                t->q = true;
                t->elapsed_ms = 0;
            } else {
                if (t->prev_in) {
                    t->start_time_ms = now_ms;
                }
                if (t->q) {
                    uint32_t elapsed = now_ms - t->start_time_ms;
                    if (elapsed >= t->preset_ms) {
                        t->elapsed_ms = t->preset_ms;
                        t->q = false;
                    } else {
                        t->elapsed_ms = elapsed;
                    }
                }
            }
            t->prev_in = in_val;
            le_pi_set_bool_fast(img, inst->out, t->q);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_TP) case LE_OP_TP: {
            uint16_t t_idx = inst->modifier;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            if (inst->in_b != LE_ADDR_UNUSED) {
                int32_t pt = le_pi_get_int_fast(img, inst->in_b);
                if (pt >= 0) t->preset_ms = (uint32_t)pt;
            }
            bool in_val = le_pi_get_bool_fast(img, inst->in_a);
            if (in_val && !t->prev_in && !t->q) {
                t->start_time_ms = now_ms;
                t->q = (t->preset_ms > 0);
                t->elapsed_ms = 0;
            }
            if (t->q) {
                uint32_t elapsed = now_ms - t->start_time_ms;
                if (elapsed >= t->preset_ms) {
                    t->q = false;
                    t->elapsed_ms = in_val ? t->preset_ms : 0;
                } else {
                    t->elapsed_ms = elapsed;
                }
            } else if (!in_val) {
                t->elapsed_ms = 0;
            }
            t->prev_in = in_val;
            le_pi_set_bool_fast(img, inst->out, t->q);
            LE_STEP_OK;
        }

        LE_OP_LBL(LE_OP_CTU) case LE_OP_CTU: {
            uint16_t c_idx = inst->modifier;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_pi_get_bool_fast(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_pi_get_bool_fast(img, inst->in_b) : false;
            if (en && !c->prev_cu) c->count++;
            if (has_aux && aux && !c->prev_cd) c->count = 0;          /* reset on rising edge */
            if (c->count > c->preset) c->count = c->preset;           /* clamp at preset */
            c->qu = (c->count >= c->preset);
            c->prev_cu = en;
            c->prev_cd = aux;
            le_pi_set_bool_fast(img, inst->out, c->qu);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CTD) case LE_OP_CTD: {
            uint16_t c_idx = inst->modifier;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_pi_get_bool_fast(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_pi_get_bool_fast(img, inst->in_b) : false;
            if (en && !c->prev_cu) { if (c->count > 0) c->count--; }  /* count down, floor at 0 */
            if (has_aux && aux && !c->prev_cd) c->count = c->preset;  /* reset restores preset */
            c->qd = (c->count <= 0);
            c->prev_cu = en;
            c->prev_cd = aux;
            le_pi_set_bool_fast(img, inst->out, c->qd);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CTUD) case LE_OP_CTUD: {
            uint16_t c_idx = inst->modifier;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            bool en = le_pi_get_bool_fast(img, inst->in_a);
            bool has_aux = (inst->in_b != LE_ADDR_UNUSED);
            bool aux = has_aux ? le_pi_get_bool_fast(img, inst->in_b) : false;
            if (en && !c->prev_cu) c->count++;                         /* header is cu */
            if (has_aux && aux && !c->prev_cd) { if (c->count > 0) c->count--; }  /* in_b is cd */
            if (c->count > c->preset) c->count = c->preset;
            c->qu = (c->count >= c->preset);
            c->qd = (c->count <= 0);
            c->prev_cu = en;
            c->prev_cd = aux;
            le_pi_set_bool_fast(img, inst->out, c->qu);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_COUNTER_CV) case LE_OP_COUNTER_CV: {
            uint16_t c_idx = inst->modifier;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            le_pi_set_int_fast(img, inst->out, c->count);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_COUNTER_QD) case LE_OP_COUNTER_QD: {
            uint16_t c_idx = inst->modifier;
            le_counter_state_t* c = le_process_image_counter(img, c_idx);
            if (!c) return LE_ERR_OUT_OF_BOUNDS;
            le_pi_set_bool_fast(img, inst->out, c->qd);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_TIMER_ET) case LE_OP_TIMER_ET: {
            uint16_t t_idx = inst->modifier;
            le_timer_state_t* t = le_process_image_timer(img, t_idx);
            if (!t) return LE_ERR_OUT_OF_BOUNDS;
            le_pi_set_int_fast(img, inst->out, (int32_t)t->elapsed_ms);
            LE_STEP_OK;
        }
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
        LE_OP_LBL(LE_OP_MOVE_F) case LE_OP_MOVE_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, a);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ADD_F) case LE_OP_ADD_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, a + b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_SUB_F) case LE_OP_SUB_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, a - b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_MUL_F) case LE_OP_MUL_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, a * b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_DIV_F) case LE_OP_DIV_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, le_safe_div_f(a, b));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_NEG_F) case LE_OP_NEG_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, -a);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ABS_F) case LE_OP_ABS_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, fabsf(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_MIN_F) case LE_OP_MIN_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, (a < b) ? a : b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_MAX_F) case LE_OP_MAX_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, (a > b) ? a : b);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CLAMP_F) case LE_OP_CLAMP_F:
            /* Scalar clamp opcode is unused (the CLAMP element emits the
             * LE_FUNC_CLAMP_F block builtin); reject loudly if ever emitted. */
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_SCALE_F) case LE_OP_SCALE_F: {
            uint8_t s_idx = inst->modifier & 0xFF;
            float a = le_pi_get_float_fast(img, inst->in_a);
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
                le_pi_set_float_fast(img, inst->out, res);
            }
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CADD_F) case LE_OP_CADD_F:
        LE_OP_LBL(LE_OP_CSUB_F) case LE_OP_CSUB_F:
        LE_OP_LBL(LE_OP_CMUL_F) case LE_OP_CMUL_F:
        LE_OP_LBL(LE_OP_CDIV_F) case LE_OP_CDIV_F:
        LE_OP_LBL(LE_OP_MOVE_C) case LE_OP_MOVE_C:
#if LE_ENABLE_COMPLEX
        {
            le_complex_t a = le_pi_get_complex_fast(img, inst->in_a);
            le_complex_t b = le_pi_get_complex_fast(img, inst->in_b);
            le_complex_t res = le_c_make(0.0f, 0.0f);
            switch (op) {
                case LE_OP_MOVE_C: res = a; break;
                case LE_OP_CADD_F: res = le_c_add(a, b); break;
                case LE_OP_CSUB_F: res = le_c_sub(a, b); break;
                case LE_OP_CMUL_F: res = le_c_mul(a, b); break;
                case LE_OP_CDIV_F: res = le_c_div(a, b); break;
                default: break;
            }
            le_pi_set_complex_fast(img, inst->out, res);
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif

        LE_OP_LBL(LE_OP_FMA_F) case LE_OP_FMA_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            float c = le_pi_get_float_fast(img, inst->out);
            le_pi_set_float_fast(img, inst->out, fmaf(a, b, c));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_FMS_F) case LE_OP_FMS_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            float c = le_pi_get_float_fast(img, inst->out);
            le_pi_set_float_fast(img, inst->out, fmaf(-a, b, c));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_LOAD_F) case LE_OP_LOAD_F: {
            uint32_t raw = ((uint32_t)inst->in_b << 16) | (uint32_t)inst->in_a;
            float val;
            memcpy(&val, &raw, sizeof(float));
            le_pi_set_float_fast(img, inst->out, val);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_SQRT_F) case LE_OP_SQRT_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_sqrt(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_SIN_F) case LE_OP_SIN_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_sin(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_COS_F) case LE_OP_COS_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_cos(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_TAN_F) case LE_OP_TAN_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_tan(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ASIN_F) case LE_OP_ASIN_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_asin(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ACOS_F) case LE_OP_ACOS_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_acos(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ATAN_F) case LE_OP_ATAN_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_atan(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_ATAN2_F) case LE_OP_ATAN2_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, le_fast_atan2(a, b));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_EXP_F) case LE_OP_EXP_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_exp(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_LOG_F) case LE_OP_LOG_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_log(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_LOG10_F) case LE_OP_LOG10_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            le_pi_set_float_fast(img, inst->out, le_fast_log10(a));
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_POW_F) case LE_OP_POW_F: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            le_pi_set_float_fast(img, inst->out, le_fast_pow(a, b));
            LE_STEP_OK;
        }
        case LE_OP_RESERVED_5F:
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_CMP_GT) case LE_OP_CMP_GT: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (a > b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CMP_LT) case LE_OP_CMP_LT: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (a < b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CMP_GE) case LE_OP_CMP_GE: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (a >= b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CMP_LE) case LE_OP_CMP_LE: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (a <= b);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CMP_EQ) case LE_OP_CMP_EQ: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (fabsf(a - b) < 1e-6f);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
        }
        LE_OP_LBL(LE_OP_CMP_NE) case LE_OP_CMP_NE: {
            float a = le_pi_get_float_fast(img, inst->in_a);
            float b = le_pi_get_float_fast(img, inst->in_b);
            bool res = (fabsf(a - b) >= 1e-6f);
            if (mod & LE_MOD_INVERT_OUT) res = !res;
            le_pi_set_bool_fast(img, inst->out, res);
            LE_STEP_OK;
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
        LE_OP_LBL(LE_OP_PID) case LE_OP_PID:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_pid(inst, mod, img, now_ms, scan_dt);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif

        case LE_OP_RESERVED_71:
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_SYM_COMP) case LE_OP_SYM_COMP:
#if LE_ENABLE_PROTECTION
        {
            le_status_t st = le_eval_sym_comp(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_DIST_21) case LE_OP_DIST_21:
            /* Marker: the active DIST_21 path is the variable-arity BLOCK
             * builtin LE_FUNC_DIST_21. */
            return LE_ERR_UNKNOWN_OPCODE;
        LE_OP_LBL(LE_OP_I2C) case LE_OP_I2C:
#if LE_ENABLE_SERIAL_BUS
        {
            le_status_t st = le_eval_i2c(inst, img, now_ms);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_SPI) case LE_OP_SPI:
#if LE_ENABLE_SERIAL_BUS
        {
            le_status_t st = le_eval_spi(inst, img, now_ms);
            if (st != LE_OK) return st;
            LE_STEP_OK;
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
        LE_OP_LBL(LE_OP_EXT_CALL) case LE_OP_EXT_CALL: {
            uint8_t func_id = mod; /* function ID passed in modifier */
            const uint16_t args[3] = { inst->in_a, inst->in_b, inst->out };
            if (g_le_hal && g_le_hal->ext_call)
            {
                return g_le_hal->ext_call(func_id, args, 2, 1, img);
            }
            le_status_t st = le_custom_nodes_dispatch(func_id, args, 2, 1, img);
            if (st != LE_ERR_UNKNOWN_OPCODE) {
                return st;
            }
            /* Default fallback: NOP if no board handler registered */
            LE_STEP_OK;
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
        LE_OP_LBL(LE_OP_LPF_1P) case LE_OP_LPF_1P:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_lpf_1p(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_BIQUAD_IIR) case LE_OP_BIQUAD_IIR:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_biquad_iir(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_MOVING_AVG) case LE_OP_MOVING_AVG:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_moving_avg(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_RATE_LIMITER) case LE_OP_RATE_LIMITER:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_rate_limiter(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_DEADBAND) case LE_OP_DEADBAND:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_deadband(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_WASHOUT) case LE_OP_WASHOUT:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_washout(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_PEAK_DETECTOR) case LE_OP_PEAK_DETECTOR:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_peak_detector(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_RMS) case LE_OP_RMS:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_rms(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_MEDIAN) case LE_OP_MEDIAN:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_median(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_DERIVATIVE) case LE_OP_DERIVATIVE:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_derivative(inst, mod, img, scan_srate);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_ZERO_CROSSING) case LE_OP_ZERO_CROSSING:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_zero_crossing(inst, mod, img, scan_srate);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_LUT_1D) case LE_OP_LUT_1D:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_lut_1d(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_TOTALIZER) case LE_OP_TOTALIZER:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_totalizer(inst, mod, img, scan_dt);
            if (st != LE_OK) return st;
            LE_STEP_OK;
        }
#else
        {
            (void)mod;
            return LE_ERR_UNKNOWN_OPCODE;
        }
#endif
        LE_OP_LBL(LE_OP_MIN_MAX_HOLD) case LE_OP_MIN_MAX_HOLD:
#if LE_ENABLE_DSP
        {
            le_status_t st = le_eval_min_max_hold(inst, mod, img);
            if (st != LE_OK) return st;
            LE_STEP_OK;
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
        LE_OP_LBL(LE_OP_BLOCK) case LE_OP_BLOCK: {
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
            const uint16_t* args = (const uint16_t*)(desc + 1); /* args follow the header */

            le_status_t st = le_eval_block(mod, desc, args, img, now_ms, scan_dt, scan_srate);
            if (st != LE_OK) return st;
            LE_STEP_OK;
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

#if !LE_USE_THREADED_DISPATCH
    next_instruction:
        continue;
    }
#else
    }
    op_lbl_unknown:
        return LE_ERR_UNKNOWN_OPCODE;
#endif

    return LE_OK;
}
#undef LE_STEP_OK
#undef LE_OP_LBL
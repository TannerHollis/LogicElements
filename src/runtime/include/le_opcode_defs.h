/**
 * @file le_opcode_defs.h
 * @brief Opcode definitions, modifier flags, and variable-arity function handles.
 */

#ifndef LE_OPCODE_DEFS_H
#define LE_OPCODE_DEFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Opcodes                                                                    */
/* ========================================================================== */

typedef enum {
    /* 0x00 - 0x1F: Digital Combinational Logic */
    LE_OP_NOP               = 0x00,
    LE_OP_MOVE              = 0x01,  /* Direct assignment out = in_a */
    LE_OP_NOT               = 0x02,  /* out = !in_a */
    LE_OP_AND               = 0x03,  /* out = in_a && in_b */
    LE_OP_OR                = 0x04,  /* out = in_a || in_b */
    LE_OP_XOR               = 0x05,  /* out = in_a ^ in_b */
    LE_OP_NAND              = 0x06,  /* out = !(in_a && in_b) */
    LE_OP_NOR               = 0x07,  /* out = !(in_a || in_b) */
    LE_OP_MUX               = 0x08,  /* out = sel ? in_b : in_a */
    /* 0x09 - 0x0C: Bit-Parallel 32-Bit Word Logic */
    LE_OP_WORD_AND          = 0x09,  /* out_w32 = in_a_w32 & in_b_w32 (32-bit bit-parallel AND) */
    LE_OP_WORD_OR           = 0x0A,  /* out_w32 = in_a_w32 | in_b_w32 (32-bit bit-parallel OR) */
    LE_OP_WORD_XOR          = 0x0B,  /* out_w32 = in_a_w32 ^ in_b_w32 (32-bit bit-parallel XOR) */
    LE_OP_WORD_NOT          = 0x0C,  /* out_w32 = ~in_a_w32           (32-bit bit-parallel NOT) */
    LE_OP_RESERVED_0D       = 0x0D,
    LE_OP_RESERVED_0E       = 0x0E,
    LE_OP_RESERVED_0F       = 0x0F,
    LE_OP_RESERVED_10       = 0x10,
    LE_OP_RESERVED_11       = 0x11,
    LE_OP_RESERVED_12       = 0x12,
    LE_OP_RESERVED_13       = 0x13,
    LE_OP_RESERVED_14       = 0x14,
    LE_OP_RESERVED_15       = 0x15,
    LE_OP_RESERVED_16       = 0x16,
    LE_OP_RESERVED_17       = 0x17,
    /* 0x18 - 0x1D: 32-Bit Signed Integer Arithmetic */
    LE_OP_MOVE_I            = 0x18,  /* out_i = in_a_i (32-bit int move) */
    LE_OP_ADD_I             = 0x19,  /* out_i = in_a_i + in_b_i (32-bit int add) */
    LE_OP_SUB_I             = 0x1A,  /* out_i = in_a_i - in_b_i (32-bit int sub) */
    LE_OP_MUL_I             = 0x1B,  /* out_i = in_a_i * in_b_i (32-bit int mul) */
    LE_OP_DIV_I             = 0x1C,  /* out_i = (in_b_i != 0) ? (in_a_i / in_b_i) : 0 (32-bit int div) */
    LE_OP_NEG_I             = 0x1D,  /* out_i = -in_a_i (32-bit int neg) */
    LE_OP_LOAD_I            = 0x1E,  /* out_i = imm32 (in_a | (in_b << 16)) */
    LE_OP_RESERVED_1F       = 0x1F,

    /* 0x20 - 0x2F: Edge Detection & Latches */
    LE_OP_RTRIG             = 0x20,  /* Rising edge pulse */
    LE_OP_FTRIG             = 0x21,  /* Falling edge pulse */
    LE_OP_SR                = 0x22,  /* Set-Reset Latch (Set dominant) */
    LE_OP_RS                = 0x23,  /* Reset-Set Latch (Reset dominant) */
    LE_OP_RESERVED_24       = 0x24,
    LE_OP_RESERVED_25       = 0x25,
    LE_OP_RESERVED_26       = 0x26,
    LE_OP_RESERVED_27       = 0x27,
    LE_OP_RESERVED_28       = 0x28,
    LE_OP_RESERVED_29       = 0x29,
    LE_OP_RESERVED_2A       = 0x2A,
    LE_OP_RESERVED_2B       = 0x2B,
    LE_OP_RESERVED_2C       = 0x2C,
    LE_OP_RESERVED_2D       = 0x2D,
    LE_OP_RESERVED_2E       = 0x2E,
    LE_OP_RESERVED_2F       = 0x2F,

    /* 0x30 - 0x3F: Timers and Counters */
    LE_OP_TON               = 0x30,  /* On-Delay Timer */
    LE_OP_TOF               = 0x31,  /* Off-Delay Timer */
    LE_OP_TP                = 0x32,  /* Pulse Timer */
    LE_OP_CTU               = 0x33,  /* Count Up */
    LE_OP_CTD               = 0x34,  /* Count Down */
    LE_OP_CTUD              = 0x35,  /* Count Up/Down */
    LE_OP_COUNTER_CV        = 0x36,  /* Counter Current Value -> %INT_REG */
    LE_OP_COUNTER_QD        = 0x37,  /* Counter Down Reached -> %BOOL_REG */
    LE_OP_TIMER_ET          = 0x38,  /* Timer Elapsed Time -> %INT_REG */
    LE_OP_RESERVED_39       = 0x39,
    LE_OP_RESERVED_3A       = 0x3A,
    LE_OP_RESERVED_3B       = 0x3B,
    LE_OP_RESERVED_3C       = 0x3C,
    LE_OP_RESERVED_3D       = 0x3D,
    LE_OP_RESERVED_3E       = 0x3E,
    LE_OP_RESERVED_3F       = 0x3F,

    /* 0x40 - 0x5F: Float Arithmetic & Math */
    LE_OP_MOVE_F            = 0x40,  /* out_f = in_a_f */
    LE_OP_ADD_F             = 0x41,  /* out_f = in_a_f + in_b_f */
    LE_OP_SUB_F             = 0x42,  /* out_f = in_a_f - in_b_f */
    LE_OP_MUL_F             = 0x43,  /* out_f = in_a_f * in_b_f */
    LE_OP_DIV_F             = 0x44,  /* out_f = in_a_f / in_b_f */
    LE_OP_NEG_F             = 0x45,  /* out_f = -in_a_f */
    LE_OP_ABS_F             = 0x46,  /* out_f = fabsf(in_a_f) */
    LE_OP_MIN_F             = 0x47,  /* out_f = fminf(in_a_f, in_b_f) */
    LE_OP_MAX_F             = 0x48,  /* out_f = fmaxf(in_a_f, in_b_f) */
    LE_OP_CLAMP_F           = 0x49,  /* clamp with min/max */
    LE_OP_SCALE_F           = 0x4A,  /* linear scaling */

    /* 0x4B - 0x4F: Complex Arithmetic */
    LE_OP_CADD_F            = 0x4B,  /* out_c = in_a_c + in_b_c  (complex add) */
    LE_OP_CSUB_F            = 0x4C,  /* out_c = in_a_c - in_b_c  (complex sub) */
    LE_OP_CMUL_F            = 0x4D,  /* out_c = in_a_c * in_b_c  (complex mul) */
    LE_OP_CDIV_F            = 0x4E,  /* out_c = in_a_c / in_b_c  (complex div) */
    LE_OP_MOVE_C            = 0x4F,  /* out_c = in_a_c (complex move) */
    LE_OP_FMA_F             = 0x50,  /* out_f = (in_a_f * in_b_f) + out_f (fused multiply-accumulate) */
    LE_OP_FMS_F             = 0x51,  /* out_f = out_f - (in_a_f * in_b_f) (fused multiply-subtract) */
    LE_OP_LOAD_F            = 0x52,  /* out_f = imm32_as_float (in_a | (in_b << 16)) */
    LE_OP_SQRT_F            = 0x53,  /* out_f = sqrt(in_a_f) */
    LE_OP_SIN_F             = 0x54,  /* out_f = sin(in_a_f) */
    LE_OP_COS_F             = 0x55,  /* out_f = cos(in_a_f) */
    LE_OP_TAN_F             = 0x56,  /* out_f = tan(in_a_f) */
    LE_OP_ASIN_F            = 0x57,  /* out_f = asin(in_a_f) */
    LE_OP_ACOS_F            = 0x58,  /* out_f = acos(in_a_f) */
    LE_OP_ATAN_F            = 0x59,  /* out_f = atan(in_a_f) */
    LE_OP_ATAN2_F           = 0x5A,  /* out_f = atan2(in_a_f, in_b_f) */
    LE_OP_EXP_F             = 0x5B,  /* out_f = exp(in_a_f) */
    LE_OP_LOG_F             = 0x5C,  /* out_f = ln(in_a_f) */
    LE_OP_LOG10_F           = 0x5D,  /* out_f = log10(in_a_f) */
    LE_OP_POW_F             = 0x5E,  /* out_f = pow(in_a_f, in_b_f) */
    LE_OP_RESERVED_5F       = 0x5F,

    /* 0x60 - 0x6F: Comparison Operations (Float -> Bool) */
    LE_OP_CMP_GT            = 0x60,  /* out_bool = in_a_f > in_b_f */
    LE_OP_CMP_LT            = 0x61,  /* out_bool = in_a_f < in_b_f */
    LE_OP_CMP_GE            = 0x62,  /* out_bool = in_a_f >= in_b_f */
    LE_OP_CMP_LE            = 0x63,  /* out_bool = in_a_f <= in_b_f */
    LE_OP_CMP_EQ            = 0x64,  /* out_bool = in_a_f == in_b_f */
    LE_OP_CMP_NE            = 0x65,  /* out_bool = in_a_f != in_b_f */
    LE_OP_RESERVED_66       = 0x66,
    LE_OP_RESERVED_67       = 0x67,
    LE_OP_RESERVED_68       = 0x68,
    LE_OP_RESERVED_69       = 0x69,
    LE_OP_RESERVED_6A       = 0x6A,
    LE_OP_RESERVED_6B       = 0x6B,
    LE_OP_RESERVED_6C       = 0x6C,
    LE_OP_RESERVED_6D       = 0x6D,
    LE_OP_RESERVED_6E       = 0x6E,
    LE_OP_RESERVED_6F       = 0x6F,

    /* 0x70 - 0x75: Control, Protection & Serial */
    LE_OP_PID               = 0x70,  /* Closed-loop PID Controller (DSP/control) */
    LE_OP_RESERVED_71       = 0x71,  /* Formerly scalar LE_OP_OVERCURRENT (now LE_FUNC_OVERCURRENT_51) */
    LE_OP_SYM_COMP          = 0x72,  /* 3-Phase Symmetrical Components (protection) */
    LE_OP_DIST_21           = 0x73,  /* Mho Distance Relay Zone (protection; block builtin marker) */
    LE_OP_I2C               = 0x74,  /* I2C Master Transaction Block (serial bus) */
    LE_OP_SPI               = 0x75,  /* SPI Master Transaction Block (serial bus) */
    LE_OP_RESERVED_76       = 0x76,
    LE_OP_RESERVED_77       = 0x77,
    LE_OP_RESERVED_78       = 0x78,
    LE_OP_RESERVED_79       = 0x79,
    LE_OP_RESERVED_7A       = 0x7A,
    LE_OP_RESERVED_7B       = 0x7B,
    LE_OP_RESERVED_7C       = 0x7C,
    LE_OP_RESERVED_7D       = 0x7D,
    LE_OP_RESERVED_7E       = 0x7E,
    LE_OP_RESERVED_7F       = 0x7F,

    /* 0x80 - 0x9F: Extended + Digital Signal Processing (DSP) & Filters */
    LE_OP_EXT_CALL          = 0x80,  /**< Calls a board-specific custom node or BSP routine (modifier = function_id). */
    LE_OP_RESERVED_81       = 0x81,
    LE_OP_RESERVED_82       = 0x82,
    LE_OP_RESERVED_83       = 0x83,
    LE_OP_RESERVED_84       = 0x84,
    LE_OP_RESERVED_85       = 0x85,
    LE_OP_RESERVED_86       = 0x86,
    LE_OP_RESERVED_87       = 0x87,
    LE_OP_RESERVED_88       = 0x88,
    LE_OP_RESERVED_89       = 0x89,
    LE_OP_RESERVED_8A       = 0x8A,
    LE_OP_RESERVED_8B       = 0x8B,
    LE_OP_RESERVED_8C       = 0x8C,
    LE_OP_RESERVED_8D       = 0x8D,
    LE_OP_RESERVED_8E       = 0x8E,
    LE_OP_RESERVED_8F       = 0x8F,
    LE_OP_LPF_1P            = 0x90,  /**< Single-Pole Low-Pass Filter (EWMA). */
    LE_OP_BIQUAD_IIR        = 0x91,  /**< 2nd-Order Biquad IIR Filter (Direct Form II). */
    LE_OP_MOVING_AVG        = 0x92,  /**< N-Tap Moving Average Filter. */
    LE_OP_RATE_LIMITER      = 0x93,  /**< Slew Rate Limiter. */
    LE_OP_DEADBAND          = 0x94,  /**< Deadband / Noise Threshold Filter. */
    LE_OP_WASHOUT           = 0x95,  /**< Washout / High-Pass Derivative Filter. */
    LE_OP_PEAK_DETECTOR     = 0x96,  /**< Peak / Envelope Follower. */
    LE_OP_RMS               = 0x97,  /**< True RMS Meter. */
    LE_OP_MEDIAN            = 0x98,  /**< Median Filter for spike/glitch rejection. */
    LE_OP_DERIVATIVE        = 0x99,  /**< Filtered Derivative (Rate of Change). */
    LE_OP_ZERO_CROSSING     = 0x9A,  /**< Zero-Crossing Detector & Frequency Counter. */
    LE_OP_LUT_1D            = 0x9B,  /**< 1D Piecewise Linear Lookup Table. */
    LE_OP_TOTALIZER         = 0x9C,  /**< Rate Totalizer / Numerical Integrator with Reset. */
    LE_OP_MIN_MAX_HOLD      = 0x9D,  /**< Min / Max Peak Hold with Reset. */
    LE_OP_RESERVED_9E       = 0x9E,
    LE_OP_RESERVED_9F       = 0x9F,

    /* 0xA0 - 0xFE: Variable-arity blocks + reserved for future use */
    LE_OP_BLOCK             = 0xA0,  /**< Variable-arity block call (MUX, N-in/M-out custom node). See le_block_desc_t. */
    LE_OP_RESERVED_A1       = 0xA1,
    LE_OP_RESERVED_A2       = 0xA2,
    LE_OP_RESERVED_A3       = 0xA3,
    LE_OP_RESERVED_A4       = 0xA4,
    LE_OP_RESERVED_A5       = 0xA5,
    LE_OP_RESERVED_A6       = 0xA6,
    LE_OP_RESERVED_A7       = 0xA7,
    LE_OP_RESERVED_A8       = 0xA8,
    LE_OP_RESERVED_A9       = 0xA9,
    LE_OP_RESERVED_AA       = 0xAA,
    LE_OP_RESERVED_AB       = 0xAB,
    LE_OP_RESERVED_AC       = 0xAC,
    LE_OP_RESERVED_AD       = 0xAD,
    LE_OP_RESERVED_AE       = 0xAE,
    LE_OP_RESERVED_AF       = 0xAF,
    LE_OP_RESERVED_B0       = 0xB0,
    LE_OP_RESERVED_B1       = 0xB1,
    LE_OP_RESERVED_B2       = 0xB2,
    LE_OP_RESERVED_B3       = 0xB3,
    LE_OP_RESERVED_B4       = 0xB4,
    LE_OP_RESERVED_B5       = 0xB5,
    LE_OP_RESERVED_B6       = 0xB6,
    LE_OP_RESERVED_B7       = 0xB7,
    LE_OP_RESERVED_B8       = 0xB8,
    LE_OP_RESERVED_B9       = 0xB9,
    LE_OP_RESERVED_BA       = 0xBA,
    LE_OP_RESERVED_BB       = 0xBB,
    LE_OP_RESERVED_BC       = 0xBC,
    LE_OP_RESERVED_BD       = 0xBD,
    LE_OP_RESERVED_BE       = 0xBE,
    LE_OP_RESERVED_BF       = 0xBF,
    LE_OP_RESERVED_C0       = 0xC0,
    LE_OP_RESERVED_C1       = 0xC1,
    LE_OP_RESERVED_C2       = 0xC2,
    LE_OP_RESERVED_C3       = 0xC3,
    LE_OP_RESERVED_C4       = 0xC4,
    LE_OP_RESERVED_C5       = 0xC5,
    LE_OP_RESERVED_C6       = 0xC6,
    LE_OP_RESERVED_C7       = 0xC7,
    LE_OP_RESERVED_C8       = 0xC8,
    LE_OP_RESERVED_C9       = 0xC9,
    LE_OP_RESERVED_CA       = 0xCA,
    LE_OP_RESERVED_CB       = 0xCB,
    LE_OP_RESERVED_CC       = 0xCC,
    LE_OP_RESERVED_CD       = 0xCD,
    LE_OP_RESERVED_CE       = 0xCE,
    LE_OP_RESERVED_CF       = 0xCF,
    LE_OP_RESERVED_D0       = 0xD0,
    LE_OP_RESERVED_D1       = 0xD1,
    LE_OP_RESERVED_D2       = 0xD2,
    LE_OP_RESERVED_D3       = 0xD3,
    LE_OP_RESERVED_D4       = 0xD4,
    LE_OP_RESERVED_D5       = 0xD5,
    LE_OP_RESERVED_D6       = 0xD6,
    LE_OP_RESERVED_D7       = 0xD7,
    LE_OP_RESERVED_D8       = 0xD8,
    LE_OP_RESERVED_D9       = 0xD9,
    LE_OP_RESERVED_DA       = 0xDA,
    LE_OP_RESERVED_DB       = 0xDB,
    LE_OP_RESERVED_DC       = 0xDC,
    LE_OP_RESERVED_DD       = 0xDD,
    LE_OP_RESERVED_DE       = 0xDE,
    LE_OP_RESERVED_DF       = 0xDF,
    LE_OP_RESERVED_E0       = 0xE0,
    LE_OP_RESERVED_E1       = 0xE1,
    LE_OP_RESERVED_E2       = 0xE2,
    LE_OP_RESERVED_E3       = 0xE3,
    LE_OP_RESERVED_E4       = 0xE4,
    LE_OP_RESERVED_E5       = 0xE5,
    LE_OP_RESERVED_E6       = 0xE6,
    LE_OP_RESERVED_E7       = 0xE7,
    LE_OP_RESERVED_E8       = 0xE8,
    LE_OP_RESERVED_E9       = 0xE9,
    LE_OP_RESERVED_EA       = 0xEA,
    LE_OP_RESERVED_EB       = 0xEB,
    LE_OP_RESERVED_EC       = 0xEC,
    LE_OP_RESERVED_ED       = 0xED,
    LE_OP_RESERVED_EE       = 0xEE,
    LE_OP_RESERVED_EF       = 0xEF,
    LE_OP_RESERVED_F0       = 0xF0,
    LE_OP_RESERVED_F1       = 0xF1,
    LE_OP_RESERVED_F2       = 0xF2,
    LE_OP_RESERVED_F3       = 0xF3,
    LE_OP_RESERVED_F4       = 0xF4,
    LE_OP_RESERVED_F5       = 0xF5,
    LE_OP_RESERVED_F6       = 0xF6,
    LE_OP_RESERVED_F7       = 0xF7,
    LE_OP_RESERVED_F8       = 0xF8,
    LE_OP_RESERVED_F9       = 0xF9,
    LE_OP_RESERVED_FA       = 0xFA,
    LE_OP_RESERVED_FB       = 0xFB,
    LE_OP_RESERVED_FC       = 0xFC,
    LE_OP_RESERVED_FD       = 0xFD,
    LE_OP_RESERVED_FE       = 0xFE,

    LE_OP_END               = 0xFF   /**< End of execution table marker. */
} le_opcode_t;

/* Modifier Flags for Instructions */
#define LE_MOD_NONE             0x00
#define LE_MOD_INVERT_A         (1U << 0)  /* Invert Input A before evaluating */
#define LE_MOD_INVERT_B         (1U << 1)  /* Invert Input B before evaluating */
#define LE_MOD_INVERT_OUT       (1U << 2)  /* Invert Output */

/* ========================================================================== */
/* Variable-arity block function identifiers (modifier of LE_OP_BLOCK)        */
/* ========================================================================== */
#define LE_FUNC_NONE             0x00
#define LE_FUNC_MUX_SELECT       0x01   /* [sel, in0, in1] -> [out]: out = args[2] ? args[1] : args[0] */
#define LE_FUNC_RECT2POLAR       0x02   /* [real, imag] -> [mag, angle_rad] */
#define LE_FUNC_POLAR2RECT       0x03   /* [mag, angle_rad] -> [real, imag] */
#define LE_FUNC_PHASOR_SHIFT     0x04   /* [real, imag, delta_rad] -> [real', imag']: CCW rotation by delta */
#define LE_FUNC_COMPLEX2POLAR    0x05   /* [cplx] -> [mag, angle_rad] */
#define LE_FUNC_COMPLEX2RECT     0x06   /* [cplx] -> [real, imag] */
#define LE_FUNC_RECT2COMPLEX     0x07   /* [real, imag] -> [cplx] */
#define LE_FUNC_POLAR2COMPLEX    0x08   /* [mag, angle_rad] -> [cplx] */
#define LE_FUNC_COMPLEX_MUL      0x09   /* [c_a, c_b] -> [c_out] */

#define LE_FUNC_CLAMP_F          0x0A   /* [value, min, max] -> [out]: clamp value to [min, max] */

#define LE_FUNC_PHASOR_1P        0x0B   /* [sample, sync_cplx] -> [cplx]: synced phasor extractor */
#define LE_FUNC_DIFF_87          0x0C   /* [cph0..cphN-1] -> [bool]: N-input dual-slope differential (ANSI 87) */
#define LE_FUNC_DIST_21          0x0D   /* [v_c, i_c, offset_on] -> [bool]: mho distance (21) with prefault V memory */
#define LE_FUNC_PHASE_COMP       0x0E   /* [c_a, c_b, c_c] -> [c_a', c_b', c_c']: 3p transformer phase-shift compensation (ANSI 87T) */
#define LE_FUNC_OVERCURRENT_51   0x0F   /* [i_c, enable] -> [bool]: IEC/IEEE inverse-time overcurrent (ANSI 51) */

#define LE_FUNC_PHASOR_3P        0x10   /* [a,b,c,sync,freq_hz] -> [pa,pb,pc]: 3-phase synced phasor extractor */
#define LE_FUNC_FREQ_EST         0x11   /* [sample] -> [freq_hz, valid]: ANSI 81 dynamic frequency estimator */

#define LE_FUNC_MUX_SELECT_F     0x12   /* [sel (bool), in0 (float), in1 (float)] -> [out (float)] */
#define LE_FUNC_MUX_SELECT_I     0x13   /* [sel (bool), in0 (int32), in1 (int32)] -> [out (int32)] */
#define LE_FUNC_MUX_SELECT_C     0x14   /* [sel (bool), in0 (cmplx), in1 (cmplx)] -> [out (cmplx)] */

/* Reserved block func handles (0x15 - 0x7F) */
#define LE_FUNC_RESERVED_15      0x15
#define LE_FUNC_RESERVED_16      0x16
#define LE_FUNC_RESERVED_17      0x17
#define LE_FUNC_RESERVED_18      0x18
#define LE_FUNC_RESERVED_19      0x19
#define LE_FUNC_RESERVED_1A      0x1A
#define LE_FUNC_RESERVED_1B      0x1B
#define LE_FUNC_RESERVED_1C      0x1C
#define LE_FUNC_RESERVED_1D      0x1D
#define LE_FUNC_RESERVED_1E      0x1E
#define LE_FUNC_RESERVED_1F      0x1F
#define LE_FUNC_RESERVED_20      0x20
#define LE_FUNC_RESERVED_21      0x21
#define LE_FUNC_RESERVED_22      0x22
#define LE_FUNC_RESERVED_23      0x23
#define LE_FUNC_RESERVED_24      0x24
#define LE_FUNC_RESERVED_25      0x25
#define LE_FUNC_RESERVED_26      0x26
#define LE_FUNC_RESERVED_27      0x27
#define LE_FUNC_RESERVED_28      0x28
#define LE_FUNC_RESERVED_29      0x29
#define LE_FUNC_RESERVED_2A      0x2A
#define LE_FUNC_RESERVED_2B      0x2B
#define LE_FUNC_RESERVED_2C      0x2C
#define LE_FUNC_RESERVED_2D      0x2D
#define LE_FUNC_RESERVED_2E      0x2E
#define LE_FUNC_RESERVED_2F      0x2F
#define LE_FUNC_RESERVED_30      0x30
#define LE_FUNC_RESERVED_31      0x31
#define LE_FUNC_RESERVED_32      0x32
#define LE_FUNC_RESERVED_33      0x33
#define LE_FUNC_RESERVED_34      0x34
#define LE_FUNC_RESERVED_35      0x35
#define LE_FUNC_RESERVED_36      0x36
#define LE_FUNC_RESERVED_37      0x37
#define LE_FUNC_RESERVED_38      0x38
#define LE_FUNC_RESERVED_39      0x39
#define LE_FUNC_RESERVED_3A      0x3A
#define LE_FUNC_RESERVED_3B      0x3B
#define LE_FUNC_RESERVED_3C      0x3C
#define LE_FUNC_RESERVED_3D      0x3D
#define LE_FUNC_RESERVED_3E      0x3E
#define LE_FUNC_RESERVED_3F      0x3F
#define LE_FUNC_RESERVED_40      0x40
#define LE_FUNC_RESERVED_41      0x41
#define LE_FUNC_RESERVED_42      0x42
#define LE_FUNC_RESERVED_43      0x43
#define LE_FUNC_RESERVED_44      0x44
#define LE_FUNC_RESERVED_45      0x45
#define LE_FUNC_RESERVED_46      0x46
#define LE_FUNC_RESERVED_47      0x47
#define LE_FUNC_RESERVED_48      0x48
#define LE_FUNC_RESERVED_49      0x49
#define LE_FUNC_RESERVED_4A      0x4A
#define LE_FUNC_RESERVED_4B      0x4B
#define LE_FUNC_RESERVED_4C      0x4C
#define LE_FUNC_RESERVED_4D      0x4D
#define LE_FUNC_RESERVED_4E      0x4E
#define LE_FUNC_RESERVED_4F      0x4F
#define LE_FUNC_RESERVED_50      0x50
#define LE_FUNC_RESERVED_51      0x51
#define LE_FUNC_RESERVED_52      0x52
#define LE_FUNC_RESERVED_53      0x53
#define LE_FUNC_RESERVED_54      0x54
#define LE_FUNC_RESERVED_55      0x55
#define LE_FUNC_RESERVED_56      0x56
#define LE_FUNC_RESERVED_57      0x57
#define LE_FUNC_RESERVED_58      0x58
#define LE_FUNC_RESERVED_59      0x59
#define LE_FUNC_RESERVED_5A      0x5A
#define LE_FUNC_RESERVED_5B      0x5B
#define LE_FUNC_RESERVED_5C      0x5C
#define LE_FUNC_RESERVED_5D      0x5D
#define LE_FUNC_RESERVED_5E      0x5E
#define LE_FUNC_RESERVED_5F      0x5F
#define LE_FUNC_RESERVED_60      0x60
#define LE_FUNC_RESERVED_61      0x61
#define LE_FUNC_RESERVED_62      0x62
#define LE_FUNC_RESERVED_63      0x63
#define LE_FUNC_RESERVED_64      0x64
#define LE_FUNC_RESERVED_65      0x65
#define LE_FUNC_RESERVED_66      0x66
#define LE_FUNC_RESERVED_67      0x67
#define LE_FUNC_RESERVED_68      0x68
#define LE_FUNC_RESERVED_69      0x69
#define LE_FUNC_RESERVED_6A      0x6A
#define LE_FUNC_RESERVED_6B      0x6B
#define LE_FUNC_RESERVED_6C      0x6C
#define LE_FUNC_RESERVED_6D      0x6D
#define LE_FUNC_RESERVED_6E      0x6E
#define LE_FUNC_RESERVED_6F      0x6F
#define LE_FUNC_RESERVED_70      0x70
#define LE_FUNC_RESERVED_71      0x71
#define LE_FUNC_RESERVED_72      0x72
#define LE_FUNC_RESERVED_73      0x73
#define LE_FUNC_RESERVED_74      0x74
#define LE_FUNC_RESERVED_75      0x75
#define LE_FUNC_RESERVED_76      0x76
#define LE_FUNC_RESERVED_77      0x77
#define LE_FUNC_RESERVED_78      0x78
#define LE_FUNC_RESERVED_79      0x79
#define LE_FUNC_RESERVED_7A      0x7A
#define LE_FUNC_RESERVED_7B      0x7B
#define LE_FUNC_RESERVED_7C      0x7C
#define LE_FUNC_RESERVED_7D      0x7D
#define LE_FUNC_RESERVED_7E      0x7E
#define LE_FUNC_RESERVED_7F      0x7F
#define LE_FUNC_CUSTOM_BASE      0x80   /* func_id >= this dispatches to the HAL ext_call */

#ifdef __cplusplus
}
#endif

#endif /* LE_OPCODE_DEFS_H */

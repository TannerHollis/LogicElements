/**
 * @file le_types.h
 * @brief Core types, opcodes, memory layout, and binary format definitions for LogicElements C Runtime.
 */

#ifndef LE_TYPES_H
#define LE_TYPES_H

#include "le_complex.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Compile-Time Capacity Limits (Configurable per target microcontroller)    */
/* ========================================================================== */

#ifndef LE_MAX_DIGITAL_IN
#define LE_MAX_DIGITAL_IN       64      /* Up to 64 digital inputs */
#endif

#ifndef LE_MAX_DIGITAL_OUT
#define LE_MAX_DIGITAL_OUT      64      /* Up to 64 digital outputs */
#endif

#ifndef LE_MAX_BOOL_REGS
#define LE_MAX_BOOL_REGS        256     /* Up to 256 internal boolean registers */
#endif

#ifndef LE_MAX_FLOATS
#define LE_MAX_FLOATS           128     /* Up to 128 32-bit float registers */
#endif

#ifndef LE_MAX_INT_REGS
#define LE_MAX_INT_REGS         64      /* Up to 64 32-bit integer registers */
#endif

/* ========================================================================== */
/* Optional Capability Switches                                               */
/* A board designer can reduce the runtime down toward boolean-only by setting */
/* every subsystem switch to 0; each guarded block then contributes no state,  */
/* opcodes, or registers.                                                      */
/*                                                                             */
/* Dependencies are AUTOMATICALLY satisfied - a subsystem cannot be built      */
/* without its prerequisites, so enabling a higher layer forces its lower      */
/* layers ON:                                                                  */
/*     LE_ENABLE_PROTECTION  =>  LE_ENABLE_COMPLEX  =>  LE_ENABLE_ANALOG       */
/*     LE_ENABLE_DSP         =>  LE_ENABLE_ANALOG                              */
/* Complex phasors / %C registers require analog channels to acquire their     */
/* signals; protection relays operate on phasors; DSP filters mix sampled      */
/* (analog) channels. Prerequisites are forced ON by the higher layer's own    */
/* switch - the higher-layer switch is the master control.                     */
/* ========================================================================== */
#ifndef LE_ENABLE_PROTECTION
#define LE_ENABLE_PROTECTION    1       /* P&C relay opcodes (OVERCURRENT, PID, PHASOR_1P, SYM_COMP, DIST_21, DIFF_87, PHASE_COMP) */
#endif

#ifndef LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX       1       /* Complex arithmetic, %C registers & conversions */
#endif

#ifndef LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG        1       /* Analog input channels (%AIN) and scaling */
#endif

#ifndef LE_ENABLE_SERIAL_BUS
#define LE_ENABLE_SERIAL_BUS    1       /* I2C and SPI bus opcodes */
#endif

#ifndef LE_ENABLE_DSP
#define LE_ENABLE_DSP           1       /* DSP and filter opcodes */
#endif

/* ---- Prerequisite implication: lower layers are forced ON ---------------- */
#if LE_ENABLE_PROTECTION
#undef  LE_ENABLE_COMPLEX
#define LE_ENABLE_COMPLEX       1       /* protection runs on phasors -> complex must compile */
#endif
#if LE_ENABLE_COMPLEX
#undef  LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG        1       /* complex phasors need analog acquisition -> analog must compile */
#endif
#if LE_ENABLE_DSP
#undef  LE_ENABLE_ANALOG
#define LE_ENABLE_ANALOG        1       /* DSP filters mix sampled channels -> analog must compile */
#endif

/* ========================================================================== */
/* Subsystem capacity limits (only defined when the subsystem is enabled)      */
/* ========================================================================== */
#if LE_ENABLE_ANALOG
#ifndef LE_MAX_ANALOG_IN
#define LE_MAX_ANALOG_IN        16      /* Up to 16 Analog Inputs (ADC channels) */
#endif
#endif

#if LE_ENABLE_COMPLEX
#ifndef LE_MAX_COMPLEX
#define LE_MAX_COMPLEX          64      /* Up to 64 complex registers (%C, real+imag pairs) */
#endif
#endif

#if LE_ENABLE_PROTECTION
#ifndef LE_MAX_RAW_SAMPLES
#define LE_MAX_RAW_SAMPLES 64           /* Raw sample buffer for high-rate acquisition (DFT window upper bound) */
#endif
#ifndef LE_MAX_SAMPLES_PER_CYCLE
#define LE_MAX_SAMPLES_PER_CYCLE LE_MAX_RAW_SAMPLES /* Backwards-compatible alias */
#endif
#endif

#ifndef LE_MAX_ALIASES
#define LE_MAX_ALIASES          32      /* Up to 32 user-declared register aliases (+ board aliases) */
#endif

#if LE_ENABLE_SERIAL_BUS
/* No static device-count limits: serial-bus device state is baked into the
 * preconfigured state image and copied into the state workspace at load,
 * bounded only by LE_RAM_WORKSPACE_BYTES. */
#endif

#if LE_ENABLE_DSP
#ifndef LE_MOVING_AVG_MAX_WINDOW
#define LE_MOVING_AVG_MAX_WINDOW 32     /* Samples window buffer inside a moving-avg state */
#endif

#ifndef LE_RMS_MAX_WINDOW
#define LE_RMS_MAX_WINDOW       32      /* Samples window buffer inside a True RMS state */
#endif

#ifndef LE_MAX_MEDIAN_WINDOW
#define LE_MAX_MEDIAN_WINDOW    9       /* Samples window buffer inside a median filter state */
#endif

#ifndef LE_MAX_LUT_POINTS
#define LE_MAX_LUT_POINTS       16      /* Breakpoints buffer inside a 1D lookup table state */
#endif
#endif


/* ========================================================================== */
/* Binary Format Constants                                                    */
/* ========================================================================== */

#define LE_BIN_MAGIC            0x4C454231  /* ASCII "LEB1" */
#define LE_BIN_VERSION          11  // Incremented for PHASOR_1P dynamic freq input support

/* Alias table limits: an alias name is at most 7 characters (LE_ALIAS_NAME_MAX),
 * matching le_alias_t::name. Runtime pulse slots bound concurrent aliased pulses. */
#ifndef LE_ALIAS_NAME_MAX
#define LE_ALIAS_NAME_MAX       7
#endif
#ifndef LE_MAX_PULSES
#define LE_MAX_PULSES           8
#endif

/* ========================================================================== */
/* Fixed-Rate Scan Configuration (deterministic execution)                    */
/* The DESIGNER owns the scan rate: the circuit JSON declares `scan_rate_hz`, */
/* the compiler embeds it in the .lebin timing descriptor, and the loader      */
/* applies it to the VM clock (le_vm_set_scan_period_us) at load. The BOARD    */
/* only calibrates the cost model (LE_NS_PER_ABSTRACT_CYCLE) so the compiler / */
/* loader can verify the requested rate is achievable; if a rate fails, the   */
/* designer simply lowers it.                                                 */
/* ========================================================================== */
#ifndef LE_NS_PER_ABSTRACT_CYCLE
#define LE_NS_PER_ABSTRACT_CYCLE 0     /* Port-calibrated nanoseconds per abstract compiler cycle; 0 = no budget enforcement */
#endif
#ifndef LE_DEFAULT_SCAN_DT_SEC
#define LE_DEFAULT_SCAN_DT_SEC  0.01f  /* Historic fallback dt (seconds) when no scan rate is configured */
#endif

/* Program Header Flags */
#define LE_FLAG_AUTOSTART       (1U << 0)
#define LE_FLAG_WATCHDOG_EN     (1U << 1)

/* ========================================================================== */
/* Memory Address Encoding (16-bit)                                           */
/* Bits 15..12 encode the region type:                                       */
/*   0x0000 - 0x0FFF : Digital Inputs  (%IN)                                  */
/*   0x1000 - 0x1FFF : Digital Outputs (%OUT)                                 */
/*   0x2000 - 0x3FFF : Bool Registers  (%B)                                   */
/*   0x4000 - 0x7FFF : Float Registers (%F)                                   */
/*   0x8000 - 0x8FFF : Timer Status / Output Bits                            */
/*   0x9000 - 0x9FFF : Counter Status / Output Bits                          */
/*   0xA000 - 0xAFFF : Integer Registers (%I)                                */
/*   0xB000 - 0xBFFF : Analog Inputs    (%AIN)                               */
/*   0xC000 - 0xFFFF : Constants / Literals                                  */
/* ========================================================================== */

#define LE_ADDR_REGION_MASK     0xF000
#define LE_ADDR_INDEX_MASK      0x0FFF

#define LE_REGION_DIN           0x0000
#define LE_REGION_DOUT          0x1000
#define LE_REGION_BOOL_REG      0x2000
#define LE_REGION_BOOL_REG_EXT  0x3000
#define LE_REGION_FLOAT         0x4000
#define LE_REGION_FLOAT_EXT1    0x5000
#define LE_REGION_FLOAT_EXT2    0x6000
#define LE_REGION_FLOAT_EXT3    0x7000
#define LE_REGION_TIMER         0x8000
#define LE_REGION_COUNTER       0x9000
#define LE_REGION_INT_REG       0xA000
#define LE_REGION_AIN           0xB000
#define LE_REGION_CONST         0xC000
#if LE_ENABLE_COMPLEX
#define LE_REGION_CMPLX         0xD000   /* Complex registers (%C): 12-bit index, real+imag pair each */
#endif

/* Predefined Constants */
#define LE_CONST_FALSE          0xC000
#define LE_CONST_TRUE           0xC001
#define LE_CONST_ZERO_F         0xC002
#define LE_CONST_ONE_F          0xC003
#define LE_CONST_60_F           0xC004  /* 60 Hz default for PHASOR_1P freq_hz input */
#if LE_ENABLE_COMPLEX
#define LE_CONST_ZERO_C         0xC005   /* Complex zero (0+0j) */
#endif
#define LE_ADDR_UNUSED          0xFFFF

/* Helper Macros for Address Construction */
#define LE_ADDR_MAKE_DIN(idx)      ((uint16_t)(LE_REGION_DIN | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_DOUT(idx)     ((uint16_t)(LE_REGION_DOUT | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_BOOL_REG(idx) ((uint16_t)(LE_REGION_BOOL_REG | ((idx) & 0x1FFF)))
#define LE_ADDR_MAKE_FLOAT(idx)    ((uint16_t)(LE_REGION_FLOAT | ((idx) & 0x3FFF)))
#define LE_ADDR_MAKE_INT_REG(idx)  ((uint16_t)(LE_REGION_INT_REG | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_TIMER(idx)    ((uint16_t)(LE_REGION_TIMER | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_COUNTER(idx)  ((uint16_t)(LE_REGION_COUNTER | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_AIN(idx)      ((uint16_t)(LE_REGION_AIN | ((idx) & LE_ADDR_INDEX_MASK)))
#if LE_ENABLE_COMPLEX
#define LE_ADDR_MAKE_CMPLX(idx)    ((uint16_t)(LE_REGION_CMPLX | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_CMPLX_PAIR(idx) ((uint16_t)(LE_REGION_CMPLX | (((idx) & 0x0FFE))))  /* even index */
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
    /* Reserved for future logic ops (keeps the dispatch table dense). */
    LE_OP_RESERVED_09 = 0x09,
    LE_OP_RESERVED_0A = 0x0A,
    LE_OP_RESERVED_0B = 0x0B,
    LE_OP_RESERVED_0C = 0x0C,
    LE_OP_RESERVED_0D = 0x0D,
    LE_OP_RESERVED_0E = 0x0E,
    LE_OP_RESERVED_0F = 0x0F,
    LE_OP_RESERVED_10 = 0x10,
    LE_OP_RESERVED_11 = 0x11,
    LE_OP_RESERVED_12 = 0x12,
    LE_OP_RESERVED_13 = 0x13,
    LE_OP_RESERVED_14 = 0x14,
    LE_OP_RESERVED_15 = 0x15,
    LE_OP_RESERVED_16 = 0x16,
    LE_OP_RESERVED_17 = 0x17,
    LE_OP_RESERVED_18 = 0x18,
    LE_OP_RESERVED_19 = 0x19,
    LE_OP_RESERVED_1A = 0x1A,
    LE_OP_RESERVED_1B = 0x1B,
    LE_OP_RESERVED_1C = 0x1C,
    LE_OP_RESERVED_1D = 0x1D,
    LE_OP_RESERVED_1E = 0x1E,
    LE_OP_RESERVED_1F = 0x1F,

    /* 0x20 - 0x2F: Edge Detection & Latches */
    LE_OP_RTRIG             = 0x20,  /* Rising edge pulse */
    LE_OP_FTRIG             = 0x21,  /* Falling edge pulse */
    LE_OP_SR                = 0x22,  /* Set-Reset Latch (Set dominant) */
    LE_OP_RS                = 0x23,  /* Reset-Set Latch (Reset dominant) */
    LE_OP_RESERVED_24 = 0x24,
    LE_OP_RESERVED_25 = 0x25,
    LE_OP_RESERVED_26 = 0x26,
    LE_OP_RESERVED_27 = 0x27,
    LE_OP_RESERVED_28 = 0x28,
    LE_OP_RESERVED_29 = 0x29,
    LE_OP_RESERVED_2A = 0x2A,
    LE_OP_RESERVED_2B = 0x2B,
    LE_OP_RESERVED_2C = 0x2C,
    LE_OP_RESERVED_2D = 0x2D,
    LE_OP_RESERVED_2E = 0x2E,
    LE_OP_RESERVED_2F = 0x2F,

    /* 0x30 - 0x3F: Timers and Counters */
    LE_OP_TON               = 0x30,  /* On-Delay Timer */
    LE_OP_TOF               = 0x31,  /* Off-Delay Timer */
    LE_OP_TP                = 0x32,  /* Pulse Timer */
    LE_OP_CTU               = 0x33,  /* Count Up */
    LE_OP_CTD               = 0x34,  /* Count Down */
    LE_OP_CTUD              = 0x35,  /* Count Up/Down */
    LE_OP_RESERVED_36 = 0x36,
    LE_OP_RESERVED_37 = 0x37,
    LE_OP_RESERVED_38 = 0x38,
    LE_OP_RESERVED_39 = 0x39,
    LE_OP_RESERVED_3A = 0x3A,
    LE_OP_RESERVED_3B = 0x3B,
    LE_OP_RESERVED_3C = 0x3C,
    LE_OP_RESERVED_3D = 0x3D,
    LE_OP_RESERVED_3E = 0x3E,
    LE_OP_RESERVED_3F = 0x3F,

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

    /* 0x4B - 0x4F: Complex Arithmetic (T_CMPLX operands; always enumerated so
     * the dispatch table stays dense — execution is guarded with LE_ENABLE_COMPLEX) */
    LE_OP_CADD_F            = 0x4B,  /* out_c = in_a_c + in_b_c  (complex add) */
    LE_OP_CSUB_F            = 0x4C,  /* out_c = in_a_c - in_b_c  (complex sub) */
    LE_OP_CMUL_F            = 0x4D,  /* out_c = in_a_c * in_b_c  (complex mul) */
    LE_OP_CDIV_F            = 0x4E,  /* out_c = in_a_c / in_b_c  (complex div) */
    LE_OP_MOVE_C            = 0x4F,  /* out_c = in_a_c (complex move) */
    LE_OP_RESERVED_50 = 0x50,
    LE_OP_RESERVED_51 = 0x51,
    LE_OP_RESERVED_52 = 0x52,
    LE_OP_RESERVED_53 = 0x53,
    LE_OP_RESERVED_54 = 0x54,
    LE_OP_RESERVED_55 = 0x55,
    LE_OP_RESERVED_56 = 0x56,
    LE_OP_RESERVED_57 = 0x57,
    LE_OP_RESERVED_58 = 0x58,
    LE_OP_RESERVED_59 = 0x59,
    LE_OP_RESERVED_5A = 0x5A,
    LE_OP_RESERVED_5B = 0x5B,
    LE_OP_RESERVED_5C = 0x5C,
    LE_OP_RESERVED_5D = 0x5D,
    LE_OP_RESERVED_5E = 0x5E,
    LE_OP_RESERVED_5F = 0x5F,

    /* 0x60 - 0x6F: Comparison Operations (Float -> Bool) */
    LE_OP_CMP_GT            = 0x60,  /* out_bool = in_a_f > in_b_f */
    LE_OP_CMP_LT            = 0x61,  /* out_bool = in_a_f < in_b_f */
    LE_OP_CMP_GE            = 0x62,  /* out_bool = in_a_f >= in_b_f */
    LE_OP_CMP_LE            = 0x63,  /* out_bool = in_a_f <= in_b_f */
    LE_OP_CMP_EQ            = 0x64,  /* out_bool = in_a_f == in_b_f */
    LE_OP_CMP_NE            = 0x65,  /* out_bool = in_a_f != in_b_f */
    LE_OP_RESERVED_66 = 0x66,
    LE_OP_RESERVED_67 = 0x67,
    LE_OP_RESERVED_68 = 0x68,
    LE_OP_RESERVED_69 = 0x69,
    LE_OP_RESERVED_6A = 0x6A,
    LE_OP_RESERVED_6B = 0x6B,
    LE_OP_RESERVED_6C = 0x6C,
    LE_OP_RESERVED_6D = 0x6D,
    LE_OP_RESERVED_6E = 0x6E,
    LE_OP_RESERVED_6F = 0x6F,

    /* 0x70 - 0x75: Control, Protection & Serial (always enumerated for a dense
     * dispatch table; execution is guarded per feature switch). PID is a DSP/
     * control element (LE_ENABLE_DSP), not a protection relay. Phasor extraction,
     * complex<->float conversions AND the inverse-time overcurrent (ANSI 51) are
     * variable-arity BLOCK builtins (see LE_FUNC_PHASOR_1P / LE_FUNC_RECT2POLAR /
     * LE_FUNC_OVERCURRENT_51, etc.). */
    LE_OP_PID               = 0x70,  /* Closed-loop PID Controller (DSP/control) */
    LE_OP_RESERVED_71       = 0x71,  /* Formerly scalar LE_OP_OVERCURRENT (now the
                                      * LE_FUNC_OVERCURRENT_51 BLOCK builtin with a
                                      * complex phasor + enable input). Kept as a
                                      * placeholder so the table stays dense. */
    LE_OP_SYM_COMP          = 0x72,  /* 3-Phase Symmetrical Components (protection) */
    LE_OP_DIST_21           = 0x73,  /* Mho Distance Relay Zone (protection; block builtin marker) */
    LE_OP_I2C               = 0x74,  /* I2C Master Transaction Block (serial bus) */
    LE_OP_SPI               = 0x75,  /* SPI Master Transaction Block (serial bus) */
    LE_OP_RESERVED_76 = 0x76,
    LE_OP_RESERVED_77 = 0x77,
    LE_OP_RESERVED_78 = 0x78,
    LE_OP_RESERVED_79 = 0x79,
    LE_OP_RESERVED_7A = 0x7A,
    LE_OP_RESERVED_7B = 0x7B,
    LE_OP_RESERVED_7C = 0x7C,
    LE_OP_RESERVED_7D = 0x7D,
    LE_OP_RESERVED_7E = 0x7E,
    LE_OP_RESERVED_7F = 0x7F,

    /* 0x80 - 0x9F: Extended + Digital Signal Processing (DSP) & Filters */
    LE_OP_EXT_CALL          = 0x80,  /**< Calls a board-specific custom node or BSP routine (modifier = function_id). */
    LE_OP_RESERVED_81 = 0x81,
    LE_OP_RESERVED_82 = 0x82,
    LE_OP_RESERVED_83 = 0x83,
    LE_OP_RESERVED_84 = 0x84,
    LE_OP_RESERVED_85 = 0x85,
    LE_OP_RESERVED_86 = 0x86,
    LE_OP_RESERVED_87 = 0x87,
    LE_OP_RESERVED_88 = 0x88,
    LE_OP_RESERVED_89 = 0x89,
    LE_OP_RESERVED_8A = 0x8A,
    LE_OP_RESERVED_8B = 0x8B,
    LE_OP_RESERVED_8C = 0x8C,
    LE_OP_RESERVED_8D = 0x8D,
    LE_OP_RESERVED_8E = 0x8E,
    LE_OP_RESERVED_8F = 0x8F,
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
    LE_OP_RESERVED_9E = 0x9E,
    LE_OP_RESERVED_9F = 0x9F,

    /* 0xA0 - 0xFE: Variable-arity blocks + reserved for future use */
    LE_OP_BLOCK             = 0xA0,  /**< Variable-arity block call (MUX, N-in/M-out custom node). See le_block_desc_t. */
    LE_OP_RESERVED_A1 = 0xA1,
    LE_OP_RESERVED_A2 = 0xA2,
    LE_OP_RESERVED_A3 = 0xA3,
    LE_OP_RESERVED_A4 = 0xA4,
    LE_OP_RESERVED_A5 = 0xA5,
    LE_OP_RESERVED_A6 = 0xA6,
    LE_OP_RESERVED_A7 = 0xA7,
    LE_OP_RESERVED_A8 = 0xA8,
    LE_OP_RESERVED_A9 = 0xA9,
    LE_OP_RESERVED_AA = 0xAA,
    LE_OP_RESERVED_AB = 0xAB,
    LE_OP_RESERVED_AC = 0xAC,
    LE_OP_RESERVED_AD = 0xAD,
    LE_OP_RESERVED_AE = 0xAE,
    LE_OP_RESERVED_AF = 0xAF,
    LE_OP_RESERVED_B0 = 0xB0,
    LE_OP_RESERVED_B1 = 0xB1,
    LE_OP_RESERVED_B2 = 0xB2,
    LE_OP_RESERVED_B3 = 0xB3,
    LE_OP_RESERVED_B4 = 0xB4,
    LE_OP_RESERVED_B5 = 0xB5,
    LE_OP_RESERVED_B6 = 0xB6,
    LE_OP_RESERVED_B7 = 0xB7,
    LE_OP_RESERVED_B8 = 0xB8,
    LE_OP_RESERVED_B9 = 0xB9,
    LE_OP_RESERVED_BA = 0xBA,
    LE_OP_RESERVED_BB = 0xBB,
    LE_OP_RESERVED_BC = 0xBC,
    LE_OP_RESERVED_BD = 0xBD,
    LE_OP_RESERVED_BE = 0xBE,
    LE_OP_RESERVED_BF = 0xBF,
    LE_OP_RESERVED_C0 = 0xC0,
    LE_OP_RESERVED_C1 = 0xC1,
    LE_OP_RESERVED_C2 = 0xC2,
    LE_OP_RESERVED_C3 = 0xC3,
    LE_OP_RESERVED_C4 = 0xC4,
    LE_OP_RESERVED_C5 = 0xC5,
    LE_OP_RESERVED_C6 = 0xC6,
    LE_OP_RESERVED_C7 = 0xC7,
    LE_OP_RESERVED_C8 = 0xC8,
    LE_OP_RESERVED_C9 = 0xC9,
    LE_OP_RESERVED_CA = 0xCA,
    LE_OP_RESERVED_CB = 0xCB,
    LE_OP_RESERVED_CC = 0xCC,
    LE_OP_RESERVED_CD = 0xCD,
    LE_OP_RESERVED_CE = 0xCE,
    LE_OP_RESERVED_CF = 0xCF,
    LE_OP_RESERVED_D0 = 0xD0,
    LE_OP_RESERVED_D1 = 0xD1,
    LE_OP_RESERVED_D2 = 0xD2,
    LE_OP_RESERVED_D3 = 0xD3,
    LE_OP_RESERVED_D4 = 0xD4,
    LE_OP_RESERVED_D5 = 0xD5,
    LE_OP_RESERVED_D6 = 0xD6,
    LE_OP_RESERVED_D7 = 0xD7,
    LE_OP_RESERVED_D8 = 0xD8,
    LE_OP_RESERVED_D9 = 0xD9,
    LE_OP_RESERVED_DA = 0xDA,
    LE_OP_RESERVED_DB = 0xDB,
    LE_OP_RESERVED_DC = 0xDC,
    LE_OP_RESERVED_DD = 0xDD,
    LE_OP_RESERVED_DE = 0xDE,
    LE_OP_RESERVED_DF = 0xDF,
    LE_OP_RESERVED_E0 = 0xE0,
    LE_OP_RESERVED_E1 = 0xE1,
    LE_OP_RESERVED_E2 = 0xE2,
    LE_OP_RESERVED_E3 = 0xE3,
    LE_OP_RESERVED_E4 = 0xE4,
    LE_OP_RESERVED_E5 = 0xE5,
    LE_OP_RESERVED_E6 = 0xE6,
    LE_OP_RESERVED_E7 = 0xE7,
    LE_OP_RESERVED_E8 = 0xE8,
    LE_OP_RESERVED_E9 = 0xE9,
    LE_OP_RESERVED_EA = 0xEA,
    LE_OP_RESERVED_EB = 0xEB,
    LE_OP_RESERVED_EC = 0xEC,
    LE_OP_RESERVED_ED = 0xED,
    LE_OP_RESERVED_EE = 0xEE,
    LE_OP_RESERVED_EF = 0xEF,
    LE_OP_RESERVED_F0 = 0xF0,
    LE_OP_RESERVED_F1 = 0xF1,
    LE_OP_RESERVED_F2 = 0xF2,
    LE_OP_RESERVED_F3 = 0xF3,
    LE_OP_RESERVED_F4 = 0xF4,
    LE_OP_RESERVED_F5 = 0xF5,
    LE_OP_RESERVED_F6 = 0xF6,
    LE_OP_RESERVED_F7 = 0xF7,
    LE_OP_RESERVED_F8 = 0xF8,
    LE_OP_RESERVED_F9 = 0xF9,
    LE_OP_RESERVED_FA = 0xFA,
    LE_OP_RESERVED_FB = 0xFB,
    LE_OP_RESERVED_FC = 0xFC,
    LE_OP_RESERVED_FD = 0xFD,
    LE_OP_RESERVED_FE = 0xFE,

    LE_OP_END               = 0xFF   /**< End of execution table marker. */
} le_opcode_t;

/* Modifier Flags for Instructions */
#define LE_MOD_NONE             0x00
#define LE_MOD_INVERT_A         (1U << 0)  /* Invert Input A before evaluating */
#define LE_MOD_INVERT_B         (1U << 1)  /* Invert Input B before evaluating */
#define LE_MOD_INVERT_OUT       (1U << 2)  /* Invert Output */

/* ========================================================================== */
/* Static zero-heap state arena for stateful "complex" elements.              */
/* A fixed byte arena (no malloc); blocks are bound to heap pointers at       */
/* runtime start and instructions dereference those pointers each scan.       */
/* ========================================================================== */
#ifndef LE_RAM_WORKSPACE_BYTES
#define LE_RAM_WORKSPACE_BYTES      2048    /* Total RAM reserved for the unified workspace: it is carved at
                                             * load time into (a) the packed register arena (bits, floats,
                                             * ints, complex, analog sized by the .lebin's declared register
                                             * counts) and (b) the state workspace (the preconfigured state
                                             * image copied verbatim). A program is rejected when its combined
                                             * register arena + state image exceed this budget, so RAM usage
                                             * tracks what a program declares, bounded by one board tunable. */
#endif

/** @brief Kind tag naming which state struct a runtime state block holds.
 * Values are explicit so the on-disk `.lebin` state-desc table is stable
 * regardless of which optional subsystems are compiled in. */
typedef enum {
    LE_BLK_NONE      = 0,    /* stateless (variable-arity block call) */
    LE_BLK_TIMER     = 1,   /* le_timer_state_t   */
    LE_BLK_COUNTER   = 2,   /* le_counter_state_t */
    LE_BLK_SCALER    = 3,   /* le_scale_state_t   */
    LE_BLK_LPF       = 4,   /* le_lpf_state_t     */
    LE_BLK_BIQUAD    = 5,   /* le_biquad_state_t  */
    LE_BLK_MOVING_AVG = 6,
    LE_BLK_RATE_LIMITER = 7,
    LE_BLK_DEADBAND  = 8,
    LE_BLK_WASHOUT   = 9,
    LE_BLK_PEAK      = 10,
    LE_BLK_RMS       = 11,
    LE_BLK_MEDIAN    = 12,
    LE_BLK_DERIVATIVE = 13,
    LE_BLK_ZERO_CROSSING = 14,
    LE_BLK_LUT_1D    = 15,
    LE_BLK_TOTALIZER = 16,
    LE_BLK_MIN_MAX_HOLD = 17,
    LE_BLK_I2C       = 18,  /* le_i2c_device_state_t   (serial bus) */
    LE_BLK_SPI       = 19,  /* le_spi_device_state_t   (serial bus) */
    LE_BLK_PID       = 20,  /* le_pid_state_t          (DSP / control — not protection) */
    LE_BLK_OVERCURRENT = 21, /* le_overcurrent_state_t */
    LE_BLK_PHASOR    = 22,  /* le_phasor_state_t  */
    LE_BLK_SYMCOMP   = 23,  /* le_symcomp_state_t */
    LE_BLK_21        = 24,  /* le_dist21_state_t  */
    LE_BLK_DIFF_87   = 25,  /* le_diff87_state_t (ANSI 87 differential) */
    LE_BLK_PHASE_COMP = 26, /* le_comp33_state_t (3-phase transformer phase compensation) */
    LE_BLK_PHASOR3   = 27,  /* le_phasor3_state_t (3-phase synced phasor extractor bank) */
    LE_BLK_FREQ_EST  = 28,  /* le_freq_est_state_t (ANSI 81 dynamic frequency estimator) */
    LE_BLK_LAST        /* sentinel (not a real block kind) */
} le_block_kind_t;

/** @brief One active state-block slot in the runtime block table (RAM). */
typedef struct {
    uint8_t  kind;         /**< @ref le_block_kind_t. */
    uint16_t state_size;   /**< Bytes at @p state (for bookkeeping/reset). */
    uint8_t* state;        /**< Pointer into the zero-heap state arena. */
} le_rt_block_t;

/**
 * @brief Binary directive describing a group of same-kind state blocks.
 * Appended after the (variable-arity) block table so the loader can reserve
 * heap slices and bind pointers at load time. Instances of one kind occupy
 * contiguous runtime-block rows.
 */
typedef struct {
    uint8_t  kind;    /**< le_block_kind_t (LE_BLK_NONE is ignored). */
    uint8_t  count;   /**< Number of instances of @p kind. */
    uint16_t size;    /**< Bytes of one instance's state struct. */
} le_state_desc_t;

/** @brief Bytes of one @ref le_state_desc_t record. */
#define LE_STATE_DESC_BYTES 4

/* ========================================================================== */
/* Variable-arity block function identifiers (modifier of LE_OP_BLOCK)        */
/* ========================================================================== */
#define LE_FUNC_NONE             0x00
#define LE_FUNC_MUX_SELECT       0x01   /* [sel, in0, in1] -> [out]: out = args[2] ? args[1] : args[0] */
/* Complex<->float conversions (always enumerated for a dense block dispatch
 * table; execution is guarded with LE_ENABLE_COMPLEX). */
#define LE_FUNC_RECT2POLAR       0x02   /* [real, imag] -> [mag, angle_rad] */
#define LE_FUNC_POLAR2RECT       0x03   /* [mag, angle_rad] -> [real, imag] */
#define LE_FUNC_PHASOR_SHIFT     0x04   /* [real, imag, delta_rad] -> [real', imag']: CCW rotation by delta */
#define LE_FUNC_COMPLEX2POLAR    0x05   /* [cplx] -> [mag, angle_rad] */
#define LE_FUNC_COMPLEX2RECT     0x06   /* [cplx] -> [real, imag] (complex register to rectangular floats) */
#define LE_FUNC_RECT2COMPLEX     0x07   /* [real, imag] -> [cplx] */
#define LE_FUNC_POLAR2COMPLEX    0x08   /* [mag, angle_rad] -> [cplx] */
#define LE_FUNC_COMPLEX_MUL      0x09   /* [c_a, c_b] -> [c_out] */

/* 0x0A: stateless float utility */
#define LE_FUNC_CLAMP_F          0x0A   /* [value, min, max] -> [out]: clamp value to [min, max] */

/* Protection & phasor blocks (always enumerated; execution is guarded with
 * LE_ENABLE_PROTECTION). */
#define LE_FUNC_PHASOR_1P        0x0B   /* [sample, sync_cplx] -> [cplx]: synced phasor extractor */
#define LE_FUNC_DIFF_87          0x0C   /* [cph0..cphN-1] -> [bool]: N-input dual-slope differential (ANSI 87) */
#define LE_FUNC_DIST_21          0x0D   /* [v_c, i_c, offset_on] -> [bool]: mho distance (21) with prefault V memory */
#define LE_FUNC_PHASE_COMP       0x0E   /* [c_a, c_b, c_c] -> [c_a', c_b', c_c']: 3p transformer phase-shift compensation (ANSI 87T) */
#define LE_FUNC_OVERCURRENT_51   0x0F   /* [i_c, enable] -> [bool]: IEC/IEEE inverse-time overcurrent (ANSI 51).
                                         * Takes a COMPLEX phasor (magnitude drives the inverse-time curve) plus a
                                         * boolean ENABLE that is AND'd with the pickup evaluation BEFORE the timing
                                         * accumulator (wire a directionality boolean for ground directional OC). */

#define LE_FUNC_PHASOR_3P        0x10   /* [a,b,c,sync,freq_hz] -> [pa,pb,pc]: 3-phase synced phasor extractor */
#define LE_FUNC_FREQ_EST         0x11   /* [sample] -> [freq_hz, valid]: ANSI 81 dynamic frequency estimator */

/* Reserved block func handles: keep the 0x12 - 0x7F range dense for a unified
 * O(1) block-dispatch table. */
#define LE_FUNC_RESERVED_12  0x12
#define LE_FUNC_RESERVED_13  0x13
#define LE_FUNC_RESERVED_14  0x14
#define LE_FUNC_RESERVED_15  0x15
#define LE_FUNC_RESERVED_16  0x16
#define LE_FUNC_RESERVED_17  0x17
#define LE_FUNC_RESERVED_18  0x18
#define LE_FUNC_RESERVED_19  0x19
#define LE_FUNC_RESERVED_1A  0x1A
#define LE_FUNC_RESERVED_1B  0x1B
#define LE_FUNC_RESERVED_1C  0x1C
#define LE_FUNC_RESERVED_1D  0x1D
#define LE_FUNC_RESERVED_1E  0x1E
#define LE_FUNC_RESERVED_1F  0x1F
#define LE_FUNC_RESERVED_20  0x20
#define LE_FUNC_RESERVED_21  0x21
#define LE_FUNC_RESERVED_22  0x22
#define LE_FUNC_RESERVED_23  0x23
#define LE_FUNC_RESERVED_24  0x24
#define LE_FUNC_RESERVED_25  0x25
#define LE_FUNC_RESERVED_26  0x26
#define LE_FUNC_RESERVED_27  0x27
#define LE_FUNC_RESERVED_28  0x28
#define LE_FUNC_RESERVED_29  0x29
#define LE_FUNC_RESERVED_2A  0x2A
#define LE_FUNC_RESERVED_2B  0x2B
#define LE_FUNC_RESERVED_2C  0x2C
#define LE_FUNC_RESERVED_2D  0x2D
#define LE_FUNC_RESERVED_2E  0x2E
#define LE_FUNC_RESERVED_2F  0x2F
#define LE_FUNC_RESERVED_30  0x30
#define LE_FUNC_RESERVED_31  0x31
#define LE_FUNC_RESERVED_32  0x32
#define LE_FUNC_RESERVED_33  0x33
#define LE_FUNC_RESERVED_34  0x34
#define LE_FUNC_RESERVED_35  0x35
#define LE_FUNC_RESERVED_36  0x36
#define LE_FUNC_RESERVED_37  0x37
#define LE_FUNC_RESERVED_38  0x38
#define LE_FUNC_RESERVED_39  0x39
#define LE_FUNC_RESERVED_3A  0x3A
#define LE_FUNC_RESERVED_3B  0x3B
#define LE_FUNC_RESERVED_3C  0x3C
#define LE_FUNC_RESERVED_3D  0x3D
#define LE_FUNC_RESERVED_3E  0x3E
#define LE_FUNC_RESERVED_3F  0x3F
#define LE_FUNC_RESERVED_40  0x40
#define LE_FUNC_RESERVED_41  0x41
#define LE_FUNC_RESERVED_42  0x42
#define LE_FUNC_RESERVED_43  0x43
#define LE_FUNC_RESERVED_44  0x44
#define LE_FUNC_RESERVED_45  0x45
#define LE_FUNC_RESERVED_46  0x46
#define LE_FUNC_RESERVED_47  0x47
#define LE_FUNC_RESERVED_48  0x48
#define LE_FUNC_RESERVED_49  0x49
#define LE_FUNC_RESERVED_4A  0x4A
#define LE_FUNC_RESERVED_4B  0x4B
#define LE_FUNC_RESERVED_4C  0x4C
#define LE_FUNC_RESERVED_4D  0x4D
#define LE_FUNC_RESERVED_4E  0x4E
#define LE_FUNC_RESERVED_4F  0x4F
#define LE_FUNC_RESERVED_50  0x50
#define LE_FUNC_RESERVED_51  0x51
#define LE_FUNC_RESERVED_52  0x52
#define LE_FUNC_RESERVED_53  0x53
#define LE_FUNC_RESERVED_54  0x54
#define LE_FUNC_RESERVED_55  0x55
#define LE_FUNC_RESERVED_56  0x56
#define LE_FUNC_RESERVED_57  0x57
#define LE_FUNC_RESERVED_58  0x58
#define LE_FUNC_RESERVED_59  0x59
#define LE_FUNC_RESERVED_5A  0x5A
#define LE_FUNC_RESERVED_5B  0x5B
#define LE_FUNC_RESERVED_5C  0x5C
#define LE_FUNC_RESERVED_5D  0x5D
#define LE_FUNC_RESERVED_5E  0x5E
#define LE_FUNC_RESERVED_5F  0x5F
#define LE_FUNC_RESERVED_60  0x60
#define LE_FUNC_RESERVED_61  0x61
#define LE_FUNC_RESERVED_62  0x62
#define LE_FUNC_RESERVED_63  0x63
#define LE_FUNC_RESERVED_64  0x64
#define LE_FUNC_RESERVED_65  0x65
#define LE_FUNC_RESERVED_66  0x66
#define LE_FUNC_RESERVED_67  0x67
#define LE_FUNC_RESERVED_68  0x68
#define LE_FUNC_RESERVED_69  0x69
#define LE_FUNC_RESERVED_6A  0x6A
#define LE_FUNC_RESERVED_6B  0x6B
#define LE_FUNC_RESERVED_6C  0x6C
#define LE_FUNC_RESERVED_6D  0x6D
#define LE_FUNC_RESERVED_6E  0x6E
#define LE_FUNC_RESERVED_6F  0x6F
#define LE_FUNC_RESERVED_70  0x70
#define LE_FUNC_RESERVED_71  0x71
#define LE_FUNC_RESERVED_72  0x72
#define LE_FUNC_RESERVED_73  0x73
#define LE_FUNC_RESERVED_74  0x74
#define LE_FUNC_RESERVED_75  0x75
#define LE_FUNC_RESERVED_76  0x76
#define LE_FUNC_RESERVED_77  0x77
#define LE_FUNC_RESERVED_78  0x78
#define LE_FUNC_RESERVED_79  0x79
#define LE_FUNC_RESERVED_7A  0x7A
#define LE_FUNC_RESERVED_7B  0x7B
#define LE_FUNC_RESERVED_7C  0x7C
#define LE_FUNC_RESERVED_7D  0x7D
#define LE_FUNC_RESERVED_7E  0x7E
#define LE_FUNC_RESERVED_7F  0x7F
#define LE_FUNC_CUSTOM_BASE      0x80   /* func_id >= this dispatches to the HAL ext_call. */

/* ========================================================================== */
/* Binary instruction and header structures (packed)                         */
/* ========================================================================== */

#pragma pack(push, 1)

/**
 * @brief Packed binary file header for compiled LogicElements bytecode (.lebin).
 */
typedef struct {
    uint32_t magic;             /**< Magic identifier: 0x4C454231 (ASCII "LEB1"). */
    uint16_t version;           /**< Binary format version number (currently 8). */
    uint16_t flags;             /**< Program execution flags (for example, LE_FLAG_AUTOSTART). */
    uint16_t instruction_count; /**< Total number of instructions in the bytecode payload. */
    uint16_t digital_in_count;  /**< Number of digital input channels (%I) required. */
    uint16_t digital_out_count; /**< Number of digital output channels (%Q) required. */
    uint16_t bool_reg_count;    /**< Number of internal boolean registers (%M) required. */
    uint16_t float_reg_count;   /**< Number of float registers (%R) required. */
    uint16_t complex_reg_count; /**< Number of complex registers (%C, real+imag pairs) required. */
    uint16_t int_reg_count;     /**< Number of int registers (%I) required. */
    uint16_t analog_in_count;   /**< Number of analog input channels (%AIN) required. */
    uint16_t block_count;       /**< Number of variable-arity block descriptors in the payload. */
    uint16_t state_desc_count;  /**< Number of state-group records (kinds present) in the payload. */
    uint32_t state_img_len;     /**< Bytes of the preconfigured state image (copied to RAM at load). */
    uint16_t alias_count;       /**< Number of user-declared register aliases (le_alias_t entries) appended after the state image. */
    uint16_t timing_count;      /**< 1 when an 8-byte le_timing_desc_t follows the alias table (0 otherwise). Pads the header to a multiple of 4 bytes so the zero-copy instruction array that follows is 4-byte aligned for in-flash execution. */
    uint32_t crc32;             /**< IEEE 802.3 CRC32 of the whole payload (instructions + block + state + state image + aliases + timing). */
} le_header_t;

/**
 * @brief Optional timing descriptor appended after the alias table (10 bytes).
 *
 * The compiler computes a worst-case abstract cost of the emitted program and
 * bakes it (target-independent) along with the safety margin it applied and the
 * circuit's declared fixed scan rate. Any target board scales the abstract cost
 * by its own calibrated `ns_per_abstract_cycle` to derive an estimated scan
 * duration and verify the requested rate is achievable.
 */
typedef struct {
    uint32_t abstract_cycles;   /**< Total worst-case abstract cost (compiler units). */
    uint16_t safety_margin_pct; /**< Compiler safety multiplier (e.g. 150 = 1.5x). */
    uint16_t design_scan_rate_hz; /**< Circuit-declared fixed scan rate (0 = unspecified). */
    uint16_t rsvd;              /**< Reserved (0). */
} le_timing_desc_t;

/** @brief Bytes of one @ref le_timing_desc_t record. */
#define LE_TIMING_DESC_BYTES 10

/**
 * @brief Timing / achievability report for a loaded program.
 *
 * Computed by @ref le_loader_timing from the binary's timing descriptor, the
 * board's `us_per_abstract_cycle` cost model, and the VM's configured scan
 * period. `feasible` is set when the worst-case scan fits inside the period.
 */
typedef struct {
    uint32_t scan_period_us;    /**< Fixed scan period configured on the VM (0 = none). */
    uint32_t worst_case_us;     /**< Estimated worst-case scan duration in microseconds. */
    uint32_t margin_us;         /**< scan_period_us - worst_case_us (0 when infeasible/unconfigured). */
    uint32_t abstract_cycles;   /**< Compiler cost units carried in the binary. */
    uint16_t margin_pct;        /**< 100 * margin_us / scan_period_us. */
    uint16_t safety_margin_pct; /**< Compiler safety multiplier carried in the binary. */
    uint8_t  feasible;          /**< 1 when worst_case_us <= scan_period_us. */
} le_timing_t;

/**
 * @brief Fixed 8-byte instruction format for the virtual machine.
 */
typedef struct {
    uint8_t  opcode;    /**< Operation code from @ref le_opcode_t. */
    uint8_t  modifier;  /**< Bitmask of input/output inversion modifiers or device index. */
    uint16_t in_a;      /**< 16-bit process image address for operand A. */
    uint16_t in_b;      /**< 16-bit process image address for operand B. */
    uint16_t out;       /**< 16-bit process image address for the destination result. */
} le_instruction_t;

/**
 * @brief Variable-arity block descriptor (referenced by LE_OP_BLOCK's in_a).
 *
 * A block carries more operands than the two data fields of
 * @ref le_instruction_t. Descriptors live in a zero-copy block table appended
 * after the instruction payload. Each descriptor is followed in memory by
 * (in_count + out_count) uint16 operand addresses:
 *     args[0..in_count)      inputs
 *     args[in_count..)       outputs
 * The data type of each operand is implied by its process-image address region
 * (bool / float / AIN / constant), so no per-arg type byte is stored.
 */
typedef struct {
    uint8_t  in_count;   /**< Number of live input operands (0..N). */
    uint8_t  out_count;  /**< Number of output operands (1..M). */
    uint8_t  flags;      /**< Reserved for per-function hints (edge, reset, ...). */
    uint8_t  reserved;   /**< Reserved (0). */
} le_block_desc_t;

/** @brief Bytes of a descriptor header (excludes the variable arg array). */
#define LE_BLOCK_DESC_HEADER_BYTES 4

/**
 * @brief A single user-declared register alias.
 *
 * Maps a short (<= LE_ALIAS_NAME_MAX) symbolic name to a process-image
 * address. Compiled circuits may declare an `aliases` object; each entry pairs
 * a name with a register target (`%B`, `%F`, `%I`, `%C`, `%IN`, `%OUT`,
 * `%AIN`, ...). The table is appended after the state image. The runtime/host
 * queries it (le_alias_lookup) to override, pulse, and target registers by
 * name instead of a raw address.
 */
typedef struct {
    char     name[LE_ALIAS_NAME_MAX]; /* Alias, left-aligned, NUL-terminated (max 7 chars). */
    uint8_t  kind;                    /* Reserved; 0 = generic/writable register. */
    uint8_t  pad;                     /* Reserved (0). */
    uint16_t addr;                    /* Process-image address the alias resolves to. */
} le_alias_t;                         /* 11 bytes, packed */

/** @brief Bytes of one alias table entry. */
#define LE_ALIAS_BYTES              (LE_ALIAS_NAME_MAX + 1 + 1 + 2)

#pragma pack(pop)

/**
 * @brief A single runtime pulse slot used to implement alias/register pulse
 * commands. A pulse sets a writable register to its active (non-zero) state,
 * then clears it to zero once `now_ms` reaches @p clear_after_ms.
 */
typedef struct {
    uint16_t addr;              /**< Process-image address being pulsed. */
    bool     active;            /**< Set while the pulse slot is in use. */
    uint32_t duration_ms;       /**< Requested pulse duration in milliseconds. */
    uint32_t clear_after_ms;    /**< Absolute VM timestamp when the pulse clears (0 = not yet anchored to a scan). */
} le_pulse_t;
/* These are laid out 1-byte-aligned (`#pragma pack`) so their in-memory     */
/* byte layout is identical between the host compiler (which bakes a          */
/* preconfigured state image into the .lebin) and the target MCU compiler     */
/* (which memcpy's that image directly to RAM). No padding is inserted, so    */
/* sizeof() is the exact sum of member sizes on every toolchain/endianness    */
/* (all supported targets are little-endian).                                 */
/* ========================================================================== */
#pragma pack(push, 1)

/**
 * @brief Runtime state tracking for timer blocks (TON, TOF, TP).
 */
typedef struct {
    uint32_t start_time_ms; /**< Timestamp in milliseconds when the timing interval began. */
    uint32_t preset_ms;     /**< Preset duration in milliseconds. */
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
 * @brief IEC/IEEE inverse-time overcurrent protection element state (ANSI 51).
 */

typedef enum {
    LE_CURVE_IEC_NORMAL   = 0,  /**< IEC Normal Inverse: A=0.14, B=0.0, p=0.02 */
    LE_CURVE_IEC_VERY     = 1,  /**< IEC Very Inverse: A=13.5, B=0.0, p=1.0 */
    LE_CURVE_IEC_EXTREME  = 2,  /**< IEC Extremely Inverse: A=80.0, B=0.0, p=2.0 */
    LE_CURVE_IEEE_MODERATELY = 3,  /**< IEEE Moderately Inverse: A=0.0515, B=0.1140, p=0.02 */
    LE_CURVE_IEEE_VERY    = 4,  /**< IEEE Very Inverse: A=19.61, B=0.491, p=2.0 */
    LE_CURVE_IEEE_EXTREME = 5,  /**< IEEE Extremely Inverse: A=28.2, B=0.1217, p=2.0 */
    LE_CURVE_IEEE_SHORT   = 6,  /**< IEEE Short Time Inverse: A=0.00342, B=0.00262, p=0.02 */
    LE_CURVE_IEEE_LONG    = 7,  /**< IEEE Long Time Inverse: A=26.13, B=0.349, p=2.0 */
    LE_CURVE_CUSTOM       = 8,  /**< User-supplied curve (a_coeff/b_coeff/p_coeff baked by the compiler) */
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

#ifndef LE_MAX_RAW_SAMPLES
#define LE_MAX_RAW_SAMPLES 64   /**< Raw sample buffer for high-rate acquisition (DFT window upper bound) */
#endif

/**
 * @brief Single-phase fundamental phasor extraction filter state.
 *
 * Implements a dynamic DFT that adapts to the measured frequency:
 * - Maintains a high-rate sample buffer (raw_samples) filled at scan rate
 * - The freq_hz input determines which subset of samples to use for the DFT
 * - Self-sync mode allows output angle to be 0-degree referenced
 * - Sample rate derives dynamically from the enforced scan period (le_rt_scan_dt)
 *   unless explicitly overridden in sample_rate_hz
 */
typedef struct {
    uint16_t     samples_per_cycle;                  /**< Default samples per cycle (config) */
    bool         self_sync;                          /**< true=0-degree reference, false=sync-referenced */
    float        sample_rate_hz;                     /**< Board scan rate (samples/sec, 0=derive from scan dt) */
    le_complex_t phasor;                             /**< Filtered fundamental complex phasor */
    float        magnitude;                          /**< RMS magnitude of extracted phasor */
    float        angle_rad;                          /**< Phase angle in radians (-pi to +pi) */
    uint16_t     raw_write_idx;                      /**< Raw sample buffer write pointer */
    float        raw_samples[LE_MAX_RAW_SAMPLES];    /**< High-rate raw sample buffer */
} le_phasor_state_t;

/**
 * @brief Three-phase fundamental phasor extraction filter state (PHASOR_3P).
 *
 * Banks three independent single-phase phasor extractors (a, b, c) against a
 * single bus reference phasor (sync) and a single system frequency input.
 * Each phase maintains its own high-rate sample buffer and DFT accumulator,
 * sharing the samples_per_cycle / sample_rate_hz / self_sync properties.
 */
typedef struct {
    /* Shared configuration (baked by the compiler). */
    uint16_t     samples_per_cycle;    /**< Default samples per cycle (config) */
    float        sample_rate_hz;       /**< Board scan rate (samples/second) */
    bool         self_sync;            /**< true=0-degree reference, false=sync-referenced */

    /* Phase A extractor state. */
    float        raw_samples_a[LE_MAX_RAW_SAMPLES];  /**< Phase A high-rate raw samples */
    uint16_t     raw_write_idx_a;                    /**< Phase A raw buffer write pointer */
    le_complex_t phasor_a;                           /**< Phase A fundamental complex phasor */
    float        magnitude_a;                        /**< Phase A RMS magnitude of extracted phasor */
    float        angle_rad_a;                        /**< Phase A phase angle in radians (-pi to +pi) */

    /* Phase B extractor state. */
    float        raw_samples_b[LE_MAX_RAW_SAMPLES];  /**< Phase B high-rate raw samples */
    uint16_t     raw_write_idx_b;                    /**< Phase B raw buffer write pointer */
    le_complex_t phasor_b;                           /**< Phase B fundamental complex phasor */
    float        magnitude_b;                        /**< Phase B RMS magnitude of extracted phasor */
    float        angle_rad_b;                        /**< Phase B phase angle in radians (-pi to +pi) */

    /* Phase C extractor state. */
    float        raw_samples_c[LE_MAX_RAW_SAMPLES];  /**< Phase C high-rate raw samples */
    uint16_t     raw_write_idx_c;                    /**< Phase C raw buffer write pointer */
    le_complex_t phasor_c;                           /**< Phase C fundamental complex phasor */
    float        magnitude_c;                        /**< Phase C RMS magnitude of extracted phasor */
    float        angle_rad_c;                        /**< Phase C phase angle in radians (-pi to +pi) */
} le_phasor3_state_t;

/**
 * @brief Dynamic frequency tracking and estimation state (ANSI 81).
 *
 * Tracks fundamental power-system frequency from instantaneous samples with
 * hysteresis noise rejection, fractional sub-sample zero-crossing interpolation,
 * validity bounds gating, and default-to-nominal fallback only on loss of
 * potential (timeout) / dead signal. An out-of-bounds but *healthy* frequency is
 * still reported (so 81U/81O can trip on a genuine under/over-frequency event),
 * with `valid` cleared to flag the abnormality. Sampling rate is derived
 * dynamically from the enforced scan cadence (le_rt_scan_dt()); no
 * sample-rate property is stored.
 */
typedef struct {
    /* Baked configuration properties */
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
    /* Prefault voltage memory: the last known-good V phasor, substituted for the
     * configured duration when the measured voltage is depressed (faulted). */
    le_complex_t prefault_v;     /**< Remembered pre-fault (known-good) voltage phasor. */
    uint32_t prefault_arm_ms;    /**< Timestamp when prefault substitution was armed. */
    bool   prefault_armed;       /**< True between arming and duration expiry. */
    float  r_meas;               /**< Measured apparent resistance (R). */
    float  x_meas;               /**< Measured apparent reactance (X). */
    bool   tripped;              /**< Distance zone trip flag. */
} le_dist21_state_t;

/**
 * @brief Dual-slope differential protection characteristic (ANSI 87).
 *
 * Implements the SEL-style proportional (dual-slope) differential restraint.
 * The "operate" current is the phasor vector sum of the N differential inputs;
 * the "restraint" current is the SUM of phasor magnitudes (large-bus
 * convention — no averaging).
 * The relay trips when the operate current exceeds a dual-slope ramp whose
 * slope changes at the restraint knee:
 *     threshold = o87p + slp1 * I_rt                 for I_rt <= irs1
 *     threshold = o87p + slp1*irs1 + slp2*(I_rt-irs1) for I_rt >  irs1
 * Parameters mirror SEL 87 settings (O87P, SLP1, IPS1/IRS1, SLP2).
 */
typedef struct {
    float  o87p;   /**< Differential pickup (pu), the y-intercept of the ramp. */
    float  slp1;   /**< First slope (pu operate per pu restraint), I_rt <= irs1. */
    float  irs1;   /**< Restraint-current knee where the slope changes slope (pu). */
    float  slp2;   /**< Second slope (pu operate per pu restraint), I_rt > irs1. */
    /* Runtime measure / trip state. */
    float  operate;    /**< Measured operate current (|vector sum|), pu. */
    float  restraint;  /**< Measured restraint current (sum of phasor magnitudes), pu. */
    bool   tripped;    /**< Differential (87) trip flag. */
} le_diff87_state_t;

/**
 * @brief Three-phase transformer phase-shift compensation (ANSI 87T).
 *
 * For transformer differential protection, the wye/delta winding phase
 * displacement must be removed so the operate (differential) current is zero
 * under through-load. This transform applies the SEL delta/wye compensation
 * matrix M(k) (k = 1..12) to the three winding phasors:
 *     I'_x = s(k) * sum_j M(k)[x][j] * I_j
 * where the scalar multiplier s(k) = 1/sqrt(3) for odd k, and 1/3 for even k.
 * `comp` stores the SEL compensation setting index k (1..12). Other numeric
 * settings used in some relays are phase shifts; this implementation uses the
 * IEEE/IEC delta-wye transformer compensation table (TCOMP style).
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

    /* Runtime state */
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

    /* Runtime state */
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
    float    sample_rate_hz;           /**< PLC scan sampling rate (Hz). */
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
    double   accumulator;              /**< High-precision running sum. */
    float    time_base_sec;            /**< Time base divisor (1=sec, 60=min, 3600=hr). */
    float    scale_factor;             /**< Output scaling multiplier. */
    float    sample_time_sec;          /**< Scan step delta time (seconds). */
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

/**
 * @brief Status and return codes for runtime API operations.
 */
typedef enum {
    LE_OK                   = 0,   /**< Operation completed successfully. */
    LE_ERR_NULL_PTR         = -1,  /**< A required pointer parameter was NULL. */
    LE_ERR_INVALID_MAGIC    = -2,  /**< Magic header does not match expected identifier. */
    LE_ERR_INVALID_VERSION  = -3,  /**< Bytecode version is unsupported. */
    LE_ERR_CRC_MISMATCH     = -4,  /**< CRC32 checksum validation failed. */
    LE_ERR_CAPACITY         = -5,  /**< Required resources exceed target capacity limits. */
    LE_ERR_OUT_OF_BOUNDS    = -6,  /**< Index or address exceeds allocated bounds. */
    LE_ERR_UNKNOWN_OPCODE   = -7,  /**< Instruction contains an unrecognized opcode. */
    LE_ERR_NOT_FOUND        = -8,  /**< Named alias/register not found. */
    LE_ERR_SCAN_JITTER      = -9,  /**< Fixed-rate scan boundary missed (non-uniform cadence). */
    LE_ERR_TIMING_BUDGET    = -10, /**< Program worst-case scan exceeds the configured scan period. */
    LE_ERR_SCAN_OVERFLOW    = -11  /**< A measured scan overran the fixed period. */
} le_status_t;

#ifdef __cplusplus
}
#endif

#endif /* LE_TYPES_H */
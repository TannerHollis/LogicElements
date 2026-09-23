/**
 * @file le_types.h
 * @brief Core types, opcodes, memory layout, and binary format definitions for LogicElements C Runtime.
 */

#ifndef LE_TYPES_H
#define LE_TYPES_H

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

#ifndef LE_MAX_ANALOG_IN
#define LE_MAX_ANALOG_IN        16      /* Up to 16 Analog Inputs (ADC channels) */
#endif

#ifndef LE_MAX_SCALERS
#define LE_MAX_SCALERS          16      /* Up to 16 linear scalers */
#endif

#ifndef LE_MAX_TIMERS
#define LE_MAX_TIMERS           32      /* Up to 32 timers (TON/TOF/TP) */
#endif

#ifndef LE_MAX_COUNTERS
#define LE_MAX_COUNTERS         16      /* Up to 16 counters (CTU/CTD) */
#endif

#ifndef LE_MAX_PID
#define LE_MAX_PID              8       /* Up to 8 PID loop states */
#endif

#ifndef LE_MAX_OVERCURRENT
#define LE_MAX_OVERCURRENT      8       /* Up to 8 Overcurrent elements */
#endif

/* ========================================================================== */
/* Optional Protection & Control Relay Configuration                          */
/* ========================================================================== */
#ifndef LE_ENABLE_PROTECTION
#define LE_ENABLE_PROTECTION    1       /* Set to 1 to enable P&C relay opcodes */
#endif

#if LE_ENABLE_PROTECTION
#include "le_complex.h"

#ifndef LE_MAX_PHASORS
#define LE_MAX_PHASORS          16      /* Up to 16 1P Phasor filter windows */
#endif

#ifndef LE_MAX_DIFF87
#define LE_MAX_DIFF87           8       /* Up to 8 Differential elements */
#endif

#ifndef LE_MAX_DIST21
#define LE_MAX_DIST21           8       /* Up to 8 Distance relay zones */
#endif

#ifndef LE_MAX_SAMPLES_PER_CYCLE
#define LE_MAX_SAMPLES_PER_CYCLE 32     /* Up to 32 samples/cycle */
#endif
#endif

/* ========================================================================== */
/* Optional Serial Bus (I2C / SPI) Device Configuration                       */
/* ========================================================================== */
#ifndef LE_ENABLE_SERIAL_BUS
#define LE_ENABLE_SERIAL_BUS    1       /* Set to 1 to enable I2C and SPI bus opcodes */
#endif

#if LE_ENABLE_SERIAL_BUS
#ifndef LE_MAX_I2C_DEVICES
#define LE_MAX_I2C_DEVICES      4       /* Up to 4 external I2C devices */
#endif

#ifndef LE_MAX_SPI_DEVICES
#define LE_MAX_SPI_DEVICES      4       /* Up to 4 external SPI devices */
#endif
#endif

/* ========================================================================== */
/* Optional Digital Signal Processing (DSP) & Filters Configuration           */
/* ========================================================================== */
#ifndef LE_ENABLE_DSP
#define LE_ENABLE_DSP           1       /* Set to 1 to enable DSP and filter opcodes */
#endif

#if LE_ENABLE_DSP
#ifndef LE_MAX_DSP_FILTERS
#define LE_MAX_DSP_FILTERS      16      /* Up to 16 filter instances per DSP element type */
#endif

#ifndef LE_MOVING_AVG_MAX_WINDOW
#define LE_MOVING_AVG_MAX_WINDOW 32     /* Up to 32 samples window for moving average */
#endif

#ifndef LE_RMS_MAX_WINDOW
#define LE_RMS_MAX_WINDOW       32      /* Up to 32 samples window for True RMS */
#endif

#ifndef LE_MAX_MEDIAN_WINDOW
#define LE_MAX_MEDIAN_WINDOW    9       /* Up to 9 samples window for median filter */
#endif

#ifndef LE_MAX_LUT_POINTS
#define LE_MAX_LUT_POINTS       16      /* Up to 16 breakpoints for 1D lookup table */
#endif
#endif


/* ========================================================================== */
/* Binary Format Constants                                                    */
/* ========================================================================== */

#define LE_BIN_MAGIC            0x4C454231  /* ASCII "LEB1" */
#define LE_BIN_VERSION          3

/* Program Header Flags */
#define LE_FLAG_AUTOSTART       (1U << 0)
#define LE_FLAG_WATCHDOG_EN     (1U << 1)

/* ========================================================================== */
/* Memory Address Encoding (16-bit)                                           */
/* Bits 15..12 encode the region type:                                       */
/*   0x0000 - 0x0FFF : Digital Inputs                                        */
/*   0x1000 - 0x1FFF : Digital Outputs                                       */
/*   0x2000 - 0x3FFF : Boolean Registers                                     */
/*   0x4000 - 0x7FFF : Float Registers                                       */
/*   0x8000 - 0x8FFF : Timer Status / Output Bits                            */
/*   0x9000 - 0x9FFF : Counter Status / Output Bits                          */
/*   0xA000 - 0xAFFF : Integer Registers                                     */
/*   0xB000 - 0xBFFF : Analog Inputs                                         */
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

/* Predefined Constants */
#define LE_CONST_FALSE          0xC000
#define LE_CONST_TRUE           0xC001
#define LE_CONST_ZERO_F         0xC002
#define LE_CONST_ONE_F          0xC003
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

    /* 0x20 - 0x2F: Edge Detection & Latches */
    LE_OP_RTRIG             = 0x20,  /* Rising edge pulse */
    LE_OP_FTRIG             = 0x21,  /* Falling edge pulse */
    LE_OP_SR                = 0x22,  /* Set-Reset Latch (Set dominant) */
    LE_OP_RS                = 0x23,  /* Reset-Set Latch (Reset dominant) */

    /* 0x30 - 0x3F: Timers and Counters */
    LE_OP_TON               = 0x30,  /* On-Delay Timer */
    LE_OP_TOF               = 0x31,  /* Off-Delay Timer */
    LE_OP_TP                = 0x32,  /* Pulse Timer */
    LE_OP_CTU               = 0x33,  /* Count Up */
    LE_OP_CTD               = 0x34,  /* Count Down */
    LE_OP_CTUD              = 0x35,  /* Count Up/Down */

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

    /* 0x60 - 0x6F: Comparison Operations (Float -> Bool) */
    LE_OP_CMP_GT            = 0x60,  /* out_bool = in_a_f > in_b_f */
    LE_OP_CMP_LT            = 0x61,  /* out_bool = in_a_f < in_b_f */
    LE_OP_CMP_GE            = 0x62,  /* out_bool = in_a_f >= in_b_f */
    LE_OP_CMP_LE            = 0x63,  /* out_bool = in_a_f <= in_b_f */
    LE_OP_CMP_EQ            = 0x64,  /* out_bool = in_a_f == in_b_f */
    LE_OP_CMP_NE            = 0x65,  /* out_bool = in_a_f != in_b_f */

    /* 0x70 - 0x8F: Control, Protection & Conversions */
    LE_OP_PID               = 0x70,  /* Closed-loop PID Controller */
    LE_OP_OVERCURRENT       = 0x71,  /* IEC/IEEE Inverse-Time Overcurrent */
    LE_OP_RECT2POLAR        = 0x72,  /* (x, y) -> (mag, angle) */
    LE_OP_POLAR2RECT        = 0x73,  /* (mag, angle) -> (x, y) */
    LE_OP_PHASOR_SHIFT      = 0x74,  /* Shift angle by delta */
#if LE_ENABLE_PROTECTION
    LE_OP_PHASOR_1P         = 0x75,  /* 1-Phase Winding Phasor Extraction (DFT / Cosine Filter) */
    LE_OP_SYM_COMP          = 0x76,  /* 3-Phase Symmetrical Components (Seq 0, 1, 2) */
    LE_OP_DIFF_87           = 0x77,  /* SEL-Style Dual-Slope Percentage Differential */
    LE_OP_DIST_21           = 0x78,  /* Mho Distance Relay Zone */
#endif
#if LE_ENABLE_SERIAL_BUS
    LE_OP_I2C               = 0x79,  /* I2C Master Transaction Block */
    LE_OP_SPI               = 0x7A,  /* SPI Master Transaction Block */
#endif

    /* 0x80 - 0x8F: Board custom nodes and external hardware functions */
    LE_OP_EXT_CALL          = 0x80,  /**< Calls a board-specific custom node or BSP routine (modifier = function_id). */
    LE_OP_BLOCK             = 0xA0,  /**< Variable-arity block call (MUX, N-in/M-out custom node). See le_block_desc_t. */

    /* 0x90 - 0x9F: Digital Signal Processing (DSP) & Filters */
#if LE_ENABLE_DSP
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
#endif

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
#ifndef LE_STATE_HEAP_BYTES
#define LE_STATE_HEAP_BYTES         4096    /* total arena available to state blocks */
#endif

#ifndef LE_MAX_RT_BLOCKS
#define LE_MAX_RT_BLOCKS            128     /* max concurrently-active state blocks */
#endif

/** @brief Kind tag naming which state struct a runtime block row holds. */
typedef enum {
    LE_BLK_NONE      = 0,    /* stateless (variable-arity block call) */
    LE_BLK_TIMER,            /* le_timer_state_t   */
    LE_BLK_COUNTER,          /* le_counter_state_t */
    LE_BLK_PID,              /* le_pid_state_t     */
    LE_BLK_OVERCURRENT,      /* le_overcurrent_state_t */
    LE_BLK_PHASOR,           /* le_phasor_state_t  */
    LE_BLK_SCALER,           /* le_scale_state_t   */
    LE_BLK_LPF,              /* le_lpf_state_t     */
    LE_BLK_BIQUAD,           /* le_biquad_state_t  */
    LE_BLK_MOVING_AVG,
    LE_BLK_RATE_LIMITER,
    LE_BLK_DEADBAND,
    LE_BLK_WASHOUT,
    LE_BLK_PEAK,
    LE_BLK_RMS,
    LE_BLK_MEDIAN,
    LE_BLK_DERIVATIVE,
    LE_BLK_ZERO_CROSSING,
    LE_BLK_LUT_1D,
    LE_BLK_TOTALIZER,
    LE_BLK_MIN_MAX_HOLD,
    LE_BLK_SYMCOMP,
    LE_BLK_87,
    LE_BLK_21,
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

/**
 * @brief Binary per-element configuration directive: applied to a bound state
 * block at load time so compiled circuits honor their tuning (e.g. LPF alpha,
 * PID gains, scaler mapping). `value` is float payload; integer/hybrid fields
 * are cast by the kind/slot handler.
 */
typedef struct {
    uint8_t  kind;    /**< le_block_kind_t. */
    uint8_t  idx;     /**< Instance index within @p kind. */
    uint8_t  slot;    /**< Field slot id (see le_rt_configure). */
    uint8_t  reserved;/**< 0. */
    float    value;   /**< Parameter value. */
} le_config_t;

/** @brief Bytes of one @ref le_config_t record. */
#define LE_CONFIG_BYTES 8

/* ========================================================================== */
/* Variable-arity block function identifiers (modifier of LE_OP_BLOCK)        */
/* ========================================================================== */
#define LE_FUNC_NONE             0x00
#define LE_FUNC_MUX_SELECT       0x01   /* [sel, in0, in1] -> [out]: out = args[2] ? args[1] : args[0] */
#define LE_FUNC_RECT2POLAR       0x02   /* [real, imag] -> [mag, angle_rad] */
#define LE_FUNC_POLAR2RECT       0x03   /* [mag, angle_rad] -> [real, imag] */
#define LE_FUNC_PHASOR_SHIFT     0x04   /* [real, imag, delta_rad] -> [real', imag']: CCW rotation by delta */
#define LE_FUNC_PHASOR_1P        0x05   /* [sample, sync_mag, sync_angle] -> [mag, angle_rad]: synced phasor extractor */
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
    uint16_t version;           /**< Binary format version number (currently 2). */
    uint16_t flags;             /**< Program execution flags (for example, LE_FLAG_AUTOSTART). */
    uint16_t instruction_count; /**< Total number of instructions in the bytecode payload. */
    uint16_t digital_in_count;  /**< Number of digital input channels (%I) required. */
    uint16_t digital_out_count; /**< Number of digital output channels (%Q) required. */
    uint16_t bool_reg_count;    /**< Number of internal boolean registers (%M) required. */
    uint16_t float_reg_count;   /**< Number of float registers (%R) required. */
    uint16_t timer_count;       /**< Number of timer instances required. */
    uint16_t counter_count;     /**< Number of counter instances required. */
    uint16_t block_count;       /**< Number of variable-arity block descriptors in the payload. */
    uint16_t state_desc_count;  /**< Number of state-directive records in the payload. */
    uint16_t config_count;      /**< Number of per-element config directives in the payload. */
    uint32_t crc32;             /**< IEEE 802.3 CRC32 of the whole payload (instructions + block + state + config). */
} le_header_t;

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

#pragma pack(pop)

/* ========================================================================== */
/* State structures for stateful runtime elements                             */
/* ========================================================================== */

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
 * @brief Inverse-time overcurrent protection element state (ANSI 51).
 */
typedef struct {
    float    pickup;        /**< Pickup current threshold in per-unit or amperes. */
    float    time_dial;     /**< Time dial multiplier (TMS). */
    uint8_t  curve_type;    /**< IEC/IEEE curve characteristics selection. */
    float    accumulator;   /**< Trip integration accumulator state. */
    bool     tripped;       /**< Tripped condition flag. */
} le_overcurrent_state_t;

#if LE_ENABLE_PROTECTION
/**
 * @brief Single-phase fundamental phasor extraction filter state.
 */
typedef struct {
    float        samples[LE_MAX_SAMPLES_PER_CYCLE]; /**< Circular sample history window. */
    uint16_t     samples_per_cycle;                 /**< Number of samples per power frequency cycle. */
    uint16_t     write_idx;                         /**< Current circular buffer write pointer. */
    le_complex_t phasor;                            /**< Filtered fundamental complex phasor. */
    float        magnitude;                         /**< RMS magnitude of the extracted phasor. */
    float        angle_rad;                         /**< Phase angle in radians (-pi to +pi). */
} le_phasor_state_t;

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
 * @brief Dual-slope percentage restrained differential protection state (ANSI 87).
 */
typedef struct {
    float        o87p;      /**< Minimum operating current pickup threshold. */
    float        slp1;      /**< Percentage restraint slope 1 (typically 0.15 to 0.40). */
    float        irs1;      /**< Restraint current breakpoint knee-point. */
    float        slp2;      /**< Percentage restraint slope 2 (typically 0.50 to 0.80). */
    float        i_op;      /**< Calculated operating current magnitude. */
    float        i_rt;      /**< Calculated restraint current magnitude. */
    bool         tripped;   /**< Differential trip flag. */
} le_diff87_state_t;

/**
 * @brief Mho circle distance relay protection zone state (ANSI 21).
 */
typedef struct {
    float        reach_ohms;     /**< Zone reach impedance magnitude in secondary ohms. */
    float        line_angle_rad; /**< Characteristic transmission line impedance angle in radians. */
    float        r_meas;         /**< Measured apparent resistance (R). */
    float        x_meas;         /**< Measured apparent reactance (X). */
    bool         tripped;        /**< Distance zone trip flag. */
} le_dist21_state_t;
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
    LE_ERR_UNKNOWN_OPCODE   = -7   /**< Instruction contains an unrecognized opcode. */
} le_status_t;

#ifdef __cplusplus
}
#endif

#endif /* LE_TYPES_H */

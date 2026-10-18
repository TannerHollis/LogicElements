/**
 * @file le_addr.h
 * @brief Memory Address Encoding and Virtual Operand Space definitions.
 *
 * Physical I/O Space (4,096 channels each):
 *   0x0000 - 0x0FFF : Physical Digital Inputs   (%IN[i])
 *   0x1000 - 0x1FFF : Physical Digital Outputs  (%OUT[i])
 *   0x2000 - 0x2FFF : Physical Analog Inputs    (%AIN[i], ADC)
 *   0x3000 - 0x3FFF : Physical Analog Outputs   (%AOUT[i], DAC)
 * Variable / Memory Space:
 *   0x4000 - 0x5FFF : User Boolean Bits         (%B[i], 8,192 capacity)
 *   0x6000 - 0x7FFF : Signed 32-bit Integers    (%I[i], 8,192 capacity)
 *   0x8000 - 0x9FFF : 32-bit IEEE 754 Floats    (%F[i], 8,192 capacity)
 *   0xA000 - 0xAFFF : Complex Phasors           (%C[i], 4,096 capacity)
 * Special & Future Space:
 *   0xB000 - 0xBFFF : Reserved for Future Primitives (e.g. %L int64)
 *   0xC000 - 0xCFFF : Immediate Literals / Predefined Constants
 *   0xD000 - 0xDFFF : Reserved for Future Structs (e.g. %D double64)
 *   0xE000 - 0xEFFF : Reserved for System / Diagnostic Variables (%SYS)
 *   0xF000 - 0xFFFE : Reserved
 *   0xFFFF          : Sentinel / Unused         (LE_ADDR_UNUSED)
 */

#ifndef LE_ADDR_H
#define LE_ADDR_H

#include "le_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LE_ADDR_REGION_MASK     0xF000
#define LE_ADDR_INDEX_MASK      0x0FFF

#define LE_REGION_DIN           0x0000
#define LE_REGION_DOUT          0x1000
#define LE_REGION_AIN           0x2000
#define LE_REGION_AOUT          0x3000
#define LE_REGION_BOOL_REG      0x4000
#define LE_REGION_BOOL_REG_EXT  0x5000
#define LE_REGION_INT_REG       0x6000
#define LE_REGION_INT_REG_EXT   0x7000
#define LE_REGION_FLOAT         0x8000
#define LE_REGION_FLOAT_EXT     0x9000
#if LE_ENABLE_COMPLEX
#define LE_REGION_CMPLX         0xA000
#endif
#define LE_REGION_CONST         0xC000

/* Predefined Constants */
#define LE_CONST_FALSE          0xC000
#define LE_CONST_TRUE           0xC001
#define LE_CONST_ZERO_F         0xC002
#define LE_CONST_ONE_F          0xC003
#define LE_CONST_60_F           0xC004  /* 60 Hz default for PHASOR_1P freq_hz input */
#if LE_ENABLE_COMPLEX
#define LE_CONST_ZERO_C         0xC005  /* Complex zero (0+0j) */
#endif
#define LE_ADDR_UNUSED          0xFFFF

/* Helper Macros for Address Construction */
#define LE_ADDR_MAKE_DIN(idx)        ((uint16_t)(LE_REGION_DIN | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_DOUT(idx)       ((uint16_t)(LE_REGION_DOUT | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_AIN(idx)        ((uint16_t)(LE_REGION_AIN | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_AOUT(idx)       ((uint16_t)(LE_REGION_AOUT | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_BOOL_REG(idx)   ((uint16_t)(LE_REGION_BOOL_REG | ((idx) & 0x1FFF)))
#define LE_ADDR_MAKE_INT_REG(idx)    ((uint16_t)(LE_REGION_INT_REG | ((idx) & 0x1FFF)))
#define LE_ADDR_MAKE_FLOAT(idx)      ((uint16_t)(LE_REGION_FLOAT | ((idx) & 0x1FFF)))
#if LE_ENABLE_COMPLEX
#define LE_ADDR_MAKE_CMPLX(idx)      ((uint16_t)(LE_REGION_CMPLX | ((idx) & LE_ADDR_INDEX_MASK)))
#define LE_ADDR_MAKE_CMPLX_PAIR(idx) ((uint16_t)(LE_REGION_CMPLX | (((idx) & 0x0FFE))))  /* even index */
#endif

/* Type Test Macros */
#define LE_ADDR_IS_DIN(addr)         (((addr) & 0xF000U) == LE_REGION_DIN)
#define LE_ADDR_IS_DOUT(addr)        (((addr) & 0xF000U) == LE_REGION_DOUT)
#define LE_ADDR_IS_AIN(addr)         (((addr) & 0xF000U) == LE_REGION_AIN)
#define LE_ADDR_IS_AOUT(addr)        (((addr) & 0xF000U) == LE_REGION_AOUT)
#define LE_ADDR_IS_BOOL(addr)        (((addr) & 0xE000U) == LE_REGION_BOOL_REG)
#define LE_ADDR_IS_INT(addr)         (((addr) & 0xE000U) == LE_REGION_INT_REG)
#define LE_ADDR_IS_FLOAT(addr)       (((addr) & 0xE000U) == LE_REGION_FLOAT)
#if LE_ENABLE_COMPLEX
#define LE_ADDR_IS_CMPLX(addr)       (((addr) & 0xF000U) == LE_REGION_CMPLX)
#endif
#define LE_ADDR_IS_CONST(addr)       (((addr) & 0xF000U) == LE_REGION_CONST)

/* Architectural 16-bit Address Region Limits */
#define LE_ARCH_MAX_DIN         4096U
#define LE_ARCH_MAX_DOUT        4096U
#define LE_ARCH_MAX_AIN         4096U
#define LE_ARCH_MAX_AOUT        4096U
#define LE_ARCH_MAX_BOOL_REGS   8192U   /* 13-bit index (0x4000-0x5FFF) */
#define LE_ARCH_MAX_INT_REGS    8192U   /* 13-bit index (0x6000-0x7FFF) */
#define LE_ARCH_MAX_FLOATS      8192U   /* 13-bit index (0x8000-0x9FFF) */
#define LE_ARCH_MAX_COMPLEX     4096U   /* 12-bit index (0xA000-0xAFFF) */

#ifdef __cplusplus
}
#endif

#endif /* LE_ADDR_H */

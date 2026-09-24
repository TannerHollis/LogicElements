/**
 * @file le_rt.c
 * @brief Runtime state workspace backed by the on-disk state image.
 */
#include "le_rt.h"
#include <string.h>

/* The state workspace is a SLICE of the board's unified RAM workspace
 * (le_rt_bind). The loader carves it out after the register arena and memcpy's
 * the preconfigured state image into it. Storage and bounds are caller-supplied;
 * there is never a heap allocation. */
static uint8_t* s_ws = NULL;
static uint32_t s_ws_len = 0;

/* Byte offset of each kind group within the workspace (-1 = kind absent). */
static int32_t s_kind_base[LE_BLK_LAST];

/* Declared instance count per kind (set by the loader from the state-desc
 * table). le_rt_state rejects indices at or past this count. */
static uint16_t s_kind_count[LE_BLK_LAST];

/* Memoized per-kind state struct sizes (filled at bind so the hot
 * le_rt_state path is an array read instead of a per-access switch). */
static uint16_t s_kind_size[LE_BLK_LAST];

void le_rt_bind(uint8_t* base, uint32_t len)
{
    s_ws = base;
    s_ws_len = len;
    for (int k = 0; k < LE_BLK_LAST; k++) s_kind_size[k] = le_rt_kind_size((uint8_t)k);
}

void le_rt_reset(void)
{
    if (s_ws && s_ws_len > 0) memset(s_ws, 0, s_ws_len);
    for (int i = 0; i < LE_BLK_LAST; i++) {
        s_kind_base[i] = -1;
        s_kind_count[i] = 0;
    }
}

uint8_t* le_rt_workspace(void) { return s_ws; }
uint32_t le_rt_workspace_bytes(void) { return s_ws_len; }

uint16_t le_rt_kind_size(uint8_t kind)
{
    switch (kind) {
        case LE_BLK_TIMER:                 return (uint16_t)sizeof(le_timer_state_t);
        case LE_BLK_COUNTER:               return (uint16_t)sizeof(le_counter_state_t);
#if LE_ENABLE_PROTECTION
        case LE_BLK_PID:                   return (uint16_t)sizeof(le_pid_state_t);
        case LE_BLK_OVERCURRENT:           return (uint16_t)sizeof(le_overcurrent_state_t);
        case LE_BLK_PHASOR:                return (uint16_t)sizeof(le_phasor_state_t);
        case LE_BLK_SYMCOMP:               return (uint16_t)sizeof(le_symcomp_state_t);
        case LE_BLK_21:                    return (uint16_t)sizeof(le_dist21_state_t);
        case LE_BLK_DIFF_87:               return (uint16_t)sizeof(le_diff87_state_t);
        case LE_BLK_PHASE_COMP:            return (uint16_t)sizeof(le_comp33_state_t);
#endif
        case LE_BLK_SCALER:                return (uint16_t)sizeof(le_scale_state_t);
#if LE_ENABLE_DSP
        case LE_BLK_LPF:                   return (uint16_t)sizeof(le_lpf_state_t);
        case LE_BLK_BIQUAD:                return (uint16_t)sizeof(le_biquad_state_t);
        case LE_BLK_MOVING_AVG:            return (uint16_t)sizeof(le_moving_avg_state_t);
        case LE_BLK_RATE_LIMITER:          return (uint16_t)sizeof(le_rate_limiter_state_t);
        case LE_BLK_DEADBAND:              return (uint16_t)sizeof(le_deadband_state_t);
        case LE_BLK_WASHOUT:               return (uint16_t)sizeof(le_washout_state_t);
        case LE_BLK_PEAK:                  return (uint16_t)sizeof(le_peak_state_t);
        case LE_BLK_RMS:                   return (uint16_t)sizeof(le_rms_state_t);
        case LE_BLK_MEDIAN:                return (uint16_t)sizeof(le_median_state_t);
        case LE_BLK_DERIVATIVE:            return (uint16_t)sizeof(le_derivative_state_t);
        case LE_BLK_ZERO_CROSSING:         return (uint16_t)sizeof(le_zero_crossing_state_t);
        case LE_BLK_LUT_1D:                return (uint16_t)sizeof(le_lut_1d_state_t);
        case LE_BLK_TOTALIZER:             return (uint16_t)sizeof(le_totalizer_state_t);
        case LE_BLK_MIN_MAX_HOLD:          return (uint16_t)sizeof(le_min_max_hold_state_t);
#endif
#if LE_ENABLE_SERIAL_BUS
        case LE_BLK_I2C:                   return (uint16_t)sizeof(le_i2c_device_state_t);
        case LE_BLK_SPI:                   return (uint16_t)sizeof(le_spi_device_state_t);
#endif
        default:                           return 0;
    }
}

void le_rt_set_kind_base(uint8_t kind, int32_t base)
{
    if (kind < LE_BLK_LAST) s_kind_base[kind] = base;
}

int32_t le_rt_kind_base(uint8_t kind)
{
    return (kind < LE_BLK_LAST) ? s_kind_base[kind] : -1;
}

void le_rt_set_kind_count(uint8_t kind, uint16_t count)
{
    if (kind < LE_BLK_LAST) s_kind_count[kind] = count;
}

uint8_t* le_rt_state(uint8_t kind, uint16_t idx)
{
    if (!s_ws || s_ws_len == 0) return NULL;
    int32_t base = (kind < LE_BLK_LAST) ? s_kind_base[kind] : -1;
    if (base < 0) return NULL;
    uint16_t sz = (kind < LE_BLK_LAST) ? s_kind_size[kind] : 0;  /* memoized at bind */
    if (sz == 0) return NULL;
    if (idx >= ((kind < LE_BLK_LAST) ? s_kind_count[kind] : 0)) return NULL; /* past declared instances */
    uint32_t off = (uint32_t)base + (uint32_t)idx * (uint32_t)sz;
    if (off + (uint32_t)sz > s_ws_len) return NULL;
    return &s_ws[off];
}
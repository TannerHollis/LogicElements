/**
 * @file le_rt.c
 * @brief Runtime state workspace backed by the on-disk state image.
 */
#include "le_rt.h"
#include <string.h>

/* The single, static RAM workspace. The loader copies the preconfigured state
 * image here at load time. Never heap-allocated; sized by the platform. */
static uint8_t s_workspace[LE_STATE_WORKSPACE_BYTES];

/* Byte offset of each kind group within the workspace (-1 = kind absent). */
static int32_t s_kind_base[LE_BLK_LAST];

void le_rt_reset(void)
{
    memset(s_workspace, 0, sizeof(s_workspace));
    for (int i = 0; i < LE_BLK_LAST; i++) s_kind_base[i] = -1;
}

uint8_t* le_rt_workspace(void) { return s_workspace; }
uint32_t le_rt_workspace_bytes(void) { return LE_STATE_WORKSPACE_BYTES; }

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

uint8_t* le_rt_state(uint8_t kind, uint16_t idx)
{
    int32_t base = (kind < LE_BLK_LAST) ? s_kind_base[kind] : -1;
    if (base < 0) return NULL;
    uint16_t sz = le_rt_kind_size(kind);
    if (sz == 0) return NULL;
    uint32_t off = (uint32_t)base + (uint32_t)idx * (uint32_t)sz;
    if (off + (uint32_t)sz > LE_STATE_WORKSPACE_BYTES) return NULL;
    return &s_workspace[off];
}
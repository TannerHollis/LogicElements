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

/* Fixed scan period in seconds used by time-dependent opcodes when no scan
 * rate is configured (set by the scheduler alongside le_vm_set_scan_period_us). */
static float s_scan_dt = LE_DEFAULT_SCAN_DT_SEC;

void le_rt_set_scan_dt(float seconds) { s_scan_dt = (seconds > 0.0f) ? seconds : LE_DEFAULT_SCAN_DT_SEC; }
float le_rt_scan_dt(void) { return (s_scan_dt > 0.0f) ? s_scan_dt : LE_DEFAULT_SCAN_DT_SEC; }

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
    s_scan_dt = LE_DEFAULT_SCAN_DT_SEC; /* re-assert default when the workspace re-binds */
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

static const uint16_t s_kind_sizes[LE_BLK_LAST] = {
    0, /* LE_BLK_NONE */
#define LE_BLOCK_DEF(id, val, name, type, gate) (gate) ? (uint16_t)sizeof(type) : 0,
#include "le_blocks.def"
#undef LE_BLOCK_DEF
};

static const char* const s_kind_names[LE_BLK_LAST] = {
    "NONE",
#define LE_BLOCK_DEF(id, val, name, type, gate) name,
#include "le_blocks.def"
#undef LE_BLOCK_DEF
};

uint16_t le_rt_kind_size(uint8_t kind)
{
    return (kind < LE_BLK_LAST) ? s_kind_sizes[kind] : 0;
}

const char* le_rt_kind_name(uint8_t kind)
{
    return (kind < LE_BLK_LAST) ? s_kind_names[kind] : "UNKNOWN";
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

uint16_t le_rt_kind_count(uint8_t kind)
{
    return (kind < LE_BLK_LAST) ? s_kind_count[kind] : 0;
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
/**
 * @file le_rt.c
 * @brief Static zero-heap state arena and runtime block table implementation.
 */
#include "le_rt.h"
#include <string.h>

/* The single, fixed static state arena. Never heap-allocated. */
static uint8_t s_heap[LE_STATE_HEAP_BYTES];
static le_rt_block_t s_blocks[LE_MAX_RT_BLOCKS];
static uint8_t s_used[LE_MAX_RT_BLOCKS];
static int s_group_base[LE_BLK_LAST];   /* kind -> first runtime-block row */

/* Alignment-aware bump allocator cursor into s_heap. */
static uint32_t s_top = 0;

void le_rt_init(void)
{
    memset(s_heap, 0, sizeof(s_heap));
    memset(s_blocks, 0, sizeof(s_blocks));
    memset(s_used, 0, sizeof(s_used));
    for (int i = 0; i < LE_BLK_LAST; i++) s_group_base[i] = -1;
    s_top = 0;
}

uint8_t* le_rt_heap(void) { return s_heap; }
uint32_t le_rt_capacity(void) { return LE_STATE_HEAP_BYTES; }
uint32_t le_rt_used(void) { return s_top; }

int le_rt_row_count(void)
{
    int n = 0;
    for (int i = 0; i < LE_MAX_RT_BLOCKS; i++) if (s_used[i]) n++;
    return n;
}

static uint8_t* heap_alloc(uint16_t size, uint16_t align)
{
    if (align == 0) align = 1;
    uint32_t a = s_top;
    uint32_t mask = (uint32_t)(align - 1);
    if (a & mask) a = (a + align - 1) & ~mask;
    if (a + (uint32_t)size > LE_STATE_HEAP_BYTES) return NULL;
    s_top = a + size;
    return &s_heap[a];
}

int le_rt_reserve(uint8_t kind, uint16_t size, uint16_t align)
{
    if (kind == LE_BLK_NONE) return -1;
    int row = -1;
    for (int i = 0; i < LE_MAX_RT_BLOCKS; i++) {
        if (!s_used[i]) { row = i; break; }
    }
    if (row < 0) return -1;

    uint8_t* p = heap_alloc(size, align);
    if (!p) return -1;

    s_blocks[row].kind = kind;
    s_blocks[row].state = p;
    s_blocks[row].state_size = size;
    s_used[row] = 1;
    return row;
}

void le_rt_bind(int row, uint8_t kind, uint16_t size, uint8_t* state)
{
    if (row < 0 || row >= LE_MAX_RT_BLOCKS) return;
    s_blocks[row].kind = kind;
    s_blocks[row].state = state;
    s_blocks[row].state_size = size;
    s_used[row] = 1;
}

le_rt_block_t* le_rt_get(int row)
{
    if (row < 0 || row >= LE_MAX_RT_BLOCKS) return NULL;
    if (!s_used[row]) return NULL;
    return &s_blocks[row];
}

void le_rt_set_group_base(uint8_t kind, int base)
{
    if (kind < LE_BLK_LAST) s_group_base[kind] = base;
}

int le_rt_group_base(uint8_t kind)
{
    return (kind < LE_BLK_LAST) ? s_group_base[kind] : -1;
}
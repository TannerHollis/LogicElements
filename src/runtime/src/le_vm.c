/**
 * @file le_vm.c
 * @brief Implementation of LogicElements Virtual Machine runtime.
 */

#include "le_vm.h"
#include "le_opcodes.h"
#include "le_rt.h"
#include "le_hal.h"
#include <string.h>
#include <ctype.h>

le_status_t le_vm_init(le_vm_t* vm)
{
    if (!vm) return LE_ERR_NULL_PTR;

    memset(vm, 0, sizeof(le_vm_t));
    le_process_image_init(&vm->image);
    le_rt_bind(NULL, 0); /* unbind until a program load carves the state slice */
    le_rt_reset();       /* clear the state workspace + kind-base table */
    vm->running = false;
    vm->cycle_count = 0;
    return LE_OK;
}

le_status_t le_vm_load_program(le_vm_t* vm, const le_instruction_t* instructions, uint16_t count)
{
    if (!vm) return LE_ERR_NULL_PTR;

    vm->instructions = instructions;
    vm->instruction_count = count;
    return LE_OK;
}

le_status_t le_vm_load_blocks(le_vm_t* vm, const le_block_desc_t* blocks, uint16_t count)
{
    if (!vm) return LE_ERR_NULL_PTR;

    vm->blocks = blocks;
    vm->block_count = count;
    return LE_OK;
}

le_status_t le_vm_step_us(le_vm_t* vm, uint32_t now_us)
{
    if (!vm) return LE_ERR_NULL_PTR;

    /* Fixed-rate cadence enforcement: every scan must land EXACTLY one period
     * after the previous one. Any skip/retry is a host bug (silent drift would
     * corrupt DSP/phasing/timing-dependent logic). */
    if (vm->enforce_fixed_rate && vm->scan_period_us > 0) {
        if (vm->last_scan_us != 0 && now_us != vm->last_scan_us + vm->scan_period_us) {
            return LE_ERR_SCAN_JITTER;
        }
        vm->last_scan_us = now_us;
    }

    uint32_t now_ms = now_us / 1000u;
    uint32_t t0 = (g_le_hal && g_le_hal->get_time_us) ? g_le_hal->get_time_us() : 0u;

    /* Sweep pending pulses first so they clear even if the VM is currently
     * stopped (the simulator/CLI keeps calling step with an advancing clock). */
    if (vm->pulse_count > 0) {
        for (uint8_t i = 0; i < LE_MAX_PULSES; i++) {
            if (!vm->pulses[i].active) continue;
            if (vm->pulses[i].clear_after_ms == 0) {
                /* Anchor the pulse duration to this scan's clock. */
                vm->pulses[i].clear_after_ms = now_ms + vm->pulses[i].duration_ms;
                continue;
            }
            if (now_ms >= vm->pulses[i].clear_after_ms) {
                le_process_image_set_active(&vm->image, vm->pulses[i].addr, false);
                vm->pulses[i].active = false;
                vm->pulses[i].clear_after_ms = 0;
                if (vm->pulse_count > 0) vm->pulse_count--;
            }
        }
    }

    if (!vm->running || !vm->instructions || vm->instruction_count == 0) {
        return LE_OK;
    }

    le_status_t status = le_exec_program(vm->instructions, vm->instruction_count,
                                         &vm->image, now_ms,
                                         vm->blocks, vm->block_count);
    if (status != LE_OK) {
        return status;
    }

    vm->cycle_count++;

    /* Measure the actual scan duration and gauge the margin (HAL clock only). */
    if (t0 != 0u && g_le_hal && g_le_hal->get_time_us) {
        uint32_t spent = (uint32_t)(g_le_hal->get_time_us() - t0);
        if (spent > vm->observed_worst_us) vm->observed_worst_us = spent;
        if (vm->scan_period_us > 0 && spent > vm->scan_period_us) vm->scan_overruns++;
    }
    return LE_OK;
}

/**
 * @brief Compatibility wrapper: steps at millisecond resolution. When the scan
 * period is not an integer number of milliseconds, use @ref le_vm_step_us.
 */
le_status_t le_vm_step(le_vm_t* vm, uint32_t now_ms)
{
    return le_vm_step_us(vm, now_ms * 1000u);
}

le_status_t le_vm_set_scan_period_us(le_vm_t* vm, uint32_t period_us)
{
    if (!vm) return LE_ERR_NULL_PTR;
    vm->scan_period_us = period_us;
    vm->last_scan_us = 0;
    le_rt_set_scan_dt((period_us > 0) ? ((float)period_us / 1000000.0f) : LE_DEFAULT_SCAN_DT_SEC);
    return LE_OK;
}

void le_vm_set_enforce_fixed_rate(le_vm_t* vm, bool enable)
{
    if (!vm) return;
    vm->enforce_fixed_rate = enable ? 1 : 0;
    vm->last_scan_us = 0;
}

le_status_t le_vm_run_scan(le_vm_t* vm)
{
    if (!vm) return LE_ERR_NULL_PTR;
    if (vm->scan_period_us == 0) return LE_ERR_TIMING_BUDGET;

    uint32_t now_us = (g_le_hal && g_le_hal->get_time_us) ? g_le_hal->get_time_us() : 0u;
    uint32_t next = (vm->last_scan_us != 0) ? (vm->last_scan_us + vm->scan_period_us) : now_us;

    /* Busy-wait until the fixed-rate boundary (the timer is the truth). */
    if (g_le_hal && g_le_hal->get_time_us) {
        while (g_le_hal->get_time_us() < next) { /* spin */ }
    }
    return le_vm_step_us(vm, next);
}

void le_vm_start(le_vm_t* vm)
{
    if (vm) vm->running = true;
}

void le_vm_stop(le_vm_t* vm)
{
    if (vm) vm->running = false;
}

void le_vm_reset(le_vm_t* vm)
{
    if (!vm) return;
    /* Clear the shared RAM workspace (register arena + any bound state slice),
     * the process image descriptor, and the runtime state tables. */
    memset(vm->ram_workspace, 0, sizeof(vm->ram_workspace));
    le_process_image_init(&vm->image);
    le_rt_reset(); /* zeroes bound slice (if any) + clears kind bases */
    vm->cycle_count = 0;
    vm->pulse_count = 0;
    for (uint8_t i = 0; i < LE_MAX_PULSES; i++) {
        vm->pulses[i].active = false;
    }
    vm->last_scan_us = 0;
    vm->observed_worst_us = 0;
    vm->scan_overruns = 0;
}

/* ========================================================================== */
/* Alias + pulse support                                                      */
/* ========================================================================== */

/**
 * @brief Compares an alias's stored name against a user-supplied name.
 *
 * Comparison is case-insensitive. The user-supplied @p query may carry an
 * optional leading '%' for convenience; the stored name (in @p stored) is
 * NUL-/space-padded to LE_ALIAS_NAME_MAX characters.
 *
 * @param stored Alias name bytes as stored in the compact alias table.
 * @param query  User-facing name to match (leading '%' is ignored).
 * @return True if @p query matches @p stored, false otherwise.
 */
static bool le_alias_name_eq(const char* stored, const char* query)
{
    if (!stored || !query) return false;
    size_t q = 0;
    if (query[q] == '%') q++;
    for (size_t i = 0; i < LE_ALIAS_NAME_MAX; i++) {
        char a = stored[i];
        if (a == '\0' || a == ' ') a = '\0';
        char b = (q < strlen(query)) ? query[q] : '\0';
        if (a == '\0' && (b == '\0' || b == ' ')) return true;
        if (a == '\0' || b == '\0' || b == ' ') return false;
        if ((char)tolower((unsigned char)a) != (char)tolower((unsigned char)b)) return false;
        q++;
    }
    /* stored name already consumed: match only if query is also exhausted */
    while (q < strlen(query) && query[q] == ' ') q++;
    return q >= strlen(query);
}

/**
 * @brief Locates an alias by name, checking program aliases then board aliases.
 *
 * Program (circuit-declared) aliases take precedence over host/board aliases on
 * name collision.
 *
 * @param vm   The virtual machine whose alias tables are searched.
 * @param name Alias name to resolve (optional leading '%' is tolerated).
 * @return A pointer to the matching @ref le_alias_t entry, or NULL if none.
 */
static const le_alias_t* le_vm_find_alias(const le_vm_t* vm, const char* name)
{
    /* Program aliases take precedence, then board aliases. */
    for (uint16_t i = 0; i < vm->alias_count && vm->aliases; i++) {
        if (le_alias_name_eq(vm->aliases[i].name, name)) return &vm->aliases[i];
    }
    for (uint16_t i = 0; i < vm->board_alias_count && vm->board_aliases; i++) {
        if (le_alias_name_eq(vm->board_aliases[i].name, name)) return &vm->board_aliases[i];
    }
    return NULL;
}

le_status_t le_vm_load_aliases(le_vm_t* vm, const le_alias_t* aliases, uint16_t count)
{
    if (!vm) return LE_ERR_NULL_PTR;
    vm->aliases = aliases;
    vm->alias_count = count;
    return LE_OK;
}

le_status_t le_vm_load_board_aliases(le_vm_t* vm, const le_alias_t* aliases, uint16_t count)
{
    if (!vm) return LE_ERR_NULL_PTR;
    vm->board_aliases = aliases;
    vm->board_alias_count = count;
    return LE_OK;
}

le_status_t le_alias_lookup(const le_vm_t* vm, const char* name, uint16_t* out_addr)
{
    if (!vm || !name || !out_addr) return LE_ERR_NULL_PTR;
    const le_alias_t* a = le_vm_find_alias(vm, name);
    if (!a) return LE_ERR_NOT_FOUND;
    *out_addr = a->addr;
    return LE_OK;
}

le_status_t le_alias_set_bool(le_vm_t* vm, const char* name, bool val)
{
    uint16_t addr;
    le_status_t st = le_alias_lookup(vm, name, &addr);
    if (st != LE_OK) return st;
    if (!vm) return LE_ERR_NULL_PTR;
    le_process_image_set_bool(&vm->image, addr, val);
    return LE_OK;
}

le_status_t le_alias_set_float(le_vm_t* vm, const char* name, float val)
{
    uint16_t addr;
    le_status_t st = le_alias_lookup(vm, name, &addr);
    if (st != LE_OK) return st;
    if (!vm) return LE_ERR_NULL_PTR;
    le_process_image_set_float(&vm->image, addr, val);
    return LE_OK;
}

le_status_t le_alias_set_int(le_vm_t* vm, const char* name, int32_t val)
{
    uint16_t addr;
    le_status_t st = le_alias_lookup(vm, name, &addr);
    if (st != LE_OK) return st;
    if (!vm) return LE_ERR_NULL_PTR;
    le_process_image_set_int(&vm->image, addr, val);
    return LE_OK;
}

le_status_t le_alias_toggle(le_vm_t* vm, const char* name)
{
    uint16_t addr;
    le_status_t st = le_alias_lookup(vm, name, &addr);
    if (st != LE_OK) return st;
    if (!vm) return LE_ERR_NULL_PTR;
    bool cur = le_process_image_get_bool(&vm->image, addr);
    le_process_image_set_bool(&vm->image, addr, !cur);
    return LE_OK;
}

le_status_t le_vm_pulse(le_vm_t* vm, uint16_t addr, uint32_t duration_ms)
{
    if (!vm) return LE_ERR_NULL_PTR;
    if (addr == LE_ADDR_UNUSED || (addr & LE_ADDR_REGION_MASK) == LE_REGION_CONST) {
        return LE_ERR_OUT_OF_BOUNDS;
    }
    if (duration_ms == 0) duration_ms = 1000;

    /* Reuse an existing slot for the same address to refresh its duration. */
    uint8_t slot = LE_MAX_PULSES;
    for (uint8_t i = 0; i < LE_MAX_PULSES; i++) {
        if (vm->pulses[i].active && vm->pulses[i].addr == addr) { slot = i; break; }
    }
    if (slot == LE_MAX_PULSES) {
        for (uint8_t i = 0; i < LE_MAX_PULSES; i++) {
            if (!vm->pulses[i].active) { slot = i; break; }
        }
    }
    if (slot == LE_MAX_PULSES) return LE_ERR_CAPACITY;

    if (!vm->pulses[slot].active) vm->pulse_count++;
    vm->pulses[slot].addr = addr;
    vm->pulses[slot].active = true;
    vm->pulses[slot].duration_ms = duration_ms;
    vm->pulses[slot].clear_after_ms = 0; /* anchored on the next le_vm_step */

    /* Set the register active immediately. */
    le_process_image_set_active(&vm->image, addr, true);
    return LE_OK;
}

le_status_t le_alias_pulse_for(le_vm_t* vm, const char* name, float seconds)
{
    uint16_t addr;
    le_status_t st = le_alias_lookup(vm, name, &addr);
    if (st != LE_OK) return st;
    uint32_t ms = (seconds > 0.0f) ? (uint32_t)(seconds * 1000.0f) : 1000u;
    return le_vm_pulse(vm, addr, ms);
}

le_status_t le_alias_pulse(le_vm_t* vm, const char* name)
{
    return le_alias_pulse_for(vm, name, 1.0f); /* default: 1 second */
}

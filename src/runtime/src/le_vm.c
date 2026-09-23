/**
 * @file le_vm.c
 * @brief Implementation of LogicElements Virtual Machine runtime.
 */

#include "le_vm.h"
#include "le_opcodes.h"
#include "le_rt.h"
#include <string.h>

le_status_t le_vm_init(le_vm_t* vm)
{
    if (!vm) return LE_ERR_NULL_PTR;

    memset(vm, 0, sizeof(le_vm_t));
    le_process_image_init(&vm->image);
    le_rt_init(); /* reset the static state arena + runtime block table */
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

le_status_t le_vm_step(le_vm_t* vm, uint32_t now_ms)
{
    if (!vm) return LE_ERR_NULL_PTR;
    if (!vm->running || !vm->instructions || vm->instruction_count == 0) {
        return LE_OK;
    }

    for (uint16_t i = 0; i < vm->instruction_count; i++)
    {
        le_status_t status = le_exec_instruction_ex(&vm->instructions[i], &vm->image, now_ms,
                                                 vm->blocks, vm->block_count);
        if (status != LE_OK) {
            return status;
        }
    }

    vm->cycle_count++;
    return LE_OK;
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
    le_process_image_init(&vm->image);
    vm->cycle_count = 0;
}

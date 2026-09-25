/**
 * @file cmd_sim.cpp
 * @brief Subcommand handler: `le sim`
 */

#include "cli_commands.h"
#include "le_types.h"
#include "le_process_image.h"
#include "le_vm.h"
#include "le_loader.h"
#include "le_hal.h"
#include "le_storage.h"
#include "le_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#include <windows.h>
#endif

extern "C" const le_hal_t* le_hal_get_sim(void);

static int run_interactive_cli(void)
{
    const le_hal_t* sim = le_hal_get_sim();
    sim->init();
    le_hal_set(sim);

    le_storage_t storage;
    le_storage_init(&storage, sim);

    le_vm_t vm;
    le_vm_init(&vm);

    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    printf("============================================================\n");
    printf(" LOGICELEMENTS INTERACTIVE TERMINAL (CLI)\n");
    printf(" Real-time PLC engine running in desktop simulation.\n");
    printf(" Type 'help' for commands, 'upload <slot> hex' to upload.\n");
    printf(" Press Ctrl+C to exit.\n");
    printf("============================================================\n");

#if defined(_WIN32)
    bool is_interactive = (_isatty(_fileno(stdin)) != 0);
#else
    bool is_interactive = true;
#endif

    if (!is_interactive) {
        int ch;
        while ((ch = getchar()) != EOF) {
            le_cli_process_char(&cli, (uint8_t)ch);
        }
        return 0;
    }

    uint32_t last_step_ms = sim->get_time_ms();

    while (1) {
        uint32_t now = sim->get_time_ms();
        le_cli_poll(&cli, now);

#if defined(_WIN32)
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 3) { /* Ctrl+C */
                printf("\nExiting CLI session.\n");
                break;
            }
            le_cli_process_char(&cli, (uint8_t)ch);
        } else {
            Sleep(1);
        }
#else
        int ch = getchar();
        if (ch == EOF) break;
        le_cli_process_char(&cli, (uint8_t)ch);
#endif

        if (vm.running && (now - last_step_ms >= 10)) {
            le_vm_step(&vm, now);
            last_step_ms = now;
        }
    }

    return 0;
}

int cmd_sim(int argc, char* argv[])
{
    if (argc < 2 || strcmp(argv[1], "--cli") == 0 || strcmp(argv[1], "-i") == 0) {
        return run_interactive_cli();
    }

    const char* filename = argv[1];
    long max_cycles = (argc >= 3) ? atol(argv[2]) : 50;

    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Could not open file '%s'\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 65536) {
        printf("Error: Invalid file size (%ld bytes)\n", size);
        fclose(f);
        return 1;
    }

    uint8_t* buffer = (uint8_t*)malloc(size);
    if (fread(buffer, 1, size, f) != (size_t)size) {
        printf("Error: Failed reading file content\n");
        free(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);

    le_hal_set(le_hal_get_sim());
    le_vm_t vm;
    le_vm_init(&vm);

    le_status_t status = le_loader_load(&vm, buffer, size);
    if (status != LE_OK) {
        printf("Error: Failed to load .lebin program (status code: %d)\n", status);
        free(buffer);
        return 1;
    }

    printf("============================================================\n");
    printf(" LOGICELEMENTS SIMULATOR RUNNER\n");
    printf("============================================================\n");
    printf("Loaded program:   %s (%ld bytes)\n", filename, size);
    printf("Instructions:     %d\n", vm.instruction_count);
    printf("Running for:      %ld scan cycles\n", max_cycles);
    printf("------------------------------------------------------------\n");

    uint32_t sim_time_ms = 0;
    for (long cycle = 0; cycle < max_cycles; cycle++)
    {
        bool in0 = (cycle / 10) % 2 == 1;
        bool in1 = (cycle / 20) % 2 == 1;
        le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), in0);
        le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), in1);

        le_vm_step(&vm, sim_time_ms);

        bool out0 = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0));
        bool out1 = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1));

        if (cycle % 5 == 0 || cycle == max_cycles - 1) {
            printf("[Cycle %4ld | %5u ms] %%I[0]=%d, %%I[1]=%d  ==>  %%Q[0]=%d, %%Q[1]=%d\n",
                   cycle, sim_time_ms, in0, in1, out0, out1);
        }

        sim_time_ms += 10;
    }

    printf("============================================================\n");
    printf("Simulation completed successfully.\n");

    free(buffer);
    return 0;
}

/**
 * @file le_cli.c
 * @brief Implementation of interactive UART terminal shell and XMODEM-CRC transfer.
 */

#include "le_cli.h"
#include "le_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define XMODEM_SOH      0x01
#define XMODEM_EOT      0x04
#define XMODEM_ACK      0x06
#define XMODEM_NAK      0x15
#define XMODEM_CAN      0x18
#define XMODEM_C        0x43

static void cli_print(const char* str)
{
    if (!g_le_hal || !g_le_hal->uart_write || !str) return;
    g_le_hal->uart_write((const uint8_t*)str, strlen(str));
}

static void cli_putc(uint8_t ch)
{
    if (!g_le_hal || !g_le_hal->uart_write) return;
    g_le_hal->uart_write(&ch, 1);
}

static uint16_t xmodem_crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void le_cli_init(le_cli_t* cli, le_vm_t* vm, le_storage_t* storage)
{
    if (!cli) return;
    memset(cli, 0, sizeof(le_cli_t));
    cli->vm = vm;
    cli->storage = storage;
    cli->mode = LE_CLI_MODE_NORMAL;
}

static void cli_show_prompt(void)
{
    cli_print("\r\nLE> ");
}

static void cmd_help(void)
{
    cli_print("=== LogicElements Terminal Commands ===\r\n");
    cli_print("  help                    Show this menu\r\n");
    cli_print("  info                    Target MCU info & capacity limits\r\n");
    cli_print("  caps                    Dump machine-readable .leconfig profile (JSON)\r\n");
    cli_print("  nodes                   List custom board nodes (JSON)\r\n");
    cli_print("  status                  VM status & execution cycle count\r\n");
    cli_print("  slots                   List all configuration slots\r\n");
    cli_print("  select <slot>           Switch active slot and reload VM\r\n");
    cli_print("  run                     Start logic execution\r\n");
    cli_print("  stop                    Pause logic execution\r\n");
    cli_print("  reset                   Reset process image memory\r\n");
    cli_print("  io                      Print live Process Image (DIN, DOUT, BOOL)\r\n");
    cli_print("  force <addr> <val>      Force register or I/O value (e.g. 'force din0 1' or 'force bool0 1')\r\n");
cli_print("  pulse <name> [sec]       Pulse an alias for N seconds (default 1 s), e.g. \"pulse TRIP 2\"\r\n");
    cli_print("  upload <slot> [xmodem]  Upload .lebin via secure XMODEM-CRC\r\n");
    cli_print("  upload <slot> hex       Upload .lebin by pasting Hex string\r\n");
}

static void cmd_caps(le_cli_t* cli)
{
    (void)cli;
    const char* plat = (g_le_hal && g_le_hal->get_platform_name) ? g_le_hal->get_platform_name() : "Generic MCU";
    char buf[128];

    cli_print("\r\n{\r\n");
    cli_print("  \"device\": {\r\n");
    snprintf(buf, sizeof(buf), "    \"name\": \"%s\",\r\n", plat); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"firmware_version\": \"%d.0\",\r\n", LE_BIN_VERSION); cli_print(buf);
    cli_print("    \"protocol_version\": 1\r\n");
    cli_print("  },\r\n");
    cli_print("  \"limits\": {\r\n");
    snprintf(buf, sizeof(buf), "    \"digital_inputs\": %d,\r\n", LE_MAX_DIGITAL_IN); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"digital_outputs\": %d,\r\n", LE_MAX_DIGITAL_OUT); cli_print(buf);
#if LE_ENABLE_ANALOG
    snprintf(buf, sizeof(buf), "    \"analog_inputs\": %d,\r\n", LE_MAX_ANALOG_IN); cli_print(buf);
#endif
    snprintf(buf, sizeof(buf), "    \"bool_regs\": %d,\r\n", LE_MAX_BOOL_REGS); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"floats\": %d,\r\n", LE_MAX_FLOATS); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"workspace_bytes\": %d,\r\n", LE_RAM_WORKSPACE_BYTES); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"config_slots\": %d,\r\n", LE_MAX_CONFIG_SLOTS); cli_print(buf);
    snprintf(buf, sizeof(buf), "    \"slot_size_bytes\": %d\r\n", LE_SLOT_SIZE_BYTES); cli_print(buf);
    cli_print("  },\r\n");
    cli_print("  \"features\": {\r\n");
#if LE_ENABLE_PROTECTION
    cli_print("    \"protection\": true,\r\n");
#else
    cli_print("    \"protection\": false,\r\n");
#endif
#if LE_ENABLE_SERIAL_BUS
    cli_print("    \"serial_bus\": true,\r\n");
#else
    cli_print("    \"serial_bus\": false,\r\n");
#endif
    cli_print("  },\r\n");
    cli_print("  \"custom_nodes\": ");
    const char* cnodes = (g_le_hal && g_le_hal->get_custom_nodes_json) ?
                         g_le_hal->get_custom_nodes_json() : "[]";
    if (!cnodes || cnodes[0] == '\0') {
        cnodes = "[]";
    }
    cli_print(cnodes);
    cli_print("\r\n}\r\n");
}

static void cmd_info(le_cli_t* cli)
{
    char buf[128];
    const char* plat = (g_le_hal && g_le_hal->get_platform_name) ? g_le_hal->get_platform_name() : "Generic MCU";
    cli_print("\r\n=== LogicElements Runtime Info ===\r\n");
    snprintf(buf, sizeof(buf), "  Platform:        %s\r\n", plat); cli_print(buf);
    snprintf(buf, sizeof(buf), "  Firmware Ver:    v%d.0\r\n", LE_BIN_VERSION); cli_print(buf);
    snprintf(buf, sizeof(buf), "  Config Slots:    %d slots (%d bytes each)\r\n", LE_MAX_CONFIG_SLOTS, LE_SLOT_SIZE_BYTES); cli_print(buf);
    #if LE_ENABLE_ANALOG
    snprintf(buf, sizeof(buf), "  Process Image:   DIN:%d, DOUT:%d, AIN:%d, BOOL:%d, FLOAT:%d, INT:%d\r\n",
             LE_MAX_DIGITAL_IN, LE_MAX_DIGITAL_OUT, LE_MAX_ANALOG_IN, LE_MAX_BOOL_REGS, LE_MAX_FLOATS, LE_MAX_INT_REGS); cli_print(buf);
#else
    snprintf(buf, sizeof(buf), "  Process Image:   DIN:%d, DOUT:%d, BOOL:%d, FLOAT:%d, INT:%d\r\n",
             LE_MAX_DIGITAL_IN, LE_MAX_DIGITAL_OUT, LE_MAX_BOOL_REGS, LE_MAX_FLOATS, LE_MAX_INT_REGS); cli_print(buf);
#endif
    (void)cli;
}

static void cmd_status(le_cli_t* cli)
{
    char buf[128];
    cli_print("\r\n=== VM Status ===\r\n");
    snprintf(buf, sizeof(buf), "  State:           %s\r\n", (cli->vm && cli->vm->running) ? "RUNNING" : "STOPPED"); cli_print(buf);
    snprintf(buf, sizeof(buf), "  Active Slot:     Slot %d\r\n", cli->storage ? cli->storage->active_slot : 0); cli_print(buf);
    snprintf(buf, sizeof(buf), "  Instructions:    %d\r\n", cli->vm ? cli->vm->instruction_count : 0); cli_print(buf);
    snprintf(buf, sizeof(buf), "  Cycle Count:     %u cycles\r\n", cli->vm ? cli->vm->cycle_count : 0); cli_print(buf);
    if (cli->vm && cli->vm->scan_period_us > 0) {
        snprintf(buf, sizeof(buf), "  Scan Period:     %u us (%u Hz)\r\n",
                 cli->vm->scan_period_us,
                 cli->vm->scan_period_us ? (1000000u / cli->vm->scan_period_us) : 0u); cli_print(buf);
        if (cli->vm->timing_feasible) {
            snprintf(buf, sizeof(buf), "  Timing:          est %u us of %u us period (%u%% margin) [%u overruns]\r\n",
                     cli->vm->timed_worst_us, cli->vm->scan_period_us,
                     cli->vm->scan_period_us ? (uint16_t)(100u - (uint64_t)cli->vm->timed_worst_us * 100u / cli->vm->scan_period_us) : 0,
                     cli->vm->scan_overruns); cli_print(buf);
        } else {
            snprintf(buf, sizeof(buf), "  Timing:          no program loaded / not budgeted\r\n"); cli_print(buf);
        }
    }
}

static void cmd_slots(le_cli_t* cli)
{
    if (!cli->storage) return;

    cli_print("\r\n=== Configuration Storage Slots ===\r\n");
    char buf[128];
    uint8_t count = le_storage_get_slot_count(cli->storage);
    for (uint8_t s = 0; s < count; s++)
    {
        le_slot_info_t info;
        le_storage_get_slot_info(cli->storage, s, &info);
        bool is_active = (s == cli->storage->active_slot);

        if (info.valid) {
            snprintf(buf, sizeof(buf), "  Slot %d: [VALID]   %3d inst, %4u bytes, CRC 0x%08X %s\r\n",
                     s, info.instruction_count, info.program_size, info.crc32, is_active ? "(ACTIVE)" : "");
        } else {
            snprintf(buf, sizeof(buf), "  Slot %d: [EMPTY]   No valid program %s\r\n", s, is_active ? "(ACTIVE)" : "");
        }
        cli_print(buf);
    }
}

static void cmd_select(le_cli_t* cli, const char* arg)
{
    if (!cli->storage || !cli->vm) return;
    int slot = atoi(arg);
    if (slot < 0 || slot >= LE_MAX_CONFIG_SLOTS) {
        cli_print("Error: Invalid slot number.\r\n");
        return;
    }

    if (le_storage_activate_slot(cli->storage, (uint8_t)slot, cli->vm)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Switched to Slot %d. Loaded %d instructions.\r\n",
                 slot, cli->vm->instruction_count);
        cli_print(buf);
    } else {
        cli_print("Error: Slot does not contain a valid .lebin program.\r\n");
    }
}

static void cmd_io(le_cli_t* cli)
{
    if (!cli->vm) return;

    cli_print("\r\n=== Live Process Image ===\r\n");
    char buf[128];

    cli_print("  Digital Inputs:\r\n    ");
    for (int i = 0; i < 8 && i < LE_MAX_DIGITAL_IN; i++) {
        snprintf(buf, sizeof(buf), "DIN[%d]=%d  ", i, le_process_image_get_bool(&cli->vm->image, LE_ADDR_MAKE_DIN(i)));
        cli_print(buf);
    }
    cli_print("\r\n  Digital Outputs:\r\n    ");
    for (int i = 0; i < 8 && i < LE_MAX_DIGITAL_OUT; i++) {
        snprintf(buf, sizeof(buf), "DOUT[%d]=%d  ", i, le_process_image_get_bool(&cli->vm->image, LE_ADDR_MAKE_DOUT(i)));
        cli_print(buf);
    }
    cli_print("\r\n  Analog Inputs:\r\n    ");
#if LE_ENABLE_ANALOG
    for (int i = 0; i < 8 && i < LE_MAX_ANALOG_IN; i++) {
        snprintf(buf, sizeof(buf), "AIN[%d]=%ld (%.2f)  ", i, (long)le_process_image_get_int(&cli->vm->image, LE_ADDR_MAKE_AIN(i)), le_process_image_get_float(&cli->vm->image, LE_ADDR_MAKE_AIN(i)));
        cli_print(buf);
    }
#endif
    cli_print("\r\n  Boolean Registers:\r\n    ");
    for (int i = 0; i < 8 && i < LE_MAX_BOOL_REGS; i++) {
        snprintf(buf, sizeof(buf), "BOOL[%d]=%d  ", i, le_process_image_get_bool(&cli->vm->image, LE_ADDR_MAKE_BOOL_REG(i)));
        cli_print(buf);
    }
    cli_print("\r\n");
}

static void cmd_force(le_cli_t* cli, const char* args)
{
    if (!cli->vm) return;
    char addr_str[32] = {0};
    int val = 0;
    if (sscanf(args, "%31s %d", addr_str, &val) < 2) {
        cli_print("Usage: force <addr> <val> (e.g. 'force din0 1', 'force ain0 1024', 'force bool0 1' or 'force 0x0000 1')\r\n");
        return;
    }

/* New register mnemonics: normalize to uppercase, strip a leading '%',
     * then dispatch on the alpha prefix. Recognised: IN/DIN, OUT/DOUT, AIN,
     * B/BOOL, F/FLOAT, I/INT, C. Non-mnemonic input falls through to the
     * legacy raw-address / old-name parser below. */
    {
        char na[40] = {0};
        size_t nk = 0;
        if (addr_str[0] == '%') nk = 1;
        for (size_t nq = nk; nq < 32 && addr_str[nq]; nq++) {
            na[nq - nk] = (char)((addr_str[nq] >= 'a' && addr_str[nq] <= 'z')
                                 ? (addr_str[nq] - 'a' + 'A') : addr_str[nq]);
        }
        size_t nj = 0;
        while (na[nj] >= 'A' && na[nj] <= 'Z') nj++;
        size_t nlen = nj;
        int nidx = 0;
        if (na[nj] == '[') {
            nidx = (int)strtol(&na[nj + 1], NULL, 10);
        } else {
            nidx = (int)strtol(&na[nj], NULL, 10);
        }
        const char* tok = na;
        int dm = -1;
        if (nlen == 2 && strncmp(tok, "IN", 2) == 0) dm = 0;
        else if (nlen == 3 && strncmp(tok, "DIN", 3) == 0) dm = 0;
        else if (nlen == 3 && strncmp(tok, "OUT", 3) == 0) dm = 1;
        else if (nlen == 4 && strncmp(tok, "DOUT", 4) == 0) dm = 1;
        else if (nlen == 3 && strncmp(tok, "AIN", 3) == 0) dm = 2;
        else if (nlen == 1 && strncmp(tok, "B", 1) == 0) dm = 3;
        else if (nlen == 4 && strncmp(tok, "BOOL", 4) == 0) dm = 3;
        else if (nlen == 1 && strncmp(tok, "F", 1) == 0) dm = 4;
        else if (nlen == 5 && strncmp(tok, "FLOAT", 5) == 0) dm = 4;
        else if (nlen == 3 && strncmp(tok, "INT", 3) == 0) dm = 5;
        else if (nlen == 1 && strncmp(tok, "I", 1) == 0) dm = 5;
        else if (nlen == 1 && strncmp(tok, "C", 1) == 0) dm = 6;

        switch (dm)
        {
            case 0:
                le_process_image_set_bool(&cli->vm->image, LE_ADDR_MAKE_DIN((uint16_t)nidx), val != 0);
                cli_print("Value forced.\r\n");
                return;
            case 1:
                le_process_image_set_bool(&cli->vm->image, LE_ADDR_MAKE_DOUT((uint16_t)nidx), val != 0);
                cli_print("Value forced.\r\n");
                return;
            case 2:
                le_process_image_set_int(&cli->vm->image, LE_ADDR_MAKE_AIN((uint16_t)nidx), (int32_t)val);
                cli_print("Value forced.\r\n");
                return;
            case 3:
                le_process_image_set_bool(&cli->vm->image, LE_ADDR_MAKE_BOOL_REG((uint16_t)nidx), val != 0);
                cli_print("Value forced.\r\n");
                return;
            case 4:
                le_process_image_set_float(&cli->vm->image, LE_ADDR_MAKE_FLOAT((uint16_t)nidx), (float)val);
                cli_print("Value forced.\r\n");
                return;
            case 5:
                le_process_image_set_int(&cli->vm->image, LE_ADDR_MAKE_INT_REG((uint16_t)nidx), (int32_t)val);
                cli_print("Value forced.\r\n");
                return;
            case 6:
#if LE_ENABLE_COMPLEX
                le_process_image_set_complex(&cli->vm->image,
                                             (uint16_t)(LE_REGION_CMPLX | ((uint16_t)nidx & LE_ADDR_INDEX_MASK)),
                                             le_c_make((float)val, 0.0f));
#endif
                cli_print("Value forced.\r\n");
                return;
            default:
                break;
        }
    }
    uint16_t addr = LE_ADDR_UNUSED;
    if ((addr_str[0] == 'd' || addr_str[0] == 'D') &&
        (addr_str[1] == 'i' || addr_str[1] == 'I') &&
        (addr_str[2] == 'n' || addr_str[2] == 'N')) {
        addr = LE_ADDR_MAKE_DIN(atoi(&addr_str[3]));
    } else if ((addr_str[0] == 'd' || addr_str[0] == 'D') &&
               (addr_str[1] == 'o' || addr_str[1] == 'O') &&
               (addr_str[2] == 'u' || addr_str[2] == 'U') &&
               (addr_str[3] == 't' || addr_str[3] == 'T')) {
        addr = LE_ADDR_MAKE_DOUT(atoi(&addr_str[4]));
    } else if ((addr_str[0] == 'a' || addr_str[0] == 'A') &&
               (addr_str[1] == 'i' || addr_str[1] == 'I') &&
               (addr_str[2] == 'n' || addr_str[2] == 'N')) {
        addr = LE_ADDR_MAKE_AIN(atoi(&addr_str[3]));
        le_process_image_set_int(&cli->vm->image, addr, (int32_t)val);
        cli_print("Value forced.\r\n");
        return;
    } else if ((addr_str[0] == 'b' || addr_str[0] == 'B') &&
               (addr_str[1] == 'o' || addr_str[1] == 'O') &&
               (addr_str[2] == 'o' || addr_str[2] == 'O') &&
               (addr_str[3] == 'l' || addr_str[3] == 'L')) {
        addr = LE_ADDR_MAKE_BOOL_REG(atoi(&addr_str[4]));
    } else {
        addr = (uint16_t)strtoul(addr_str, NULL, 0);
    }

    le_process_image_set_bool(&cli->vm->image, addr, val != 0);
    cli_print("Value forced.\r\n");
}

/**
 * @brief Implements the interactive `pulse` terminal command.
 *
 * Pulses a register alias for a duration; with no argument the pulse holds for
 * the default 1 second, otherwise for the given non-negative number of seconds.
 * Dispatches to @ref le_alias_pulse_for, so the name must resolve via the VM's
 * alias tables (program then board aliases).
 *
 * @param cli  The interactive shell whose VM is pulsed.
 * @param args The raw argument token stream following the `pulse` keyword.
 */
static void cmd_pulse(le_cli_t* cli, const char* args)
{
    if (!cli->vm) return;
    char name[32] = {0};
    float secs = 1.0f;
    int parsed = sscanf(args, "%31s %f", name, &secs);
    if (parsed < 1 || name[0] == '\0') {
        cli_print("Usage: pulse <name> [seconds] (default 1 s).\r\n");
        return;
    }
    if (secs < 0.0f) secs = 0.0f;
    le_status_t st = le_alias_pulse_for(cli->vm, name, secs);
    if (st == LE_OK) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Pulsed '%%%s' for %.3f s.\r\n", name, (secs <= 0.0f) ? 1.0f : secs);
        cli_print(buf);
    } else if (st == LE_ERR_NOT_FOUND) {
        cli_print("Pulse error: alias not found.\r\n");
    } else {
        cli_print("Pulse error.\r\n");
    }
}

static void start_xmodem_upload(le_cli_t* cli, uint8_t slot)
{
    cli->mode = LE_CLI_MODE_XMODEM;
    cli->upload_slot = slot;
    cli->upload_offset = 0;
    cli->xmodem_expected_block = 1;
    cli->xmodem_retries = 0;
    cli->xmodem_idx = 0;
    cli->xmodem_last_c_ms = (g_le_hal && g_le_hal->get_time_ms) ? g_le_hal->get_time_ms() : 0;

    char buf[128];
    snprintf(buf, sizeof(buf), "\r\nReady for XMODEM-CRC upload to Slot %d.\r\nStart transfer in your terminal...\r\n", slot);
    cli_print(buf);

    /* Send initial 'C' to start XMODEM-CRC negotiation */
    cli_putc(XMODEM_C);
}

static void start_hex_upload(le_cli_t* cli, uint8_t slot)
{
    cli->mode = LE_CLI_MODE_HEX;
    cli->upload_slot = slot;
    cli->upload_offset = 0;
    cli->hex_byte = 0;
    cli->hex_high_nibble = true;

    char buf[128];
    snprintf(buf, sizeof(buf), "\r\nReady for HEX upload to Slot %d. Paste hex string (end with '.' or empty line):\r\n", slot);
    cli_print(buf);
}

static void handle_command(le_cli_t* cli, char* line)
{
    /* Strip leading whitespace */
    while (*line == ' ') line++;
    if (*line == '\0') {
        cli_show_prompt();
        return;
    }

    char cmd[32] = {0};
    char arg1[32] = {0};
    char arg2[32] = {0};
    sscanf(line, "%31s %31s %31s", cmd, arg1, arg2);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "info") == 0) {
        cmd_info(cli);
    } else if (strcmp(cmd, "caps") == 0 || strcmp(cmd, "config") == 0) {
        cmd_caps(cli);
    } else if (strcmp(cmd, "nodes") == 0 || strcmp(cmd, "custom_nodes") == 0) {
        const char* cnodes = (g_le_hal && g_le_hal->get_custom_nodes_json) ?
                             g_le_hal->get_custom_nodes_json() : "[]";
        cli_print("\r\n=== Custom Board Nodes ===\r\n");
        cli_print(cnodes ? cnodes : "[]");
        cli_print("\r\n");
    } else if (strcmp(cmd, "status") == 0) {
        cmd_status(cli);
    } else if (strcmp(cmd, "slots") == 0) {
        cmd_slots(cli);
    } else if (strcmp(cmd, "select") == 0) {
        cmd_select(cli, arg1);
    } else if (strcmp(cmd, "run") == 0) {
        if (cli->vm) le_vm_start(cli->vm);
        cli_print("VM Started.\r\n");
    } else if (strcmp(cmd, "stop") == 0) {
        if (cli->vm) le_vm_stop(cli->vm);
        cli_print("VM Stopped.\r\n");
    } else if (strcmp(cmd, "reset") == 0) {
        if (cli->vm) le_vm_reset(cli->vm);
        cli_print("VM Process Image Reset.\r\n");
    } else if (strcmp(cmd, "io") == 0) {
        cmd_io(cli);
    } else if (strcmp(cmd, "force") == 0) {
        cmd_force(cli, line + 5);
    } else if (strcmp(cmd, "pulse") == 0) {
        cmd_pulse(cli, line + 5);
    } else if (strcmp(cmd, "upload") == 0) {
        uint8_t slot = (uint8_t)atoi(arg1);
        if (slot >= LE_MAX_CONFIG_SLOTS) {
            cli_print("Error: Invalid slot number.\r\n");
        } else if (cli->storage && slot == cli->storage->active_slot) {
            cli_print("Error: Cannot upload into the active slot. Switch slots first.\r\n");
        } else {
            if (strcmp(arg2, "hex") == 0) {
                start_hex_upload(cli, slot);
                return;
            } else {
                start_xmodem_upload(cli, slot);
                return;
            }
        }
    } else {
        cli_print("Unknown command. Type 'help' for options.\r\n");
    }

    cli_show_prompt();
}

static void handle_xmodem_char(le_cli_t* cli, uint8_t ch)
{
    if (cli->xmodem_idx == 0) {
        if (ch == XMODEM_EOT) {
            /* End of transmission: validate the fully-staged phantom and only
             * then atomically commit it to the target slot. */
            cli_putc(XMODEM_ACK);
            cli->mode = LE_CLI_MODE_NORMAL;

            le_status_t st = le_storage_commit_upload(cli->storage, cli->upload_slot, cli->upload_offset);
            if (st == LE_OK) {
                le_header_t hdr;
                le_storage_verify_slot(cli->storage, cli->upload_slot, &hdr);
                char buf[128];
                snprintf(buf, sizeof(buf), "\r\n[OK] XMODEM Transfer Complete! Verified %d instructions in Slot %d.\r\n",
                         hdr.instruction_count, cli->upload_slot);
                cli_print(buf);
            } else {
                cli_print("\r\n[ERROR] CRC32 verification failed for uploaded program!\r\n");
            }
            cli_show_prompt();
            return;
        }

        if (ch == XMODEM_CAN) {
            cli_print("\r\nTransfer cancelled by sender.\r\n");
            cli->mode = LE_CLI_MODE_NORMAL;
            cli_show_prompt();
            return;
        }

        if (ch != XMODEM_SOH) {
            return; /* Ignore stray bytes */
        }
    }

    cli->xmodem_buf[cli->xmodem_idx++] = ch;

    /* 133 bytes: SOH (1) + Block (1) + ~Block (1) + Data (128) + CRC16 (2) */
    if (cli->xmodem_idx == 133)
    {
        uint8_t blk = cli->xmodem_buf[1];
        uint8_t inv_blk = cli->xmodem_buf[2];
        const uint8_t* payload = &cli->xmodem_buf[3];
        uint16_t packet_crc = ((uint16_t)cli->xmodem_buf[131] << 8) | cli->xmodem_buf[132];
        uint16_t calc_crc = xmodem_crc16(payload, 128);

        bool block_ok = (blk == cli->xmodem_expected_block) && ((blk + inv_blk) == 0xFF);
        bool crc_ok = (packet_crc == calc_crc);

        if (block_ok && crc_ok)
        {
            /* Stage 128 bytes into the hidden phantom slot */
            le_storage_write_chunk(cli->storage, le_storage_get_phantom_slot(cli->storage), cli->upload_offset, payload, 128);
            cli->upload_offset += 128;
            cli->xmodem_expected_block++;
            cli->xmodem_retries = 0;
            cli_putc(XMODEM_ACK);
        }
        else
        {
            cli->xmodem_retries++;
            if (cli->xmodem_retries > 10) {
                cli_putc(XMODEM_CAN);
                cli->mode = LE_CLI_MODE_NORMAL;
                cli_print("\r\nToo many retries. Transfer aborted.\r\n");
                cli_show_prompt();
                return;
            }
            cli_putc(XMODEM_NAK);
        }

        cli->xmodem_idx = 0;
    }
}

static void handle_hex_char(le_cli_t* cli, uint8_t ch)
{
    /* Ignore leading whitespace or newlines before hex payload begins */
    if (cli->upload_offset == 0 && (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t')) {
        return;
    }

    /* Ignore inter-byte spaces or formatting tabs */
    if (ch == ' ' || ch == '\t') {
        return;
    }

    if (ch == '\r' || ch == '\n' || ch == '.') {
        if (cli->upload_offset > sizeof(le_header_t)) {
            le_status_t st = le_storage_commit_upload(cli->storage, cli->upload_slot, cli->upload_offset);
            if (st == LE_OK) {
                le_header_t hdr;
                le_storage_verify_slot(cli->storage, cli->upload_slot, &hdr);
                char buf[128];
                snprintf(buf, sizeof(buf), "\r\n[OK] Hex Upload Complete! Loaded %d instructions into Slot %d.\r\n",
                         hdr.instruction_count, cli->upload_slot);
                cli_print(buf);
            } else {
                cli_print("\r\n[ERROR] CRC32 verification failed!\r\n");
            }
        } else {
            cli_print("\r\nUpload ended.\r\n");
        }
        cli->mode = LE_CLI_MODE_NORMAL;
        cli_show_prompt();
        return;
    }

    /* Convert hex nibble */
    int val = -1;
    if (ch >= '0' && ch <= '9') val = ch - '0';
    else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;

    if (val >= 0) {
        if (cli->hex_high_nibble) {
            cli->hex_byte = (uint8_t)(val << 4);
            cli->hex_high_nibble = false;
        } else {
            cli->hex_byte |= (uint8_t)val;
            cli->hex_high_nibble = true;
            /* Stage byte into the hidden phantom slot */
            le_storage_write_chunk(cli->storage, le_storage_get_phantom_slot(cli->storage), cli->upload_offset, &cli->hex_byte, 1);
            cli->upload_offset++;
        }
    }
}

void le_cli_process_char(le_cli_t* cli, uint8_t ch)
{
    if (!cli) return;

    if (cli->mode == LE_CLI_MODE_XMODEM) {
        handle_xmodem_char(cli, ch);
        return;
    }

    if (cli->mode == LE_CLI_MODE_HEX) {
        handle_hex_char(cli, ch);
        return;
    }

    /* NORMAL CLI Mode: Handle line buffer */
    if (ch == '\n' && cli->last_was_cr) {
        cli->last_was_cr = false;
        return; /* Collapse CRLF: ignore paired LF immediately following CR */
    }
    cli->last_was_cr = (ch == '\r');

    if (ch == '\r' || ch == '\n') {
        cli_print("\r\n");
        cli->line_buf[cli->line_len] = '\0';
        handle_command(cli, cli->line_buf);
        cli->line_len = 0;
        return;
    }

    /* Backspace */
    if (ch == 0x08 || ch == 0x7F) {
        if (cli->line_len > 0) {
            cli->line_len--;
            cli_print("\b \b");
        }
        return;
    }

    /* Printable characters */
    if (ch >= 0x20 && ch <= 0x7E) {
        if (cli->line_len < sizeof(cli->line_buf) - 1) {
            cli->line_buf[cli->line_len++] = (char)ch;
            cli_putc(ch); /* Echo */
        }
    }
}

void le_cli_poll(le_cli_t* cli, uint32_t now_ms)
{
    if (!cli) return;

    /* In XMODEM mode before first block, send 'C' every 3 seconds to initiate */
    if (cli->mode == LE_CLI_MODE_XMODEM && cli->upload_offset == 0 && cli->xmodem_idx == 0)
    {
        if (now_ms - cli->xmodem_last_c_ms >= 3000) {
            cli->xmodem_last_c_ms = now_ms;
            cli->xmodem_retries++;
            if (cli->xmodem_retries > 10) {
                cli->mode = LE_CLI_MODE_NORMAL;
                cli_print("\r\nXMODEM timeout.\r\n");
                cli_show_prompt();
            } else {
                cli_putc(XMODEM_C);
            }
        }
    }
}

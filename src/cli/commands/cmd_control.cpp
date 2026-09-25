/**
 * @file cmd_control.cpp
 * @brief Subcommand handler: `le control`
 */

#include "cli_commands.h"
#include "le_host_comms.h"
#include <iostream>
#include <string>

static void print_control_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " --port <port> <action> [args] [options]\n\n"
              << "Actions:\n"
              << "  start                  Start PLC virtual machine scan loop\n"
              << "  stop                   Stop PLC virtual machine execution\n"
              << "  reset                  Reset PLC memory and restore initial state\n"
              << "  select <slot>          Activate a stored configuration slot\n"
              << "  force <addr> <0|1>     Force a digital input/output to high or low\n"
              << "  pulse <addr> <ms>      Pulse a register high for a specified millisecond duration\n\n"
              << "Options:\n"
              << "  -p, --port <port>      Target serial port (e.g. COM3 or /dev/ttyUSB0) [REQUIRED]\n"
              << "  -b, --baud <baud>      Baud rate (default: 115200)\n"
              << "  -h, --help             Show this help message\n";
}

int cmd_control(int argc, char* argv[])
{
    if (argc < 2) {
        print_control_usage(argv[0]);
        return 1;
    }

    std::string port_name;
    uint32_t baud_rate = 115200;
    std::string action;
    uint16_t addr = 0;
    uint8_t force_val = 0;
    uint8_t select_slot = 0;
    uint32_t duration_ms = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_control_usage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_name = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baud_rate = (uint32_t)std::stoul(argv[++i]);
        } else if (action.empty() && arg[0] != '-') {
            action = arg;
            if (action == "force") {
                if (i + 2 < argc) {
                    addr = (uint16_t)std::stoul(argv[++i]);
                    force_val = (uint8_t)std::stoul(argv[++i]);
                } else {
                    std::cerr << "[ERROR] 'force' requires <addr> and <0|1>.\n";
                    return 1;
                }
            } else if (action == "pulse") {
                if (i + 2 < argc) {
                    addr = (uint16_t)std::stoul(argv[++i]);
                    duration_ms = (uint32_t)std::stoul(argv[++i]);
                } else {
                    std::cerr << "[ERROR] 'pulse' requires <addr> and <duration_ms>.\n";
                    return 1;
                }
            } else if (action == "select") {
                if (i + 1 < argc) {
                    select_slot = (uint8_t)std::stoul(argv[++i]);
                } else {
                    std::cerr << "[ERROR] 'select' requires <slot>.\n";
                    return 1;
                }
            }
        }
    }

    if (port_name.empty()) {
        std::cerr << "[ERROR] Serial port must be specified via --port <port>.\n";
        return 1;
    }

    if (action.empty()) {
        std::cerr << "[ERROR] No action specified (start, stop, reset, select, force, pulse).\n";
        print_control_usage(argv[0]);
        return 1;
    }

    le_host_comms_t* client = le_host_open(port_name.c_str(), baud_rate);
    if (!client) {
        std::cerr << "[ERROR] Failed to open serial port: " << port_name << "\n";
        return 1;
    }

    int rc = -1;
    if (action == "start") {
        rc = le_host_control(client, 1, 1000);
        if (rc == 0) std::cout << "[OK] PLC execution STARTED.\n";
    } else if (action == "stop") {
        rc = le_host_control(client, 2, 1000);
        if (rc == 0) std::cout << "[OK] PLC execution STOPPED.\n";
    } else if (action == "reset") {
        rc = le_host_control(client, 3, 1000);
        if (rc == 0) std::cout << "[OK] PLC execution RESET.\n";
    } else if (action == "select") {
        rc = le_host_activate_slot(client, select_slot, 1000);
        if (rc == 0) std::cout << "[OK] Activated config slot " << (int)select_slot << ".\n";
    } else if (action == "force") {
        rc = le_host_force_io(client, addr, force_val, 1000);
        if (rc == 0) std::cout << "[OK] Forced register " << addr << " to " << (int)force_val << ".\n";
    } else if (action == "pulse") {
        rc = le_host_pulse(client, addr, duration_ms, 1000);
        if (rc == 0) std::cout << "[OK] Pulsed register " << addr << " for " << duration_ms << " ms.\n";
    } else {
        std::cerr << "[ERROR] Unknown action: " << action << "\n";
    }

    if (rc != 0) {
        std::cerr << "[ERROR] Device returned error or timed out.\n";
    }

    le_host_close(client);
    return (rc == 0) ? 0 : 1;
}

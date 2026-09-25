/**
 * @file cmd_term.cpp
 * @brief Subcommand handler: `le term`
 */

#include "cli_commands.h"
#include "le_host_comms.h"
#include <iostream>
#include <string>

static void print_term_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " --port <port> [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>      Target serial port (e.g. COM3 or /dev/ttyUSB0) [REQUIRED]\n"
              << "  -b, --baud <baud>      Baud rate (default: 115200)\n"
              << "  -h, --help             Show this help message\n";
}

int cmd_term(int argc, char* argv[])
{
    if (argc < 2) {
        print_term_usage(argv[0]);
        return 1;
    }

    std::string port_name;
    uint32_t baud_rate = 115200;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_term_usage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_name = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baud_rate = (uint32_t)std::stoul(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_term_usage(argv[0]);
            return 1;
        }
    }

    if (port_name.empty()) {
        std::cerr << "[ERROR] Serial port must be specified via --port <port>.\n";
        return 1;
    }

    le_host_comms_t* client = le_host_open(port_name.c_str(), baud_rate);
    if (!client) {
        std::cerr << "[ERROR] Failed to open serial port: " << port_name << "\n";
        return 1;
    }

    le_host_run_terminal(client);

    le_host_close(client);
    return 0;
}

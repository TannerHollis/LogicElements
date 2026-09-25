/**
 * @file cmd_upload.cpp
 * @brief Subcommand handler: `le upload`
 */

#include "cli_commands.h"
#include "le_host_comms.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

static void print_upload_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " <program.lebin> --port <port> [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>      Target serial port (e.g. COM3 or /dev/ttyUSB0) [REQUIRED]\n"
              << "  -b, --baud <baud>      Baud rate (default: 115200)\n"
              << "  -c, --chunk <bytes>    Upload packet chunk size (default: 64, max: 120)\n"
              << "  -s, --slot <n>         Target config slot (default: 0; must not be the active slot)\n"
              << "  -r, --run              Automatically start PLC execution after upload\n"
              << "  -h, --help             Show this help message\n";
}

static void on_upload_progress(size_t uploaded, size_t total, void* /*user_data*/)
{
    int percent = (total > 0) ? (int)((uploaded * 100) / total) : 0;
    int bar_width = 30;
    int pos = (percent * bar_width) / 100;

    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << percent << "% (" << uploaded << "/" << total << " B)" << std::flush;
}

int cmd_upload(int argc, char* argv[])
{
    if (argc < 2) {
        print_upload_usage(argv[0]);
        return 1;
    }

    std::string lebin_path;
    std::string port_name;
    uint32_t baud_rate = 115200;
    uint16_t chunk_size = 64;
    uint8_t target_slot = 0;
    bool auto_run = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_upload_usage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_name = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baud_rate = (uint32_t)std::stoul(argv[++i]);
        } else if ((arg == "-c" || arg == "--chunk") && i + 1 < argc) {
            chunk_size = (uint16_t)std::stoul(argv[++i]);
        } else if ((arg == "-s" || arg == "--slot") && i + 1 < argc) {
            target_slot = (uint8_t)std::stoul(argv[++i]);
        } else if (arg == "-r" || arg == "--run") {
            auto_run = true;
        } else if (arg[0] != '-' && lebin_path.empty()) {
            lebin_path = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_upload_usage(argv[0]);
            return 1;
        }
    }

    if (lebin_path.empty()) {
        std::cerr << "[ERROR] No .lebin file specified.\n";
        return 1;
    }

    if (port_name.empty()) {
        std::cerr << "[ERROR] Serial port must be specified via --port <port>.\n";
        return 1;
    }

    std::ifstream in(lebin_path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "[ERROR] Unable to open binary file: " << lebin_path << "\n";
        return 1;
    }

    std::vector<uint8_t> lebin_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (lebin_data.empty()) {
        std::cerr << "[ERROR] Binary file is empty: " << lebin_path << "\n";
        return 1;
    }

    std::cout << "Target: " << port_name << " @ " << baud_rate << " baud\n";
    std::cout << "File:   " << lebin_path << " (" << lebin_data.size() << " bytes)\n";

    le_host_comms_t* client = le_host_open(port_name.c_str(), baud_rate);
    if (!client) {
        std::cerr << "[ERROR] Failed to open serial port: " << port_name << "\n";
        return 1;
    }

    std::cout << "Connecting and handshaking with microcontroller...\n";
    char err_buf[256] = {0};

    int rc = le_host_upload_program(
        client,
        target_slot,
        lebin_data.data(),
        lebin_data.size(),
        chunk_size,
        on_upload_progress,
        NULL,
        err_buf,
        sizeof(err_buf)
    );

    std::cout << "\n";

    if (rc != 0) {
        std::cerr << "[ERROR] Upload failed: " << (err_buf[0] ? err_buf : "Protocol error") << " (code: " << rc << ")\n";
        le_host_close(client);
        return 1;
    }

    std::cout << "[OK] Program uploaded and verified successfully by target microcontroller!\n";

    if (auto_run) {
        /* Upload stores to the target slot but does not auto-activate it, so
         * activate the just-uploaded slot before starting execution. */
        std::cout << "Activating config slot " << (int)target_slot << "...\n";
        if (le_host_activate_slot(client, target_slot, 1000) != 0) {
            std::cerr << "[WARNING] Failed to activate config slot " << (int)target_slot << ".\n";
        }

        std::cout << "Starting PLC runtime execution...\n";
        if (le_host_control(client, 1, 1000) == 0) {
            std::cout << "[OK] PLC execution STARTED.\n";
        } else {
            std::cerr << "[WARNING] Failed to send START command.\n";
        }
    }

    le_host_close(client);
    return 0;
}

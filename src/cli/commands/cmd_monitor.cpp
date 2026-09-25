/**
 * @file cmd_monitor.cpp
 * @brief Subcommand handler: `le monitor`
 */

#include "cli_commands.h"
#include "le_host_comms.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

static void print_monitor_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " --port <port> [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>          Target serial port (e.g. COM3 or /dev/ttyUSB0) [REQUIRED]\n"
              << "  -b, --baud <baud>          Baud rate (default: 115200)\n"
              << "  -i, --interval <ms>        Telemetry polling interval in ms (default: 100)\n"
              << "  -n, --count <num>          Number of poll iterations (default: 0 = continuous)\n"
              << "  -h, --help                 Show this help message\n";
}

int cmd_monitor(int argc, char* argv[])
{
    if (argc < 2) {
        print_monitor_usage(argv[0]);
        return 1;
    }

    std::string port_name;
    uint32_t baud_rate = 115200;
    uint32_t interval_ms = 100;
    uint32_t max_count = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_monitor_usage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_name = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baud_rate = (uint32_t)std::stoul(argv[++i]);
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_ms = (uint32_t)std::stoul(argv[++i]);
        } else if ((arg == "-n" || arg == "--count") && i + 1 < argc) {
            max_count = (uint32_t)std::stoul(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_monitor_usage(argv[0]);
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

    // Query capabilities first to know channel counts
    le_caps_payload_t caps;
    memset(&caps, 0, sizeof(caps));
    caps.max_digital_in = 16;
    caps.max_digital_out = 16;
    caps.max_bool_regs = 128;

    if (le_host_get_caps(client, &caps, 1000) == 0) {
        std::cout << "[CONNECTED] Device: " << caps.platform_name
                  << " (DIN: " << caps.max_digital_in
                  << ", DOUT: " << caps.max_digital_out
                  << ", BOOL: " << caps.max_bool_regs << ")\n";
    } else {
        std::cout << "[WARNING] Could not read board caps; using default 16 DIN / 16 DOUT.\n";
    }

    std::cout << "Monitoring process image at " << interval_ms << " ms rate (Press Ctrl+C to stop)...\n\n";

    uint32_t iteration = 0;
    while (max_count == 0 || iteration < max_count) {
        iteration++;
        uint8_t img[64];
        size_t len = 0;

        int rc = le_host_get_image(client, img, sizeof(img), &len, 500);
        if (rc == 0 && len > 0) {
            std::cout << "\r[" << std::setw(5) << iteration << "] ";

            // Digital Inputs
            std::cout << "DIN: [";
            for (int i = 0; i < (int)caps.max_digital_in; ++i) {
                bool bit = (img[i >> 3] & (1U << (i & 7))) != 0;
                std::cout << (bit ? "1" : "0");
                if ((i + 1) % 8 == 0 && i + 1 < (int)caps.max_digital_in) std::cout << " ";
            }
            std::cout << "] ";

            // Digital Outputs
            std::cout << "DOUT: [";
            int base_dout = caps.max_digital_in;
            for (int i = 0; i < (int)caps.max_digital_out; ++i) {
                int bit_idx = base_dout + i;
                bool bit = (img[bit_idx >> 3] & (1U << (bit_idx & 7))) != 0;
                std::cout << (bit ? "1" : "0");
                if ((i + 1) % 8 == 0 && i + 1 < (int)caps.max_digital_out) std::cout << " ";
            }
            std::cout << "]" << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    std::cout << "\n";
    le_host_close(client);
    return 0;
}

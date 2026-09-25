/**
 * @file cmd_board.cpp
 * @brief Subcommand handler: `le board`
 */

#include "cli_commands.h"
#include "le_host_comms.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

static void print_board_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --info <path>          Inspect and display limits of a .leconfig profile\n"
              << "  --scan                 Scan and list available serial ports on the host\n"
              << "  -p, --port <port>      Connect to live MCU over serial to query capabilities\n"
              << "  -b, --baud <baud>      Serial baud rate (default: 115200)\n"
              << "  -o, --output <path>    Save queried capabilities or template to .leconfig JSON\n"
              << "  --template             Generate a starter .leconfig template\n"
              << "  -h, --help             Show this help message\n";
}

static void print_caps_report(const le_caps_payload_t& caps)
{
    std::cout << "\n============================================================\n"
              << " Target Device Capabilities\n"
              << "============================================================\n"
              << " Platform Name:        " << caps.platform_name << "\n"
              << " Protocol Version:     " << (int)caps.protocol_version << "\n"
              << " Firmware Version:     " << (int)caps.firmware_major << "." << (int)caps.firmware_minor << "\n"
              << " Digital Inputs (%I):  " << caps.max_digital_in << "\n"
              << " Digital Outputs (%Q): " << caps.max_digital_out << "\n"
              << " Analog Inputs (%AIN): " << caps.max_analog_in << "\n"
              << " Boolean Coils (%M):   " << caps.max_bool_regs << "\n"
              << " Float Registers (%R): " << caps.max_floats << "\n"
              << " RAM Workspace:        " << caps.workspace_bytes << " bytes\n"
              << " Config Slots:         " << (int)caps.config_slots << " x " << caps.slot_size_bytes << " bytes\n"
              << " Feature Flags:        0x" << std::hex << caps.feature_flags << std::dec << "\n"
              << "   - Protection:       " << ((caps.feature_flags & LE_CAP_PROTECTION) ? "YES" : "NO") << "\n"
              << "   - Serial Bus:       " << ((caps.feature_flags & LE_CAP_SERIAL_BUS) ? "YES" : "NO") << "\n"
              << "   - DSP & Filters:    " << ((caps.feature_flags & LE_CAP_DSP) ? "YES" : "NO") << "\n"
              << "   - Complex Arith:    " << ((caps.feature_flags & LE_CAP_COMPLEX) ? "YES" : "NO") << "\n"
              << "============================================================\n";
}

int cmd_board(int argc, char* argv[])
{
    if (argc < 2) {
        print_board_usage(argv[0]);
        return 1;
    }

    std::string info_path;
    std::string port_name;
    uint32_t baud_rate = 115200;
    std::string output_path;
    bool scan_ports = false;
    bool gen_template = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_board_usage(argv[0]);
            return 0;
        } else if (arg == "--scan") {
            scan_ports = true;
        } else if (arg == "--template") {
            gen_template = true;
        } else if (arg == "--info" && i + 1 < argc) {
            info_path = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_name = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baud_rate = (uint32_t)std::stoul(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_board_usage(argv[0]);
            return 1;
        }
    }

    if (scan_ports) {
        char ports[32][64];
        int count = le_serial_list_ports(ports, 32);
        std::cout << "Available serial communication ports (" << count << " found):\n";
        for (int i = 0; i < count; ++i) {
            std::cout << "  [" << (i + 1) << "] " << ports[i] << "\n";
        }
        return 0;
    }

    if (gen_template) {
        std::string tpl = "{\n"
                          "  \"device\": {\n"
                          "    \"name\": \"Custom Microcontroller\",\n"
                          "    \"firmware_version\": \"1.0\",\n"
                          "    \"protocol_version\": 3\n"
                          "  },\n"
                          "  \"limits\": {\n"
                          "    \"digital_inputs\": 16,\n"
                          "    \"digital_outputs\": 16,\n"
                          "    \"analog_inputs\": 4,\n"
                          "    \"bool_registers\": 128,\n"
                          "    \"floats\": 64,\n"
                          "    \"workspace_bytes\": 4096,\n"
                          "    \"config_slots\": 2,\n"
                          "    \"slot_size_bytes\": 2048\n"
                          "  },\n"
                          "  \"features\": {\n"
                          "    \"protection\": true,\n"
                          "    \"serial_bus\": true,\n"
                          "    \"dsp\": true\n"
                          "  },\n"
                          "  \"pin_map\": {\n"
                          "    \"inputs\": {\"USER_BUTTON\": \"%I0\"},\n"
                          "    \"outputs\": {\"STATUS_LED\": \"%Q0\"}\n"
                          "  }\n"
                          "}\n";
        if (!output_path.empty()) {
            std::ofstream out(output_path);
            if (!out.is_open()) {
                std::cerr << "Error writing template to: " << output_path << "\n";
                return 1;
            }
            out << tpl;
            out.close();
            std::cout << "[OK] Template written to: " << output_path << "\n";
        } else {
            std::cout << tpl;
        }
        return 0;
    }

    if (!info_path.empty()) {
        std::ifstream in(info_path);
        if (!in.is_open()) {
            std::cerr << "Error opening profile: " << info_path << "\n";
            return 1;
        }
        std::cout << "Board Profile: " << info_path << "\n";
        std::string line;
        while (std::getline(in, line)) {
            std::cout << "  " << line << "\n";
        }
        return 0;
    }

    if (!port_name.empty()) {
        std::cout << "Connecting to live hardware on " << port_name << " @ " << baud_rate << " baud...\n";
        le_host_comms_t* client = le_host_open(port_name.c_str(), baud_rate);
        if (!client) {
            std::cerr << "[ERROR] Could not open serial port: " << port_name << "\n";
            return 1;
        }

        std::cout << "Querying device capabilities...\n";
        le_caps_payload_t caps;
        int rc = le_host_get_caps(client, &caps, 2000);
        if (rc != 0) {
            std::cerr << "[ERROR] Failed to retrieve capabilities from target device.\n";
            le_host_close(client);
            return 1;
        }

        print_caps_report(caps);

        char custom_nodes[2048];
        int cn_len = le_host_get_custom_nodes(client, custom_nodes, sizeof(custom_nodes), 1500);
        if (cn_len > 0) {
            std::cout << "Custom Nodes: " << custom_nodes << "\n";
        }

        if (!output_path.empty()) {
            std::ofstream out(output_path);
            if (out.is_open()) {
                out << "{\n"
                    << "  \"device\": {\n"
                    << "    \"name\": \"" << caps.platform_name << "\",\n"
                    << "    \"firmware_version\": \"" << (int)caps.firmware_major << "." << (int)caps.firmware_minor << "\",\n"
                    << "    \"protocol_version\": " << (int)caps.protocol_version << "\n"
                    << "  },\n"
                    << "  \"limits\": {\n"
                    << "    \"digital_inputs\": " << caps.max_digital_in << ",\n"
                    << "    \"digital_outputs\": " << caps.max_digital_out << ",\n"
                    << "    \"analog_inputs\": " << caps.max_analog_in << ",\n"
                    << "    \"bool_registers\": " << caps.max_bool_regs << ",\n"
                    << "    \"floats\": " << caps.max_floats << ",\n"
                    << "    \"workspace_bytes\": " << caps.workspace_bytes << ",\n"
                    << "    \"config_slots\": " << (int)caps.config_slots << ",\n"
                    << "    \"slot_size_bytes\": " << caps.slot_size_bytes << "\n"
                    << "  },\n"
                    << "  \"features\": {\n"
                    << "    \"protection\": " << ((caps.feature_flags & LE_CAP_PROTECTION) ? "true" : "false") << ",\n"
                    << "    \"serial_bus\": " << ((caps.feature_flags & LE_CAP_SERIAL_BUS) ? "true" : "false") << ",\n"
                    << "    \"dsp\": " << ((caps.feature_flags & LE_CAP_DSP) ? "true" : "false") << "\n"
                    << "  }\n"
                    << "}\n";
                out.close();
                std::cout << "[OK] Saved discovered profile to: " << output_path << "\n";
            }
        }

        le_host_close(client);
        return 0;
    }

    print_board_usage(argv[0]);
    return 1;
}

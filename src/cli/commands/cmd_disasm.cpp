/**
 * @file cmd_disasm.cpp
 * @brief Subcommand handler: `le disasm`
 */

#include "cli_commands.h"
#include "le_compiler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void print_disasm_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " <program.lebin> [options]\n\n"
              << "Options:\n"
              << "  -o, --output <path>    Output disassembly text file (default: stdout)\n"
              << "  -b, --board <path>     Target board profile (.leconfig) to resolve custom nodes\n"
              << "  -h, --help             Show this help message\n";
}

int cmd_disasm(int argc, char* argv[])
{
    if (argc < 2) {
        print_disasm_usage(argv[0]);
        return 1;
    }

    std::string input_bin_path;
    std::string output_txt_path;
    std::string board_profile_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_disasm_usage(argv[0]);
            return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_txt_path = argv[++i];
        } else if ((arg == "-b" || arg == "--board") && i + 1 < argc) {
            board_profile_path = argv[++i];
        } else if (arg[0] != '-' && input_bin_path.empty()) {
            input_bin_path = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_disasm_usage(argv[0]);
            return 1;
        }
    }

    if (input_bin_path.empty()) {
        std::cerr << "Error: No input .lebin binary file specified.\n";
        return 1;
    }

    std::ifstream in_bin(input_bin_path, std::ios::binary);
    if (!in_bin.is_open()) {
        std::cerr << "Error: Unable to open binary file: " << input_bin_path << "\n";
        return 1;
    }

    std::vector<uint8_t> bin_data((std::istreambuf_iterator<char>(in_bin)), std::istreambuf_iterator<char>());
    in_bin.close();

    if (bin_data.empty()) {
        std::cerr << "Error: Binary file is empty: " << input_bin_path << "\n";
        return 1;
    }

    std::string board_json;
    if (!board_profile_path.empty()) {
        std::ifstream b_file(board_profile_path, std::ios::binary);
        if (b_file.is_open()) {
            std::stringstream b_buf;
            b_buf << b_file.rdbuf();
            board_json = b_buf.str();
        }
    }

    char* disasm_text = NULL;
    char* error_msg = NULL;

    int rc = le_disassemble_bin(
        bin_data.data(),
        bin_data.size(),
        -1, -1, -1,
        board_json.empty() ? NULL : board_json.c_str(),
        &disasm_text,
        &error_msg
    );

    if (rc != 0 || !disasm_text) {
        std::cerr << "[ERROR] Disassembly failed: " << (error_msg ? error_msg : "Unknown error") << "\n";
        if (error_msg) le_compiler_free_buffer(error_msg);
        return 1;
    }

    if (!output_txt_path.empty()) {
        std::ofstream out_file(output_txt_path);
        if (!out_file.is_open()) {
            std::cerr << "Error: Unable to open output file: " << output_txt_path << "\n";
            le_compiler_free_buffer(disasm_text);
            return 1;
        }
        out_file << disasm_text;
        out_file.close();
        std::cout << "[OK] Disassembly written to: " << output_txt_path << "\n";
    } else {
        std::cout << disasm_text << "\n";
    }

    le_compiler_free_buffer(disasm_text);
    return 0;
}

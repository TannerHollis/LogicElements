/**
 * @file cmd_compile.cpp
 * @brief Subcommand handler: `le compile`
 */

#include "cli_commands.h"
#include "le_compiler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void print_compile_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " <input_json> [options]\n\n"
              << "Options:\n"
              << "  -o, --output <path>    Output .lebin binary file path\n"
              << "  -b, --board <path>     Target board profile (.leconfig) path\n"
              << "  -c, --header <path>    Output C header file (.h) path\n"
              << "  -d, --disasm <path>    Output disassembly listing (.txt) path\n"
              << "  -O0                    Disable all optimizations (1:1 AST mapping)\n"
              << "  -O1                    Basic safe optimizations\n"
              << "  -O2                    Default optimizations (DCE, CSE, const folding, inversion, direct dest)\n"
              << "  -Os                    Size-focused optimizations\n"
              << "  --stats                Print optimization statistics\n"
              << "  -v, --version          Print compiler version\n"
              << "  -h, --help             Show this help message\n";
}

int cmd_compile(int argc, char* argv[])
{
    if (argc < 2) {
        print_compile_usage(argv[0]);
        return 1;
    }

    std::string input_json_path;
    std::string output_bin_path;
    std::string board_profile_path;
    std::string output_header_path;
    std::string output_disasm_path;
    bool print_stats = false;
    le_compiler_options_t options = le_compiler_options_init_default();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_compile_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "LogicElements Compiler v" << le_compiler_get_version() << "\n";
            return 0;
        } else if (arg == "-O0") {
            options = le_compiler_options_init(LE_OPT_NONE);
        } else if (arg == "-O1") {
            options = le_compiler_options_init(LE_OPT_BASIC);
        } else if (arg == "-O2") {
            options = le_compiler_options_init(LE_OPT_FULL);
        } else if (arg == "-Os") {
            options = le_compiler_options_init(LE_OPT_SIZE);
        } else if (arg == "--stats") {
            print_stats = true;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_bin_path = argv[++i];
        } else if ((arg == "-b" || arg == "--board") && i + 1 < argc) {
            board_profile_path = argv[++i];
        } else if ((arg == "-c" || arg == "--header") && i + 1 < argc) {
            output_header_path = argv[++i];
        } else if ((arg == "-d" || arg == "--disasm") && i + 1 < argc) {
            output_disasm_path = argv[++i];
        } else if (arg[0] != '-' && input_json_path.empty()) {
            input_json_path = arg;
        } else {
            std::cerr << "Unknown or invalid argument: " << arg << "\n";
            print_compile_usage(argv[0]);
            return 1;
        }
    }

    if (input_json_path.empty()) {
        std::cerr << "Error: No input circuit JSON file specified.\n";
        return 1;
    }

    if (output_bin_path.empty() && output_header_path.empty() && output_disasm_path.empty()) {
        size_t dot = input_json_path.rfind('.');
        if (dot != std::string::npos) {
            output_bin_path = input_json_path.substr(0, dot) + ".lebin";
        } else {
            output_bin_path = input_json_path + ".lebin";
        }
    }

    std::ifstream in_file(input_json_path, std::ios::binary);
    if (!in_file.is_open()) {
        std::cerr << "Error: Unable to open input file: " << input_json_path << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << in_file.rdbuf();
    std::string circuit_json = buffer.str();
    in_file.close();

    std::string board_json;
    if (!board_profile_path.empty()) {
        std::ifstream b_file(board_profile_path, std::ios::binary);
        if (!b_file.is_open()) {
            std::cerr << "Error: Unable to open board profile: " << board_profile_path << "\n";
            return 1;
        }
        std::stringstream b_buf;
        b_buf << b_file.rdbuf();
        board_json = b_buf.str();
        b_file.close();
        std::cout << "[INFO] Compiling against target board profile: " << board_profile_path << "\n";
    }

    le_compile_result_t result;
    int rc = le_compile_json_ex(circuit_json.c_str(), board_json.c_str(), &options, &result);

    if (result.warnings && *result.warnings) {
        std::cout << "[WARNING]\n" << result.warnings << "\n";
    }

    if (rc != 0 || !result.success) {
        std::cerr << "[ERROR] Compilation failed:\n"
                  << (result.error_message ? result.error_message : "Unknown error") << "\n";
        le_compile_result_free(&result);
        return 1;
    }

    if (!board_json.empty()) {
        std::cout << "[OK] Board capabilities validation PASSED.\n";
    }

    if (print_stats || options.opt_level != LE_OPT_NONE) {
        std::cout << "[INFO] Optimization stats: "
                  << result.eliminated_instructions << " instructions eliminated, "
                  << result.eliminated_registers << " registers eliminated.\n";
    }

    if (!output_bin_path.empty()) {
        std::ofstream out_bin(output_bin_path, std::ios::binary);
        if (!out_bin.is_open()) {
            std::cerr << "Error: Unable to open output binary file for writing: " << output_bin_path << "\n";
            le_compile_result_free(&result);
            return 1;
        }
        out_bin.write(reinterpret_cast<const char*>(result.binary_data), result.binary_size);
        out_bin.close();
        std::cout << "[OK] Compiled " << result.instruction_count << " instructions into "
                  << result.binary_size << " bytes -> " << output_bin_path << "\n";
    }

    if (!output_header_path.empty()) {
        std::ofstream out_hdr(output_header_path);
        if (!out_hdr.is_open()) {
            std::cerr << "Error: Unable to open output header file for writing: " << output_header_path << "\n";
            le_compile_result_free(&result);
            return 1;
        }
        out_hdr << (result.c_header_code ? result.c_header_code : "");
        out_hdr.close();
        std::cout << "[OK] Generated C embedded program header -> " << output_header_path << "\n";
    }

    if (!output_disasm_path.empty()) {
        std::ofstream out_dis(output_disasm_path);
        if (!out_dis.is_open()) {
            std::cerr << "Error: Unable to open output disassembly file for writing: " << output_disasm_path << "\n";
            le_compile_result_free(&result);
            return 1;
        }
        out_dis << (result.disassembly_text ? result.disassembly_text : "");
        out_dis.close();
        std::cout << "[OK] Generated disassembly listing -> " << output_disasm_path << "\n";
    }

    le_compile_result_free(&result);
    return 0;
}

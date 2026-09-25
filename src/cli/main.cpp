/**
 * @file main.cpp
 * @brief Unified LogicElements Host CLI entry point (`le`).
 */

#include "commands/cli_commands.h"
#include "le_compiler.h"
#include <iostream>
#include <string>
#include <cstring>

static void print_general_usage(const char* prog)
{
    std::cout << "LogicElements CLI Toolsuite v" << le_compiler_get_version() << "\n\n"
              << "Usage: " << prog << " <command> [arguments]\n\n"
              << "Available Commands:\n"
              << "  compile    Compile a circuit JSON into .lebin bytecode\n"
              << "  disasm     Disassemble a .lebin file into human-readable instructions\n"
              << "  board      Inspect board profiles, discover serial ports, or query hardware\n"
              << "  upload     Stream and flash a .lebin binary to microcontroller over serial\n"
              << "  control    Start, stop, reset runtime or force/pulse I/O registers\n"
              << "  monitor    Stream and inspect live process image (%IN, %OUT, %B) telemetry\n"
              << "  term       Open an interactive terminal session to target MCU shell\n"
              << "  sim        Run local desktop simulation of a .lebin program\n\n"
              << "Options:\n"
              << "  -v, --version  Show version information\n"
              << "  -h, --help     Show this help message\n\n"
              << "Run '" << prog << " <command> --help' for details on a specific command.\n";
}

static std::string get_base_name(const std::string& path)
{
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".exe") {
        filename = filename.substr(0, filename.size() - 4);
    }
    return filename;
}

int main(int argc, char* argv[])
{
    if (argc < 1) return 1;

    std::string exec_name = get_base_name(argv[0]);

    // Backwards-compatibility multi-call binary support
    if (exec_name == "le_compile") {
        return cmd_compile(argc, argv);
    } else if (exec_name == "le_sim") {
        return cmd_sim(argc, argv);
    }

    if (argc < 2) {
        print_general_usage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        print_general_usage(argv[0]);
        return 0;
    } else if (cmd == "-v" || cmd == "--version" || cmd == "version") {
        std::cout << "LogicElements v" << le_compiler_get_version() << "\n";
        return 0;
    }

    // Shift arguments so subcommand sees its own name as argv[0]
    int sub_argc = argc - 1;
    char** sub_argv = argv + 1;

    if (cmd == "compile") {
        return cmd_compile(sub_argc, sub_argv);
    } else if (cmd == "disasm") {
        return cmd_disasm(sub_argc, sub_argv);
    } else if (cmd == "board") {
        return cmd_board(sub_argc, sub_argv);
    } else if (cmd == "upload") {
        return cmd_upload(sub_argc, sub_argv);
    } else if (cmd == "control") {
        return cmd_control(sub_argc, sub_argv);
    } else if (cmd == "monitor") {
        return cmd_monitor(sub_argc, sub_argv);
    } else if (cmd == "term") {
        return cmd_term(sub_argc, sub_argv);
    } else if (cmd == "sim") {
        return cmd_sim(sub_argc, sub_argv);
    }

    std::cerr << "Unknown command: '" << cmd << "'\n\n";
    print_general_usage(argv[0]);
    return 1;
}

/**
 * @file cmd_compile.cpp
 * @brief Subcommand handler: `le compile`
 */

#include "cli_commands.h"
#include "le_compile_driver.h"

int cmd_compile(int argc, char* argv[]) {
    return LogicElements::le_compile_driver_main(argc, argv, "le compile");
}

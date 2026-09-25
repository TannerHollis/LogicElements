/**
 * @file cli_commands.h
 * @brief Subcommand prototypes for the unified LogicElements host CLI (`le`).
 */

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

int cmd_compile(int argc, char* argv[]);
int cmd_disasm(int argc, char* argv[]);
int cmd_board(int argc, char* argv[]);
int cmd_upload(int argc, char* argv[]);
int cmd_control(int argc, char* argv[]);
int cmd_monitor(int argc, char* argv[]);
int cmd_term(int argc, char* argv[]);
int cmd_sim(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif /* CLI_COMMANDS_H */

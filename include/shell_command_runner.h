#ifndef SHELL_COMMAND_RUNNER_H
#define SHELL_COMMAND_RUNNER_H

/**
 * @brief Execute a shell command, reporting failures.
 *
 * @param cmd  Null-terminated command string.
 * @return     system() return code.
 */
int run_command(const char *cmd);

#endif //SHELL_COMMAND_RUNNER_H

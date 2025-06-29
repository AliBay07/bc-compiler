#include "../include/shell_command_runner.h"

#include <stdio.h>
#include <stdlib.h>

int run_command(const char *cmd) {
    const int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
    }
    return status;
}
#include "flappy.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>

/* Command structure mapping command names to their handler functions */
struct command {
    const char *name;
    int (*handler)(void);
};

/* Table of available commands */
static const struct command commands[] = {
    { "help",    cmd_help    },
    { "version", cmd_version },
};

/*
 * handle_command - Execute a command by name
 * @cmd: The command name to execute
 *
 * Searches the commands table for a matching command and executes its handler.
 * If the command is not found, logs an error and returns 127.
 *
 * Returns: The handler's return value, or 127 if command not found.
 */
static int handle_command(const char *cmd) {
    for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            return commands[i].handler();
        }
    }

    fprintf(stderr, "Unknown command: %s\nTry 'flappy help'\n", cmd);
    log_error("unknown command: %s", cmd);
    return 127;
}

/*
 * cli_dispatch - Main CLI dispatcher for parsing arguments
 * @argc: Argument count
 * @argv: Argument vector
 *
 * Processes long-form command-line options and dispatches to appropriate
 * handlers. If no options are provided, displays help. Command arguments
 * are passed to handle_command for execution.
 *
 * Returns: Handler return value, 2 on invalid option, or help return value.
 */
int cli_dispatch(int argc, char **argv) {
    static struct option long_opts[] = {
        { "help",    no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            return cmd_help();
        case 'v':
            return cmd_version();
        default:
            fprintf(stderr, "Invalid option\nTry 'flappy help'\n");
            log_error("invalid CLI option");
            return 2;
        }
    }

    if (optind >= argc) {
        return cmd_help();
    }

    return handle_command(argv[optind]);
}

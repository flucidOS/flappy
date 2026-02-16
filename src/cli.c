#include "flappy.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* =====================
 * Command Dispatch Table
 * ===================== */

struct command {
    const char *name;
    int min_args;
    int (*handler)(int argc, char **argv);
};

static const struct command commands[] = {
    { "help",    0, cmd_help    },
    { "version", 0, cmd_version },
    { "list",    0, cmd_list    },
    { "info",    1, cmd_info    },
    { "files",   1, cmd_files   },
    { "owns",    1, cmd_owns    },
};

/* =====================
 * Command Resolution
 * ===================== */

static int handle_command(const char *cmd, int argc, char **argv) {
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            if (argc < commands[i].min_args) {
                fprintf(stderr,
                        "Error: missing arguments for '%s'\n",
                        cmd);
                return 2;
            }
            return commands[i].handler(argc, argv);
        }
    }

    fprintf(stderr, "Unknown command: %s\nTry 'flappy help'\n", cmd);
    log_error("unknown command: %s", cmd);
    return 127;
}

/* =====================
 * CLI Entry Point
 * ===================== */

int cli_dispatch(int argc, char **argv) {
    static struct option long_opts[] = {
        { "help",    no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "init-db", no_argument, 0, 1001 },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            return cmd_help(0, NULL);

        case 'v':
            return cmd_version(0, NULL);

        case 1001:
            return db_bootstrap_install();

        default:
            fprintf(stderr, "Invalid option\nTry 'flappy help'\n");
            log_error("invalid CLI option");
            return 2;
        }
    }

    if (optind >= argc) {
        return cmd_help(0, NULL);
    }

    return handle_command(
        argv[optind],
        argc - optind - 1,
        &argv[optind + 1]
    );
}

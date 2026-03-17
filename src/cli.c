/*
 * cli.c - Command line dispatcher for Flappy
 *
 * getopt_long only processes global flags (--help, --version, --init-db).
 * Everything after the command name is passed raw to the handler.
 * POSIXLY_CORRECT behaviour: stop at first non-option argument.
 */

#include "flappy.h"
#include "repo.h"

#include <getopt.h>
#include <string.h>
#include <stdio.h>

int cmd_install(int argc, char **argv);

int cmd_update(int argc, char **argv)
{
    const char *url = FLAPPY_DEFAULT_REPO_URL;
    if (argc >= 1 && argv[0] && strlen(argv[0]) > 0)
        url = argv[0];
    return repo_update(url);
}

int cmd_search(int argc, char **argv)
{
    const char *term = NULL;
    if (argc > 0)
        term = argv[0];
    return repo_search(term);
}

int cmd_upgrade(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return repo_upgrade();
}

struct command {
    const char *name;
    int min_args;
    int (*handler)(int argc, char **argv);
};

static const struct command commands[] = {
    { "help",       0, cmd_help       },
    { "version",    0, cmd_version    },
    { "list",       0, cmd_list       },
    { "info",       1, cmd_info       },
    { "files",      1, cmd_files      },
    { "owns",       1, cmd_owns       },
    { "inspect",    1, cmd_inspect    },
    { "depends",    1, cmd_depends    },
    { "rdepends",   1, cmd_rdepends   },
    { "orphans",    0, cmd_orphans    },
    { "update",     0, cmd_update     },
    { "search",     0, cmd_search     },
    { "upgrade",    0, cmd_upgrade    },
    { "install",    1, cmd_install    },
    { "remove",     1, cmd_remove     },
    { "purge",      1, cmd_purge      },
    { "autoremove", 0, cmd_autoremove },
    { "verify",     0, cmd_verify     },
    { "clean",      0, cmd_clean      },
};

static int handle_command(const char *cmd, int argc, char **argv)
{
    size_t n = sizeof(commands) / sizeof(commands[0]);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            if (argc < commands[i].min_args) {
                fprintf(stderr, "Error: missing arguments for '%s'\n", cmd);
                return 2;
            }
            return commands[i].handler(argc, argv);
        }
    }
    fprintf(stderr, "Unknown command: %s\nTry 'flappy help'\n", cmd);
    log_error("unknown command: %s", cmd);
    return 127;
}

int cli_dispatch(int argc, char **argv)
{
    /*
     * Only process global long options that appear BEFORE the command.
     * Use "+" prefix on optstring to stop at first non-option argument —
     * this prevents getopt from consuming --all, --force etc. that
     * belong to subcommands.
     */
    static struct option long_opts[] = {
        { "help",    no_argument, 0, 'h'  },
        { "version", no_argument, 0, 'v'  },
        { "init-db", no_argument, 0, 1001 },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "+", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':   return cmd_help(0, NULL);
        case 'v':   return cmd_version(0, NULL);
        case 1001:  return db_bootstrap_install();
        default:
            fprintf(stderr, "Invalid option\nTry 'flappy help'\n");
            log_error("invalid CLI option");
            return 2;
        }
    }

    if (optind >= argc)
        return cmd_help(0, NULL);

    return handle_command(argv[optind], argc - optind - 1, &argv[optind + 1]);
}
/**
 * cmd_help - Display help information for the Flappy package manager
 *
 * This function outputs a comprehensive help message to stdout, including
 * usage instructions and available commands for the Flappy package manager.
 * It provides users with quick reference information about the tool's basic
 * functionality and command-line interface.
 *
 * Return: Always returns 0 to indicate successful execution.
 *
 * See also: cmd_version()
 */
#include "flappy.h"
#include <stdio.h>

/*
 * cmd_help - Display help information for Flappy
 *
 * Lists all available commands across:
 *   - Core commands
 *   - Trail-4 graph commands
 *   - Trail-5 repository commands
 */

int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf(
        "Flappy - Package manager for FlucidOS\n\n"
        "Usage:\n"
        "  flappy <command> [args]\n\n"

        "Core Commands:\n"
        "  help                  Show this help message\n"
        "  version               Show version information\n"
        "  --init-db             Initialize installed database\n\n"

        "  list                  List installed packages\n"
        "  info <pkg>            Show package info\n"
        "  files <pkg>           List package files\n"
        "  owns <path>           Show which package owns a file\n"
        "  inspect <pkg>         Inspect package metadata\n"
        "  depends <pkg>         Show direct dependencies\n"
        "  rdepends <pkg>        Show reverse dependencies\n"
        "  orphans               List unused dependency packages\n\n"
        
        "  update                Update repository metadata (root required)\n"
        "  search [term]         Search repository packages\n"
        "  upgrade               Show available upgrades (dry-run)\n\n"
    );

    return 0;
}



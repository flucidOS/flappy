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

int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf(
        "Flappy - Package manager for FlucidOS\n\n"
        "Usage:\n"
        "  flappy <command> [args]\n\n"
        "Commands:\n"
        "  help\n"
        "  version\n"
        "  list\n"
        "  info <pkg>\n"
        "  files <pkg>\n"
        "  owns <path>\n"
    );
    return 0;
}



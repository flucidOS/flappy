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

int cmd_help(void) {
    printf(
        "Flappy - Package manager for FlucidOS\n\n"
        "Usage:\n"
        "  flappy <command> [options]\n\n"
        "Commands:\n"
        "  help        Show this help message\n"
        "  version     Show Flappy version\n"
    );
    return 0;
}

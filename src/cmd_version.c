/**
 * cmd_version - Display the application version and tagline
 *
 * Print the current version of Flappy along with its tagline to stdout.
 *
 * Return: 0 on success
 */
#include "flappy.h"
#include <stdio.h>

int cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("Flappy %s\n%s\n", FLAPPY_VERSION, FLAPPY_TAGLINE);
    return 0;
}



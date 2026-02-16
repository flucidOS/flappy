#include "flappy.h"

/**
 * cmd_version - Display the application version and tagline
 *
 * Print the current version of Flappy along with its tagline to stdout.
 *
 * Return: 0 on success
 */
int cmd_version(void) {
    printf("Flappy %s\n%s\n", FLAPPY_VERSION, FLAPPY_TAGLINE);
    return 0;
}

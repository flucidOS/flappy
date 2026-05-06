/*
 * cmd_autoremove.c - CLI handler for `flappy autoremove`
 *
 * Removes all orphaned dependency packages:
 *   - not explicitly installed
 *   - no reverse dependencies
 *
 * Uses safe removal semantics (keeps /etc configs).
 *
 * Usage:
 *   flappy autoremove
 */

#include "flappy.h"
#include "remove.h"

#include <stdio.h>
#include <unistd.h>

int cmd_autoremove(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (geteuid() != 0) {
        fprintf(stderr, "Error: flappy autoremove requires root privileges\n");
        return 1;
    }

    db_open_or_die();
    int rc = autoremove_packages();
    db_close();

    return rc;
}

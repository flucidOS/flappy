/*
 * cmd_remove.c - CLI handler for `flappy remove`
 *
 * Safe removal: removes files, keeps /etc configs.
 * Refuses if reverse dependencies exist.
 *
 * Usage:
 *   flappy remove <package>
 */

#include "flappy.h"
#include "remove.h"

#include <stdio.h>

int cmd_remove(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: flappy remove <package>\n");
        return 2;
    }

    db_open_or_die();
    int rc = remove_package(argv[0]);
    db_close();

    return rc;
}

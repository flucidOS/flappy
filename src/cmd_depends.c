/*
 * cmd_depends.c - CLI handler for `flappy depends`
 *
 * Prints direct dependencies of an installed package.
 *
 * Behavior:
 *   - Fails if package does not exist
 *   - Prints nothing if package has no dependencies
 *   - Output sorted deterministically (handled by graph layer)
 */

#include "flappy.h"
#include "graph.h"
#include "db_guard.h"

#include <stdio.h>

int cmd_depends(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "depends requires a package name\n");
        return 2;
    }

    db_open_or_die();

    int rc = graph_depends(argv[0]);

    db_close();

    return rc;
}
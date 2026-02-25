/*
 * cmd_rdepends.c - CLI handler for `flappy rdepends`
 *
 * Prints direct reverse dependencies of an installed package.
 *
 * Behavior:
 *   - Fails if package does not exist
 *   - Prints nothing if no reverse dependencies
 *   - Output sorted deterministically (handled by graph layer)
 */

#include "flappy.h"
#include "graph.h"
#include "db_guard.h"

#include <stdio.h>

int cmd_rdepends(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "rdepends requires a package name\n");
        return 2;
    }

    db_open_or_die();

    int rc = graph_rdepends(argv[0]);

    db_close();

    return rc;
}
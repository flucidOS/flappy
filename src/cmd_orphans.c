/*
 * cmd_orphans.c - CLI handler for `flappy orphans`
 *
 * Prints installed packages that:
 *   - Are not explicit
 *   - Have no reverse dependencies
 *
 * Behavior:
 *   - Empty output is valid (exit 0)
 *   - Output sorted deterministically (handled by graph layer)
 */

#include "flappy.h"
#include "graph.h"
#include "db_guard.h"

int cmd_orphans(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    db_open_or_die();

    int rc = graph_orphans();

    db_close();

    return rc;
}
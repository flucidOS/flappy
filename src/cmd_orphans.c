#include "flappy.h"
#include "graph.h"
#include "ui.h"

#include <stdio.h>

/*
 * cmd_orphans.c - CLI handler for `flappy orphans`
 *
 * Previously duplicated the orphan query SQL from graph.c.
 * Now delegates to graph_orphans() so the definition lives in one place.
 *
 * graph_orphans() returns:
 *   -1   DB error (db_die will have terminated, but checked defensively)
 *    0   success, no orphans found
 *   >0   success, N orphans printed
 */
int cmd_orphans(int argc, char **argv)
{
    (void)argc; (void)argv;

    db_open_or_die();

    int rc = graph_orphans();

    if (rc == 0)
        ui_info("no orphan packages");

    db_close();
    return (rc < 0) ? 1 : 0;
}
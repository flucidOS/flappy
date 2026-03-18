#include "flappy.h"
#include "graph.h"
#include "db_guard.h"
#include "ui.h"
#include <sqlite3.h>
#include <stdio.h>

int cmd_orphans(int argc, char **argv)
{
    (void)argc; (void)argv;

    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT p.name "
        "FROM packages p "
        "LEFT JOIN dependencies d ON d.depends_on = p.id "
        "WHERE p.explicit = 0 "
        "GROUP BY p.id "
        "HAVING COUNT(d.package_id) = 0 "
        "ORDER BY p.name COLLATE BINARY ASC;",
        -1, &st, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "orphans prepare");

    int found = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s\n", sqlite3_column_text(st, 0));
        found = 1;
    }
    sqlite3_finalize(st);

    if (!found)
        ui_info("no orphan packages");

    db_close();
    return 0;
}
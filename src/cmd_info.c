#include "flappy.h"
#include "db_guard.h"
#include "ui.h"
#include <sqlite3.h>
#include <stdio.h>

int cmd_info(int argc, char **argv) {
    if (argc < 1) {
        ui_error("usage: flappy info <package>");
        return 2;
    }

    const char *pkg = argv[0];
    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT name, version, explicit FROM packages WHERE name = ?;",
        -1, &st, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "info prepare");

    sqlite3_bind_text(st, 1, pkg, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        ui_error("package '%s' is not installed", pkg);
        sqlite3_finalize(st);
        db_close();
        return 1;
    }

    printf("%-12s: %s\n", "Name",         sqlite3_column_text(st, 0));
    printf("%-12s: %s\n", "Version",      sqlite3_column_text(st, 1));
    printf("%-12s: %s\n", "Installed",
           sqlite3_column_int(st, 2) ? "explicit" : "dependency");

    sqlite3_finalize(st);
    db_close();
    return 0;
}
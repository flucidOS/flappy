#include "flappy.h"
#include "db_guard.h"
#include "ui.h"
#include <sqlite3.h>
#include <stdio.h>

int cmd_owns(int argc, char **argv) {
    if (argc < 1) {
        ui_error("usage: flappy owns <path>");
        return 2;
    }

    const char *path = argv[0];
    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT p.name FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "WHERE f.path = ?;",
        -1, &st, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "owns prepare");

    sqlite3_bind_text(st, 1, path, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        ui_error("no package owns: %s", path);
        sqlite3_finalize(st);
        db_close();
        return 1;
    }

    printf("%s\n", sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    db_close();
    return 0;
}
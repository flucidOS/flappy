/**
 * cmd_owns - Find the package that owns a given file
 * @path: The file path to query
 *
 * Searches the database for the package that owns the specified file path.
 * Opens a database connection, queries the files table joined with packages,
 * and prints the owning package name if found.
 *
 * Returns: 0 on success (package found and printed), 1 if file is not owned
 *          by any package or on database error.
 *
 * Note: Prints error message to stderr if file is not found in database.
 *       Database connection is properly closed on both success and failure.
 */
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>


int cmd_owns(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "owns requires a file path\n");
        return 2;
    }

    const char *path = argv[0];

    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p.name FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "WHERE f.path = ?;",
        -1,
        &st,
        NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "owns prepare");

    sqlite3_bind_text(st, 1, path, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        fprintf(stderr, "File not owned: %s\n", path);
        sqlite3_finalize(st);
        db_close();
        return 1;
    }

    printf("%s\n", sqlite3_column_text(st, 0));

    sqlite3_finalize(st);
    db_close();
    return 0;
}


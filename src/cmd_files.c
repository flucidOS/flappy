/**
 * cmd_files - Retrieve and display all files associated with a package
 * @pkg: The name of the package to query
 *
 * Queries the database for all files belonging to the specified package
 * and prints each file path to stdout, one per line. The results are
 * ordered alphabetically by file path.
 *
 * Opens a database connection, executes a JOIN query to fetch files from
 * the packages table, and closes the connection upon completion. If no
 * files are found for the given package, an error message is printed to
 * stderr.
 *
 * Return: 0 if one or more files were found and displayed, 1 if no files
 *         were found or an error occurred
 */
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>

int cmd_files(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "files requires a package name\n");
        return 2;
    }

    const char *pkg = argv[0];

    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT f.path FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "WHERE p.name = ? "
        "ORDER BY f.path;",
        -1,
        &st,
        NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "files prepare");

    sqlite3_bind_text(st, 1, pkg, -1, SQLITE_STATIC);

    int found = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s\n", sqlite3_column_text(st, 0));
        found = 1;
    }

    if (!found)
        fprintf(stderr, "No files recorded for '%s'\n", pkg);

    sqlite3_finalize(st);
    db_close();
    return found ? 0 : 1;
}

/**
 * cmd_info - Display information about an installed package
 * @pkg: The name of the package to query
 *
 * Retrieves and displays metadata for an installed package from the database,
 * including its name, version, and installation type (explicit or dependency).
 *
 * Opens a database connection, prepares a SELECT query to fetch package details,
 * and prints the results to standard output. If the package is not found in the
 * database, an error message is written to stderr.
 *
 * Return: 0 on successful retrieval and display of package information,
 *         1 if the package is not installed or not found in the database
 *
 * Note: Database connection is opened and closed within this function.
 *       The package name should be a valid null-terminated string.
 */
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>

int cmd_info(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "info requires a package name\n");
        return 2;
    }

    const char *pkg = argv[0];

    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT name, version, explicit FROM packages WHERE name = ?;",
        -1,
        &st,
        NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "info prepare");

    sqlite3_bind_text(st, 1, pkg, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        fprintf(stderr, "Package '%s' is not installed\n", pkg);
        sqlite3_finalize(st);
        db_close();
        return 1;
    }

    printf("Name: %s\nVersion: %s\nInstalled: %s\n",
           sqlite3_column_text(st, 0),
           sqlite3_column_text(st, 1),
           sqlite3_column_int(st, 2) ? "explicit" : "dependency");

    sqlite3_finalize(st);
    db_close();
    return 0;
}



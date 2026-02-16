/**
 * cmd_list - Display all packages from the database
 *
 * Lists all packages stored in the database, sorted alphabetically by name.
 * Retrieves and prints the package name and version for each entry.
 *
 * The function handles database connection lifecycle, preparing and executing
 * a SELECT query, then iterating through results to display formatted output.
 *
 * Return: 0 on successful completion
 *
 * Note: Terminates the program if database connection fails via db_open_or_die()
 */
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>

int cmd_list(int argc, char **argv) {
    (void)argc; (void)argv;

    db_open_or_die();
    sqlite3 *db = db_handle();

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT name, version FROM packages ORDER BY name;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "list prepare");

    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s %s\n",
               sqlite3_column_text(st, 0),
               sqlite3_column_text(st, 1));
    }

    sqlite3_finalize(st);
    db_close();
    return 0;
}


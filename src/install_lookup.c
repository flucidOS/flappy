/*
 * install_lookup.c - Query repo database
 */

#include "flappy.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define REPO_DB "/var/lib/flappy/repo.db"

int install_lookup(const char *pkg,
                   char *filename,
                   char *checksum)
{
    sqlite3 *db;
    sqlite3_stmt *st;

    if (sqlite3_open_v2(REPO_DB, &db,
        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "cannot open repo database\n");
        return 1;
    }

    const char *sql =
        "SELECT filename, checksum "
        "FROM repo_packages "
        "WHERE name = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return 1;

    sqlite3_bind_text(st, 1, pkg, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        fprintf(stderr, "package not found: %s\n", pkg);
        return 1;
    }

    strcpy(filename,
        (const char*)sqlite3_column_text(st, 0));

    strcpy(checksum,
        (const char*)sqlite3_column_text(st, 1));

    sqlite3_finalize(st);
    sqlite3_close(db);

    return 0;
}
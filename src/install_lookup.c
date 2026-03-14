/*
 * install_lookup.c - Query repo database for package metadata
 *
 * Looks up filename and checksum from repo.db.
 * Table is "packages" (not "repo_packages").
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
        "FROM packages "
        "WHERE name = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "lookup: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sqlite3_bind_text(st, 1, pkg, -1, SQLITE_STATIC);

    if (sqlite3_step(st) != SQLITE_ROW) {
        fprintf(stderr, "package not found in repo: %s\n", pkg);
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 1;
    }

    const char *f = (const char *)sqlite3_column_text(st, 0);
    const char *c = (const char *)sqlite3_column_text(st, 1);

    if (!f || !c) {
        fprintf(stderr, "lookup: null filename or checksum for %s\n", pkg);
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 1;
    }

    strncpy(filename, f, 255);  filename[255] = '\0';
    strncpy(checksum, c, 127);  checksum[127] = '\0';

    sqlite3_finalize(st);
    sqlite3_close(db);

    log_info("lookup: %s -> %s", pkg, filename);
    return 0;
}
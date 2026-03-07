/*
 * install_commit.c - Register package installation
 */

#include "flappy.h"

#include <sqlite3.h>

int install_commit(const char *pkgname,
                   const char *staging)
{
    sqlite3 *db = db_handle();

    sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);

    /* TODO: insert package record */

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    (void)pkgname;
    (void)staging;

    return 0;
}
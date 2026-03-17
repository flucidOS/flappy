/*
 * verify.c - System integrity verification
 *
 * Checks installed package state against the filesystem.
 *
 * Rules:
 *   - Every file recorded in the DB must exist on disk
 *   - Every recorded file must be a regular file (not a directory)
 *   - Every package must have at least one file registered
 *
 * Output format:
 *   missing: /usr/bin/curl (owned by curl)
 *   invalid: /usr/lib/libssl.so (expected file, found directory)
 *   empty:   curl (no files registered)
 *
 * Exit codes:
 *   0 - system consistent
 *   1 - inconsistencies found
 *
 * NEVER mutates anything. Read-only.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>

#include <stdio.h>
#include <sys/stat.h>

int verify_system(void)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    int issues = 0;

    /* ---------------------------------------------------------------
     * Check 1: every file in DB exists and is a regular file
     * ------------------------------------------------------------- */
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT f.path, p.name "
        "FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "ORDER BY p.name COLLATE BINARY ASC, f.path ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "verify files prepare");

    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *path    = (const char *)sqlite3_column_text(st, 0);
        const char *pkgname = (const char *)sqlite3_column_text(st, 1);

        if (!path || !pkgname)
            continue;

        struct stat s;

        if (stat(path, &s) != 0) {
            printf("missing: %s (owned by %s)\n", path, pkgname);
            issues++;
            continue;
        }

        if (!S_ISREG(s.st_mode)) {
            printf("invalid: %s (expected file, found directory)"
                   " (owned by %s)\n", path, pkgname);
            issues++;
        }
    }

    sqlite3_finalize(st);

    /* ---------------------------------------------------------------
     * Check 2: every package has at least one file
     * ------------------------------------------------------------- */
    rc = sqlite3_prepare_v2(
        db,
        "SELECT p.name "
        "FROM packages p "
        "LEFT JOIN files f ON f.package_id = p.id "
        "GROUP BY p.id "
        "HAVING COUNT(f.path) = 0 "
        "ORDER BY p.name COLLATE BINARY ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "verify empty packages prepare");

    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 0);
        if (!name) continue;
        printf("empty:   %s (no files registered)\n", name);
        issues++;
    }

    sqlite3_finalize(st);

    /* ---------------------------------------------------------------
     * Summary
     * ------------------------------------------------------------- */
    if (issues == 0) {
        printf("system OK\n");
        return 0;
    }

    printf("verification failed (%d issue%s found)\n",
           issues, issues == 1 ? "" : "s");
    return 1;
}
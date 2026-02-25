/*
 * graph.c - Deterministic installed dependency graph engine
 *
 * Trail-4 Scope:
 *   - Direct dependency query
 *   - Reverse dependency query
 *   - Orphan detection
 *   - Deterministic ordering
 *   - Strict exit semantics
 *
 * This module does NOT implement:
 *   - Install logic
 *   - Removal logic
 *   - Resolver logic
 *
 * All read queries:
 *   - Run inside a snapshot transaction
 *   - Perform explicit existence check
 *   - Use BINARY collation for ordering
 */

#define _POSIX_C_SOURCE 200809L

#include "graph.h"
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* =========================================================================
 * Identifier Handling
 * ========================================================================= */

/*
 * normalize_lower
 *
 * Convert identifier to canonical lowercase in-place.
 * This guarantees consistent lookup semantics.
 */
static void normalize_lower(char *s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

/* =========================================================================
 * Transaction Helpers
 * ========================================================================= */

/*
 * begin_read_tx
 *
 * Start a deferred read transaction.
 * Provides snapshot isolation.
 */
static void begin_read_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "begin");
}

/*
 * commit_tx
 *
 * Commit active transaction.
 */
static void commit_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "commit");
}

/* =========================================================================
 * Existence Check
 * ========================================================================= */

/*
 * package_exists
 *
 * Returns:
 *   1 if package exists
 *   0 if not
 *
 * Must be called inside active transaction.
 */
static int package_exists(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT id FROM packages WHERE name = ?;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "exists prepare");

    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    rc = sqlite3_step(st);

    sqlite3_finalize(st);

    return (rc == SQLITE_ROW);
}

/* =========================================================================
 * Direct Dependencies
 * ========================================================================= */

int graph_depends(const char *name)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    char *canon = strdup(name);
    if (!canon)
        return 1;

    normalize_lower(canon);

    begin_read_tx(db);

    /* Strict existence check */
    if (!package_exists(db, canon)) {
        commit_tx(db);
        fprintf(stderr, "Package '%s' is not installed\n", name);
        free(canon);
        return 1;
    }

    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d ON p1.id = d.package_id "
        "JOIN packages p2 ON p2.id = d.depends_on "
        "WHERE p1.name = ? "
        "ORDER BY p2.name COLLATE BINARY ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "depends prepare");

    sqlite3_bind_text(st, 1, canon, -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const unsigned char *dep =
            sqlite3_column_text(st, 0);
        printf("%s\n", dep);
    }

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);

    return 0;
}

/* =========================================================================
 * Reverse Dependencies
 * ========================================================================= */

int graph_rdepends(const char *name)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    char *canon = strdup(name);
    if (!canon)
        return 1;

    normalize_lower(canon);

    begin_read_tx(db);

    /* Strict existence check */
    if (!package_exists(db, canon)) {
        commit_tx(db);
        fprintf(stderr, "Package '%s' is not installed\n", name);
        free(canon);
        return 1;
    }

    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d ON p1.id = d.depends_on "
        "JOIN packages p2 ON p2.id = d.package_id "
        "WHERE p1.name = ? "
        "ORDER BY p2.name COLLATE BINARY ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "rdepends prepare");

    sqlite3_bind_text(st, 1, canon, -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const unsigned char *dep =
            sqlite3_column_text(st, 0);
        printf("%s\n", dep);
    }

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);

    return 0;
}

/* =========================================================================
 * Orphan Detection
 * ========================================================================= */

int graph_orphans(void)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    begin_read_tx(db);

    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p.name "
        "FROM packages p "
        "LEFT JOIN dependencies d ON d.depends_on = p.id "
        "WHERE p.explicit = 0 "
        "GROUP BY p.id "
        "HAVING COUNT(d.package_id) = 0 "
        "ORDER BY p.name COLLATE BINARY ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "orphans prepare");

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const unsigned char *name =
            sqlite3_column_text(st, 0);
        printf("%s\n", name);
    }

    sqlite3_finalize(st);
    commit_tx(db);

    return 0;
}

/* =========================================================================
 * Install Stub (Locked for Trail-4)
 * ========================================================================= */

int graph_add_package(
    const char *name,
    const char *version,
    int explicit_flag,
    const char **depends,
    size_t depends_count)
{
    (void)name;
    (void)version;
    (void)explicit_flag;
    (void)depends;
    (void)depends_count;

    fprintf(stderr,
        "graph_add_package not available in Trail-4\n");

    return 1;
}
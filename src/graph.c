/*
 * graph.c - Installed dependency graph engine
 *
 * Implements deterministic direct graph queries over installed packages.
 *
 * Design guarantees:
 *   - Snapshot isolation for read queries
 *   - Canonical lowercase identifiers
 *   - Strict ASCII identifier policy
 *   - Alphabetical ordering (COLLATE BINARY)
 *   - Fail-fast on structural violations
 *
 * This module intentionally does NOT implement:
 *   - Transitive traversal
 *   - Cycle detection (install-time responsibility)
 *   - Automatic dependency resolution
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
 * Identifier Policy
 * ========================================================================= */

/*
 * normalize_lower
 *
 * Converts identifier to canonical lowercase in-place.
 */
static void normalize_lower(char *s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

/*
 * valid_pkg_name
 *
 * Enforces strict ASCII identifier policy:
 *
 *   [a-z0-9][a-z0-9+.-]*
 *
 * Rejects:
 *   - Uppercase
 *   - Unicode
 *   - Spaces
 *   - Underscore
 *   - Slash
 */
static int valid_pkg_name(const char *s)
{
    if (!s || !*s)
        return 0;

    if (!(islower((unsigned char)s[0]) ||
          isdigit((unsigned char)s[0])))
        return 0;

    for (size_t i = 0; s[i]; i++) {
        char c = s[i];
        if (!(islower((unsigned char)c) ||
              isdigit((unsigned char)c) ||
              c == '-' || c == '.' || c == '+'))
            return 0;
    }

    return 1;
}

/* =========================================================================
 * Transaction Helpers
 * ========================================================================= */

static void begin_read_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "begin");
}

static void commit_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "commit");
}

/* =========================================================================
 * Existence Check
 * ========================================================================= */

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

    if (!valid_pkg_name(canon)) {
        fprintf(stderr, "Invalid package name: %s\n", name);
        free(canon);
        return 1;
    }

    begin_read_tx(db);

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

    if (!valid_pkg_name(canon)) {
        fprintf(stderr, "Invalid package name: %s\n", name);
        free(canon);
        return 1;
    }

    begin_read_tx(db);

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
 * Orphans
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
        const unsigned char *pkg =
            sqlite3_column_text(st, 0);
        printf("%s\n", pkg);
    }

    sqlite3_finalize(st);
    commit_tx(db);

    return 0;
}

/* =========================================================================
 * Install Skeleton (Future Trail)
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

    /*
     * Install logic intentionally not implemented.
     * Trail-4 focuses on read-side deterministic graph behavior.
     */

    fprintf(stderr, "graph_add_package not implemented\n");
    return 1;
}
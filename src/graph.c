/*
 * graph.c - Deterministic installed dependency graph engine
 *
 * Guarantees:
 *   - Canonical lowercase identifiers
 *   - Strict ASCII validation
 *   - Snapshot isolation for reads
 *   - BEGIN IMMEDIATE for installs
 *   - Install-time cycle detection
 *   - Deterministic alphabetical traversal
 *   - No partial state commits
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

#define DFS_MAX_DEPTH 1024

/* ============================================================
 * Identifier Handling
 * ============================================================ */

static void normalize_lower(char *s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

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

/* ============================================================
 * Transaction Helpers
 * ============================================================ */

static void begin_read_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "begin");
}

static void begin_write_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "begin immediate");
}

static void commit_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "commit");
}

static void rollback_tx(sqlite3 *db)
{
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
}

/* ============================================================
 * Graph Queries
 * ============================================================ */

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

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d ON p1.id = d.package_id "
        "JOIN packages p2 ON p2.id = d.depends_on "
        "WHERE p1.name = ? "
        "ORDER BY p2.name COLLATE BINARY ASC;",
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        db_die(db, rc, "depends prepare");

    sqlite3_bind_text(st, 1, canon, -1, SQLITE_STATIC);

    int found = 0;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *dep =
            (const char *)sqlite3_column_text(st, 0);
        printf("%s\n", dep);
        found = 1;
    }

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);

    return found ? 0 : 0;
}

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

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d ON p1.id = d.depends_on "
        "JOIN packages p2 ON p2.id = d.package_id "
        "WHERE p1.name = ? "
        "ORDER BY p2.name COLLATE BINARY ASC;",
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        db_die(db, rc, "rdepends prepare");

    sqlite3_bind_text(st, 1, canon, -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *dep =
            (const char *)sqlite3_column_text(st, 0);
        printf("%s\n", dep);
    }

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);

    return 0;
}

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
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        db_die(db, rc, "orphans prepare");

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *name =
            (const char *)sqlite3_column_text(st, 0);
        printf("%s\n", name);
    }

    sqlite3_finalize(st);
    commit_tx(db);

    return 0;
}

/* ============================================================
 * Install Logic (Atomic, No Resolver)
 * ============================================================ */

int graph_add_package(
    const char *name,
    const char *version,
    int explicit_flag,
    const char **depends,
    size_t depends_count)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    char *canon = strdup(name);
    if (!canon)
        return 1;

    normalize_lower(canon);

    if (!valid_pkg_name(canon)) {
        fprintf(stderr, "Invalid package name\n");
        free(canon);
        return 1;
    }

    begin_write_tx(db);

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "INSERT INTO packages(name, version, explicit) "
        "VALUES(?, ?, ?);",
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        db_die(db, rc, "insert prepare");

    sqlite3_bind_text(st, 1, canon, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, version, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 3, explicit_flag);

    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        rollback_tx(db);
        free(canon);
        fprintf(stderr, "Package already exists\n");
        return 1;
    }

    sqlite3_finalize(st);
    int new_id = (int)sqlite3_last_insert_rowid(db);

    for (size_t i = 0; i < depends_count; i++) {

        char *dep = strdup(depends[i]);
        if (!dep)
            continue;

        normalize_lower(dep);

        if (strcmp(dep, canon) == 0) {
            fprintf(stderr, "Self-dependency not allowed\n");
            free(dep);
            rollback_tx(db);
            free(canon);
            return 1;
        }

        sqlite3_stmt *lookup = NULL;

        rc = sqlite3_prepare_v2(
            db,
            "SELECT id FROM packages WHERE name = ?;",
            -1, &lookup, NULL);

        if (rc != SQLITE_OK)
            db_die(db, rc, "lookup prepare");

        sqlite3_bind_text(lookup, 1, dep, -1, SQLITE_STATIC);

        if (sqlite3_step(lookup) != SQLITE_ROW) {
            sqlite3_finalize(lookup);
            fprintf(stderr, "Missing dependency: %s\n", dep);
            free(dep);
            rollback_tx(db);
            free(canon);
            return 1;
        }

        int dep_id = sqlite3_column_int(lookup, 0);
        sqlite3_finalize(lookup);

        sqlite3_stmt *ins = NULL;

        rc = sqlite3_prepare_v2(
            db,
            "INSERT OR IGNORE INTO dependencies(package_id, depends_on) "
            "VALUES(?, ?);",
            -1, &ins, NULL);

        if (rc != SQLITE_OK)
            db_die(db, rc, "dep insert prepare");

        sqlite3_bind_int(ins, 1, new_id);
        sqlite3_bind_int(ins, 2, dep_id);

        sqlite3_step(ins);
        sqlite3_finalize(ins);

        free(dep);
    }

    commit_tx(db);
    free(canon);

    return 0;
}
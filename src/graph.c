/*
 * graph.c - Deterministic installed dependency graph engine
 *
 * TRANSACTION CONTRACT (important):
 *
 *   graph_add_package does NOT open or commit its own transaction.
 *   The caller (install_commit) is responsible for wrapping the
 *   entire install — package row + deps + file registration + file
 *   copy — in a single BEGIN IMMEDIATE … COMMIT.
 *
 *   All other public functions (graph_depends, graph_rdepends,
 *   graph_orphans) open and close their own read transactions.
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

static void normalize_lower(char *s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

/* =========================================================================
 * Transaction Helpers (used only by read-query functions)
 *
 * Read-only queries use BEGIN DEFERRED, which is SQLite's default but
 * is stated explicitly here so the intent is clear and a future
 * maintainer cannot confuse it with BEGIN IMMEDIATE (used by writes).
 *
 * BEGIN DEFERRED acquires no lock until the first read, and never
 * escalates to a write lock, which is exactly what we want for
 * graph_depends, graph_rdepends, and graph_orphans.
 * ========================================================================= */

static void begin_deferred_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "BEGIN DEFERRED;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "begin deferred");
}

static void commit_tx(sqlite3 *db)
{
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "commit");
}

/* =========================================================================
 * Existence / ID lookup
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

/*
 * get_package_id
 *
 * Returns package rowid, or -1 if not found.
 */
static sqlite3_int64 get_package_id(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT id FROM packages WHERE name = ?;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "get_id prepare");

    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    sqlite3_int64 id = -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        id = sqlite3_column_int64(st, 0);

    sqlite3_finalize(st);
    return id;
}

/* =========================================================================
 * Cycle Detection (DFS)
 *
 * Called inside the caller's open transaction so it sees the
 * tentative package + dependency rows before they are committed.
 * ========================================================================= */

#define MAX_VISITED 1024

typedef struct {
    sqlite3_int64 ids[MAX_VISITED];
    int           count;
} VisitedSet;

static int visited_contains(const VisitedSet *v, sqlite3_int64 id)
{
    for (int i = 0; i < v->count; i++)
        if (v->ids[i] == id)
            return 1;
    return 0;
}

static int visited_add(VisitedSet *v, sqlite3_int64 id)
{
    if (v->count >= MAX_VISITED)
        return -1; /* overflow */
    v->ids[v->count++] = id;
    return 0;
}

static int dfs_has_cycle(sqlite3 *db,
                         sqlite3_int64 start,
                         sqlite3_int64 target,
                         VisitedSet *visited)
{
    if (visited_contains(visited, start))
        return 0;

    if (visited_add(visited, start) != 0) {
        log_error("cycle detection: visited set overflow");
        return 1;
    }

    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT depends_on FROM dependencies WHERE package_id = ?;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "dfs prepare");

    sqlite3_bind_int64(st, 1, start);

    int cycle = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        sqlite3_int64 dep_id = sqlite3_column_int64(st, 0);

        if (dep_id == target) {
            cycle = 1;
            break;
        }

        if (dfs_has_cycle(db, dep_id, target, visited)) {
            cycle = 1;
            break;
        }
    }

    sqlite3_finalize(st);
    return cycle;
}

/* =========================================================================
 * graph_add_package
 *
 * CALLER MUST hold an open BEGIN IMMEDIATE transaction before calling
 * this function.  graph_add_package does NOT begin or commit a
 * transaction — it only performs inserts and validation queries.
 *
 * On success: rows are inserted; caller commits when ready.
 * On failure: returns non-zero; caller must ROLLBACK.
 *             All allocated memory is freed before returning.
 * ========================================================================= */

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

    /* Canonicalize name */
    char *canon = strdup(name);
    if (!canon)
        return 1;
    normalize_lower(canon);

    /* 1. Reject duplicate */
    if (package_exists(db, canon)) {
        fprintf(stderr, "install: package '%s' is already installed\n", name);
        free(canon);
        return 1;
    }

    /* 2. Validate all dependencies exist and canonicalize */
    char **dep_canons = NULL;
    if (depends_count > 0) {
        dep_canons = calloc(depends_count, sizeof(char *));
        if (!dep_canons) {
            free(canon);
            return 1;
        }
    }

    for (size_t i = 0; i < depends_count; i++) {
        dep_canons[i] = strdup(depends[i]);
        if (!dep_canons[i]) {
            for (size_t j = 0; j < i; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }
        normalize_lower(dep_canons[i]);

        /* Self-dependency check */
        if (strcmp(dep_canons[i], canon) == 0) {
            fprintf(stderr,
                    "install: self-dependency not allowed (%s)\n", name);
            for (size_t j = 0; j <= i; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }

        /* Dependency must already be installed */
        if (!package_exists(db, dep_canons[i])) {
            fprintf(stderr,
                    "install: dependency '%s' is not installed\n",
                    dep_canons[i]);
            for (size_t j = 0; j <= i; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }
    }

    /* 3. Insert package row */
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "INSERT INTO packages(name, version, explicit) VALUES(?, ?, ?);",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "insert package prepare");

    sqlite3_bind_text(st, 1, canon,   -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, version, -1, SQLITE_STATIC);
    sqlite3_bind_int (st, 3, explicit_flag);

    rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "install: failed to insert package record\n");
        for (size_t i = 0; i < depends_count; i++) free(dep_canons[i]);
        free(dep_canons);
        free(canon);
        return 1;
    }

    sqlite3_int64 new_id = sqlite3_last_insert_rowid(db);

    /* 4. Insert dependency edges */
    for (size_t i = 0; i < depends_count; i++) {
        sqlite3_int64 dep_id = get_package_id(db, dep_canons[i]);
        if (dep_id < 0) {
            for (size_t j = 0; j < depends_count; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }

        rc = sqlite3_prepare_v2(
            db,
            "INSERT INTO dependencies(package_id, depends_on) VALUES(?, ?);",
            -1, &st, NULL
        );
        if (rc != SQLITE_OK)
            db_die(db, rc, "insert dep prepare");

        sqlite3_bind_int64(st, 1, new_id);
        sqlite3_bind_int64(st, 2, dep_id);

        rc = sqlite3_step(st);
        sqlite3_finalize(st);

        if (rc != SQLITE_DONE) {
            for (size_t j = 0; j < depends_count; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }
    }

    /* 5. Cycle detection — walk from each dependency back to new_id */
    for (size_t i = 0; i < depends_count; i++) {
        sqlite3_int64 dep_id = get_package_id(db, dep_canons[i]);
        VisitedSet visited = {0};

        if (dfs_has_cycle(db, dep_id, new_id, &visited)) {
            fprintf(stderr,
                    "install: dependency cycle detected involving '%s'\n",
                    name);
            for (size_t j = 0; j < depends_count; j++) free(dep_canons[j]);
            free(dep_canons);
            free(canon);
            return 1;
        }
    }

    /* Success — caller commits. */
    for (size_t i = 0; i < depends_count; i++) free(dep_canons[i]);
    free(dep_canons);
    free(canon);

    log_info("graph: registered package %s %s", name, version);
    return 0;
}

/* =========================================================================
 * graph_depends
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

    begin_deferred_tx(db);

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

    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
        printf("%s\n", sqlite3_column_text(st, 0));

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);
    return 0;
}

/* =========================================================================
 * graph_rdepends
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

    begin_deferred_tx(db);

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

    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
        printf("%s\n", sqlite3_column_text(st, 0));

    sqlite3_finalize(st);
    commit_tx(db);
    free(canon);
    return 0;
}

/* =========================================================================
 * graph_orphans
 * ========================================================================= */

int graph_orphans(void)
{
    sqlite3 *db = db_handle();
    if (!db)
        return 1;

    begin_deferred_tx(db);

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

    int found = 0;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        printf("%s\n", sqlite3_column_text(st, 0));
        found++;
    }

    sqlite3_finalize(st);
    commit_tx(db);
    return found;  /* caller prints "no orphans" message when 0 */
}
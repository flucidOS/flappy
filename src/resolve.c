/*
 * resolve.c - Dependency resolver for flappy
 *
 * DESIGN
 *
 *   Flappy's install pipeline requires that every declared dependency
 *   is already installed before the dependent package is committed to
 *   the DB (graph_add_package enforces this).  Previously the operator
 *   had to manually install dependencies in the correct order.
 *
 *   This module computes the correct order automatically.
 *
 * ALGORITHM
 *
 *   Iterative post-order DFS (topological sort) over the dependency
 *   graph declared in repo.db.  For each package:
 *
 *     1. Look up its declared dependencies in repo.db.
 *     2. Recursively resolve each dependency first.
 *     3. Add the package itself to the install queue.
 *
 *   Packages already installed are skipped (no re-install).
 *   Packages already queued in this run are skipped (diamond deps
 *   handled correctly — a shared dep is installed exactly once).
 *   Cycles produce a clear error and abort.
 *
 * SCOPE
 *
 *   Version constraint satisfaction uses version_satisfies() from
 *   version.c.  If the installed version of a dependency does not
 *   satisfy the constraint declared in repo.db, the resolution aborts
 *   with a clear message before any installation begins.
 *
 *   Conflict detection and atomicity are handled by the existing
 *   install pipeline — this module only determines order.
 *
 * LIMITS
 *
 *   MAX_QUEUE: maximum total packages (deps + target) in one resolution.
 *   MAX_STACK: maximum DFS recursion depth (longest dependency chain).
 *   Both are generous for a two-year single-maintainer OS.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "repo.h"
#include "install.h"
#include "resolve.h"
#include "version.h"
#include "pkg_meta.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUEUE  256
#define MAX_STACK  256

/* =========================================================================
 * Install queue — ordered list of package names to install
 * ========================================================================= */

typedef struct {
    char  names[MAX_QUEUE][64];
    int   count;
} Queue;

static int queue_contains(const Queue *q, const char *name)
{
    for (int i = 0; i < q->count; i++)
        if (strcmp(q->names[i], name) == 0)
            return 1;
    return 0;
}

static int queue_push(Queue *q, const char *name)
{
    if (q->count >= MAX_QUEUE) {
        fprintf(stderr,
            "[ERROR] resolve: dependency queue full (max %d packages)\n",
            MAX_QUEUE);
        return 1;
    }
    snprintf(q->names[q->count], sizeof(q->names[q->count]), "%s", name);
    q->count++;
    return 0;
}

/* =========================================================================
 * Cycle detection — packages currently on the DFS stack
 * ========================================================================= */

typedef struct {
    char  names[MAX_STACK][64];
    int   depth;
} Stack;

static int stack_contains(const Stack *s, const char *name)
{
    for (int i = 0; i < s->depth; i++)
        if (strcmp(s->names[i], name) == 0)
            return 1;
    return 0;
}

static int stack_push(Stack *s, const char *name)
{
    if (s->depth >= MAX_STACK) {
        fprintf(stderr,
            "[ERROR] resolve: dependency chain too deep (max %d)\n",
            MAX_STACK);
        return 1;
    }
    snprintf(s->names[s->depth], sizeof(s->names[s->depth]), "%s", name);
    s->depth++;
    return 0;
}

static void stack_pop(Stack *s)
{
    if (s->depth > 0)
        s->depth--;
}

/* =========================================================================
 * DB helpers
 * ========================================================================= */

/*
 * is_installed
 *
 * Returns 1 if `name` is present in the installed DB, 0 otherwise.
 * Opens its own read-only connection so it does not interfere with
 * the repo.db connection used during resolution.
 */
static int is_installed(const char *name)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(FLAPPY_DB_PATH, &db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(db,
        "SELECT 1 FROM packages WHERE name = ?;",
        -1, &st, NULL);
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    int found = (sqlite3_step(st) == SQLITE_ROW);

    sqlite3_finalize(st);
    sqlite3_close(db);
    return found;
}

/*
 * Dependency entry read from repo.db.
 */
typedef struct {
    char     name[64];
    dep_op_t op;
    char     version[32];  /* empty string if op == DEP_OP_NONE */
} RepoDep;

/*
 * get_repo_deps
 *
 * Looks up `pkgname` in repo.db and returns its declared dependencies.
 * `deps` must point to a caller-allocated array of at least MAX_DEPS entries.
 *
 * Returns the number of dependencies found, or -1 on error.
 */
#define MAX_DEPS 64

static int get_repo_deps(sqlite3 *repo, const char *pkgname,
                         RepoDep *deps)
{
    /*
     * repo.db stores dependencies in a `deps` table with columns:
     *   package   TEXT  (the dependent)
     *   depends   TEXT  (the dependency name)
     *   op        TEXT  (constraint operator string, NULL if none)
     *   version   TEXT  (constraint version string, NULL if none)
     *
     * If your repo.db schema differs, adjust the query here.
     * The resolver degrades gracefully to "no deps" if the table
     * is absent or the query returns no rows.
     */
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(repo,
        "SELECT depends, op, version "
        "FROM deps "
        "WHERE package = ?;",
        -1, &st, NULL);

    if (rc != SQLITE_OK) {
        /*
         * deps table may not exist in all repo.db versions.
         * Treat as zero dependencies rather than a hard error.
         */
        return 0;
    }

    sqlite3_bind_text(st, 1, pkgname, -1, SQLITE_STATIC);

    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW && count < MAX_DEPS) {
        const char *dep_name = (const char *)sqlite3_column_text(st, 0);
        const char *op_str   = (const char *)sqlite3_column_text(st, 1);
        const char *dep_ver  = (const char *)sqlite3_column_text(st, 2);

        if (!dep_name || dep_name[0] == '\0')
            continue;

        snprintf(deps[count].name, sizeof(deps[count].name),
                 "%s", dep_name);

        deps[count].op         = DEP_OP_NONE;
        deps[count].version[0] = '\0';

        if (op_str && dep_ver) {
            if      (strcmp(op_str, ">=") == 0) deps[count].op = DEP_OP_GE;
            else if (strcmp(op_str, "<=") == 0) deps[count].op = DEP_OP_LE;
            else if (strcmp(op_str, ">")  == 0) deps[count].op = DEP_OP_GT;
            else if (strcmp(op_str, "<")  == 0) deps[count].op = DEP_OP_LT;
            else if (strcmp(op_str, "=")  == 0) deps[count].op = DEP_OP_EQ;

            if (deps[count].op != DEP_OP_NONE)
                snprintf(deps[count].version, sizeof(deps[count].version),
                         "%s", dep_ver);
        }

        count++;
    }

    sqlite3_finalize(st);
    return count;
}

/*
 * pkg_exists_in_repo
 *
 * Returns 1 if `name` is present in repo.db's packages table.
 */
static int pkg_exists_in_repo(sqlite3 *repo, const char *name)
{
    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(repo,
        "SELECT 1 FROM packages WHERE name = ?;",
        -1, &st, NULL);
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    int found = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return found;
}

/*
 * get_installed_version_str
 *
 * Returns installed version of `name` into `out` (must be >= 64 bytes).
 * Returns 1 if found, 0 if not installed.
 * Opens its own connection.
 */
static int get_installed_version_str(const char *name, char *out, size_t outsz)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(FLAPPY_DB_PATH, &db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(db,
        "SELECT version FROM packages WHERE name = ?;",
        -1, &st, NULL);
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    int found = 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *v = (const char *)sqlite3_column_text(st, 0);
        if (v) {
            snprintf(out, outsz, "%s", v);
            found = 1;
        }
    }

    sqlite3_finalize(st);
    sqlite3_close(db);
    return found;
}

/* =========================================================================
 * DFS — recursive post-order traversal
 *
 * Visits dependencies before the package itself, building the install
 * queue in the correct order.
 * ========================================================================= */

static int dfs(sqlite3 *repo,
               const char *pkgname,
               Queue *queue,
               Stack *stack)
{
    /* Cycle check */
    if (stack_contains(stack, pkgname)) {
        fprintf(stderr,
            "[ERROR] resolve: dependency cycle detected at '%s'\n",
            pkgname);
        /* Print the current chain for diagnostics */
        fprintf(stderr, "  cycle: ");
        for (int i = 0; i < stack->depth; i++)
            fprintf(stderr, "%s -> ", stack->names[i]);
        fprintf(stderr, "%s\n", pkgname);
        return 1;
    }

    /* Already queued in this run — diamond dependency, skip */
    if (queue_contains(queue, pkgname))
        return 0;

    /*
     * Already installed and not being upgraded in this run — skip.
     * Version constraint satisfaction for already-installed deps is
     * checked below (before skipping) so we don't silently accept
     * an underversion.
     */

    /* Verify the package exists in the repo */
    if (!pkg_exists_in_repo(repo, pkgname)) {
        fprintf(stderr,
            "[ERROR] resolve: package '%s' not found in repository\n",
            pkgname);
        return 1;
    }

    /* Push onto DFS stack */
    if (stack_push(stack, pkgname) != 0)
        return 1;

    /* Fetch and recurse into dependencies */
    RepoDep deps[MAX_DEPS];
    int dep_count = get_repo_deps(repo, pkgname, deps);

    if (dep_count < 0) {
        stack_pop(stack);
        return 1;
    }

    for (int i = 0; i < dep_count; i++) {
        const char *dep_name = deps[i].name;

        /*
         * If the dependency is already installed, check its version
         * satisfies any constraint before accepting it.
         */
        if (is_installed(dep_name)) {
            if (deps[i].op != DEP_OP_NONE) {
                char inst_ver[64] = {0};
                if (get_installed_version_str(dep_name,
                                              inst_ver, sizeof(inst_ver))) {
                    if (!version_satisfies(inst_ver,
                                           deps[i].op,
                                           deps[i].version)) {
                        fprintf(stderr,
                            "[ERROR] resolve: installed %s %s does not "
                            "satisfy %s %s %s required by %s\n",
                            dep_name, inst_ver,
                            dep_name,
                            deps[i].op == DEP_OP_GE ? ">=" :
                            deps[i].op == DEP_OP_LE ? "<=" :
                            deps[i].op == DEP_OP_GT ? ">"  :
                            deps[i].op == DEP_OP_LT ? "<"  : "=",
                            deps[i].version,
                            pkgname);
                        stack_pop(stack);
                        return 1;
                    }
                }
            }
            /* Installed and constraint satisfied — no need to queue */
            continue;
        }

        /* Recurse — install dependency before this package */
        if (dfs(repo, dep_name, queue, stack) != 0) {
            stack_pop(stack);
            return 1;
        }
    }

    stack_pop(stack);

    /*
     * Post-order: add this package to the queue after all its deps.
     * Skip if already installed (and not already queued — caught above).
     */
    if (!is_installed(pkgname)) {
        if (queue_push(queue, pkgname) != 0)
            return 1;
    }

    return 0;
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int resolve_and_install(const char *pkgname)
{
    /* Open repo.db read-only for the duration of resolution */
    sqlite3 *repo = NULL;
    if (sqlite3_open_v2(FLAPPY_REPO_DB_PATH, &repo,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr,
            "[ERROR] resolve: cannot open repository database "
            "(run 'flappy update')\n");
        if (repo) sqlite3_close(repo);
        return 1;
    }

    Queue queue = {0};
    Stack stack = {0};

    /* Build the install order via DFS */
    int rc = dfs(repo, pkgname, &queue, &stack);
    sqlite3_close(repo);

    if (rc != 0)
        return 1;

    if (queue.count == 0) {
        /* Target already installed, nothing to do */
        fprintf(stderr, "[INFO] %s is already installed\n", pkgname);
        return 0;
    }

    /* Print the install plan before doing anything */
    if (queue.count > 1) {
        fprintf(stderr, "[INFO] install order:\n");
        for (int i = 0; i < queue.count; i++)
            fprintf(stderr, "  %d. %s%s\n",
                    i + 1,
                    queue.names[i],
                    (strcmp(queue.names[i], pkgname) == 0)
                        ? " (requested)" : " (dependency)");
        fprintf(stderr, "\n");
    }

    /* Install in order — each call goes through the full pipeline */
    for (int i = 0; i < queue.count; i++) {
        if (install_package(queue.names[i]) != 0) {
            fprintf(stderr,
                "[ERROR] resolve: failed to install '%s' — "
                "stopping (subsequent packages not installed)\n",
                queue.names[i]);
            return 1;
        }
    }

    return 0;
}
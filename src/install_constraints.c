/*
 * install_constraints.c - Dependency version constraint checker
 *
 * Called after metadata is read, before graph_add_package.
 *
 * For each declared dependency with a version constraint,
 * looks up the installed version and verifies it satisfies
 * the constraint.
 *
 * Returns:
 *   0  all constraints satisfied
 *   1  one or more constraints violated
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "pkg_meta.h"
#include "version.h"
#include "db_guard.h"
#include "ui.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *op_str(dep_op_t op)
{
    switch (op) {
    case DEP_OP_GE: return ">=";
    case DEP_OP_LE: return "<=";
    case DEP_OP_GT: return ">";
    case DEP_OP_LT: return "<";
    case DEP_OP_EQ: return "=";
    default:        return "?";
    }
}

/*
 * get_installed_version
 *
 * Returns the installed version of `name`, or NULL if not installed.
 * Caller must free the returned string.
 */
static char *get_installed_version(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(db,
        "SELECT version FROM packages WHERE name = ?;",
        -1, &st, NULL);
    if (rc != SQLITE_OK)
        return NULL;

    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    char *version = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *v = (const char *)sqlite3_column_text(st, 0);
        if (v) version = strdup(v);
    }

    sqlite3_finalize(st);
    return version;
}

int install_check_constraints(const struct flappy_pkg *pkg)
{
    sqlite3 *db = db_handle();
    if (!db) return 1;

    int failed = 0;

    for (size_t i = 0; i < pkg->depends_count; i++) {
        const struct dep_entry *dep = &pkg->depends[i];

        /* No constraint — just needs to be installed, handled by graph */
        if (dep->op == DEP_OP_NONE)
            continue;

        char *installed = get_installed_version(db, dep->name);

        if (!installed) {
            /* Not installed at all — graph_add_package will catch this */
            continue;
        }

        if (!version_satisfies(installed, dep->op, dep->version)) {
            ui_error("dependency constraint not satisfied: %s %s %s (installed: %s)",
                     dep->name, op_str(dep->op), dep->version, installed);
            log_error("constraint failed: %s requires %s %s %s, installed %s",
                      pkg->name, dep->name,
                      op_str(dep->op), dep->version, installed);
            failed = 1;
        }

        free(installed);
    }

    return failed;
}
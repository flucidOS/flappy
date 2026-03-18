/*
 * remove.c - Package removal engine
 *
 * UX contract:
 *   remove: removed: <pkg>
 *   purge:  purged: <pkg>
 *   purge --force: [WARN] forced purge of <pkg> / required by: <dep>
 *   autoremove: removing: <pkg> / [INFO] no orphan packages
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "db_guard.h"
#include "ui.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* =========================================================================
 * Internal: check + print reverse dependencies
 * ========================================================================= */

static int check_rdepends(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d  ON p1.id = d.depends_on "
        "JOIN packages p2     ON p2.id = d.package_id "
        "WHERE p1.name = ? "
        "ORDER BY p2.name COLLATE BINARY ASC;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "rdepends check");

    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (count == 0) {
            ui_error("cannot remove %s", name);
            fprintf(stderr, "\nrequired by:\n");
        }
        fprintf(stderr, "  %s\n",
                (const char *)sqlite3_column_text(st, 0));
        count++;
    }

    sqlite3_finalize(st);
    return count;
}

static void warn_rdepends(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p2.name "
        "FROM packages p1 "
        "JOIN dependencies d  ON p1.id = d.depends_on "
        "JOIN packages p2     ON p2.id = d.package_id "
        "WHERE p1.name = ?;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK) { sqlite3_finalize(st); return; }

    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (count == 0) {
            ui_warn("forced purge of %s", name);
            fprintf(stderr, "\nrequired by:\n");
        }
        const char *dep = (const char *)sqlite3_column_text(st, 0);
        fprintf(stderr, "  %s\n", dep);
        log_error("forced purge of %s which is required by %s", name, dep);
        count++;
    }

    sqlite3_finalize(st);
    if (count > 0)
        fprintf(stderr, "\n");
}

/* =========================================================================
 * Internal utilities
 * ========================================================================= */

static sqlite3_int64 get_pkg_id(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(db,
        "SELECT id FROM packages WHERE name = ?;",
        -1, &st, NULL);
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);

    sqlite3_int64 id = -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        id = sqlite3_column_int64(st, 0);

    sqlite3_finalize(st);
    return id;
}

typedef struct { char **paths; size_t count; } FileList;

static void filelist_free(FileList *fl)
{
    for (size_t i = 0; i < fl->count; i++) free(fl->paths[i]);
    free(fl->paths);
    fl->paths = NULL;
    fl->count = 0;
}

static int collect_files(sqlite3 *db, sqlite3_int64 pkg_id, FileList *fl)
{
    fl->paths = NULL; fl->count = 0;
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(db,
        "SELECT path FROM files WHERE package_id = ? ORDER BY path DESC;",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return 1;

    sqlite3_bind_int64(st, 1, pkg_id);
    size_t cap = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        if (!p) continue;
        if (fl->count >= cap) {
            size_t nc = cap ? cap * 2 : 64;
            char **tmp = realloc(fl->paths, nc * sizeof(char *));
            if (!tmp) { sqlite3_finalize(st); return 1; }
            fl->paths = tmp; cap = nc;
        }
        fl->paths[fl->count] = strdup(p);
        if (!fl->paths[fl->count]) { sqlite3_finalize(st); return 1; }
        fl->count++;
    }

    sqlite3_finalize(st);
    return 0;
}

static int delete_files(const FileList *fl, int keep_configs)
{
    int errors = 0;
    for (size_t i = 0; i < fl->count; i++) {
        const char *path = fl->paths[i];
        if (keep_configs && strncmp(path, "/etc/", 5) == 0)
            continue;
        if (unlink(path) != 0 && errno != ENOENT) {
            log_error("remove: failed to delete %s: %s",
                      path, strerror(errno));
            errors++;
        }
    }
    return errors;
}

static int delete_db_record(sqlite3 *db, sqlite3_int64 pkg_id)
{
    sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;

    int rc = sqlite3_prepare_v2(db,
        "DELETE FROM packages WHERE id = ?;",
        -1, &st, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return 1;
    }

    sqlite3_bind_int64(st, 1, pkg_id);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return 1;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return 0;
}

/* =========================================================================
 * Public: remove_package
 * ========================================================================= */

int remove_package(const char *name)
{
    sqlite3 *db = db_handle();
    if (!db) return 1;

    sqlite3_int64 pkg_id = get_pkg_id(db, name);
    if (pkg_id < 0) {
        ui_error("package '%s' is not installed", name);
        return 1;
    }

    if (check_rdepends(db, name) > 0)
        return 1;

    FileList fl = {0};
    if (collect_files(db, pkg_id, &fl) != 0) {
        ui_error("failed to collect file list");
        return 1;
    }

    int errors = delete_files(&fl, 1);
    filelist_free(&fl);

    if (errors > 0) {
        ui_error("%d file(s) could not be deleted — DB record kept", errors);
        return 1;
    }

    if (delete_db_record(db, pkg_id) != 0) {
        ui_error("failed to remove DB record for '%s'", name);
        return 1;
    }

    log_info("remove: removed package %s", name);
    fprintf(stdout, "removed: %s\n", name);
    return 0;
}

/* =========================================================================
 * Public: purge_package
 * ========================================================================= */

int purge_package(const char *name, int force)
{
    sqlite3 *db = db_handle();
    if (!db) return 1;

    sqlite3_int64 pkg_id = get_pkg_id(db, name);
    if (pkg_id < 0) {
        ui_error("package '%s' is not installed", name);
        return 1;
    }

    if (!force) {
        if (check_rdepends(db, name) > 0)
            return 1;
    } else {
        warn_rdepends(db, name);
    }

    FileList fl = {0};
    if (collect_files(db, pkg_id, &fl) != 0) {
        ui_error("failed to collect file list");
        return 1;
    }

    int errors = delete_files(&fl, 0);
    filelist_free(&fl);

    if (errors > 0) {
        ui_error("%d file(s) could not be deleted — DB record kept", errors);
        return 1;
    }

    if (delete_db_record(db, pkg_id) != 0) {
        ui_error("failed to remove DB record for '%s'", name);
        return 1;
    }

    if (force)
        log_error("forced purge of %s completed", name);
    else
        log_info("purge: removed package %s", name);

    fprintf(stdout, "purged: %s\n", name);
    return 0;
}

/* =========================================================================
 * Public: autoremove_packages
 * ========================================================================= */

int autoremove_packages(void)
{
    sqlite3 *db = db_handle();
    if (!db) return 1;

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT p.name "
        "FROM packages p "
        "LEFT JOIN dependencies d ON d.depends_on = p.id "
        "WHERE p.explicit = 0 "
        "GROUP BY p.id "
        "HAVING COUNT(d.package_id) = 0 "
        "ORDER BY p.name COLLATE BINARY ASC;",
        -1, &st, NULL);
    if (rc != SQLITE_OK)
        db_die(db, rc, "autoremove prepare");

    char **orphans = NULL;
    size_t count = 0, cap = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *n = (const char *)sqlite3_column_text(st, 0);
        if (!n) continue;
        if (count >= cap) {
            size_t nc = cap ? cap * 2 : 16;
            char **tmp = realloc(orphans, nc * sizeof(char *));
            if (!tmp) break;
            orphans = tmp; cap = nc;
        }
        orphans[count] = strdup(n);
        if (!orphans[count]) break;
        count++;
    }
    sqlite3_finalize(st);

    if (count == 0) {
        ui_info("no orphan packages");
        free(orphans);
        return 0;
    }

    int errors = 0;
    for (size_t i = 0; i < count; i++) {
        fprintf(stdout, "removing: %s\n", orphans[i]);

        sqlite3_int64 pkg_id = get_pkg_id(db, orphans[i]);
        if (pkg_id < 0) { free(orphans[i]); continue; }

        FileList fl = {0};
        if (collect_files(db, pkg_id, &fl) != 0) {
            errors++; free(orphans[i]); continue;
        }

        int errs = delete_files(&fl, 1);
        filelist_free(&fl);

        if (errs > 0) { errors++; free(orphans[i]); continue; }
        if (delete_db_record(db, pkg_id) != 0) {
            errors++; free(orphans[i]); continue;
        }

        log_info("autoremove: removed orphan %s", orphans[i]);
        free(orphans[i]);
    }

    free(orphans);

    if (errors > 0) {
        ui_error("%d package(s) could not be removed", errors);
        return 1;
    }

    return 0;
}
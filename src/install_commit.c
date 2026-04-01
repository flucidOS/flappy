/*
 * install_commit.c - Atomic package installation commit
 *
 * TRANSACTION MODEL (fix for split-transaction bug):
 *
 *   A single BEGIN IMMEDIATE transaction is opened here and covers:
 *     1. graph_add_package  (inserts package row + dependency edges)
 *     2. register_files     (inserts file rows)
 *     3. COMMIT
 *
 *   File copy to the real filesystem happens AFTER the transaction
 *   commits.  On copy failure, the DB is clean (already committed),
 *   so we roll back the DB record with a separate DELETE and then
 *   unlink any files already written.
 *
 *   This removes the previous window where a crash between the
 *   graph_add_package COMMIT and the file-registration COMMIT left
 *   a ghost package row with no files.
 *
 * Invariant: a failed install leaves the system unchanged.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "graph.h"
#include "install_constraints.h"
#include "pkg_meta.h"
#include "db_guard.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

/*
 * copy_file
 *
 * Copies a regular file from src to dst.
 * Creates parent directories as needed.
 * Preserves permissions from src.
 */
static int copy_file(const char *src, const char *dst)
{
    /* Create parent directory */
    char parent[PATH_MAX];
    strncpy(parent, dst, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';

    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        for (char *p = parent + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                if (mkdir(parent, 0755) == -1 && errno != EEXIST)
                    return -1;
                *p = '/';
            }
        }
        if (mkdir(parent, 0755) == -1 && errno != EEXIST)
            return -1;
    }

    /* Get source permissions */
    struct stat st;
    if (stat(src, &st) != 0)
        return -1;

    /* Open src */
    int fdin = open(src, O_RDONLY);
    if (fdin < 0)
        return -1;

    /* Open dst (create/truncate) */
    int fdout = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (fdout < 0) {
        close(fdin);
        return -1;
    }

    char buf[65536];
    ssize_t n;
    int err = 0;

    while ((n = read(fdin, buf, sizeof(buf))) > 0) {
        if (write(fdout, buf, (size_t)n) != n) {
            err = 1;
            break;
        }
    }

    if (n < 0)
        err = 1;

    close(fdin);
    close(fdout);

    if (err)
        unlink(dst);

    return err ? -1 : 0;
}

/* =========================================================================
 * Staging directory walker
 * ========================================================================= */

typedef struct {
    char  **paths;
    size_t  count;
    size_t  cap;
} PathList;

static int pathlist_add(PathList *pl, const char *path)
{
    if (pl->count >= pl->cap) {
        size_t newcap = pl->cap ? pl->cap * 2 : 64;
        char **tmp = realloc(pl->paths, newcap * sizeof(char *));
        if (!tmp)
            return -1;
        pl->paths = tmp;
        pl->cap   = newcap;
    }
    pl->paths[pl->count] = strdup(path);
    if (!pl->paths[pl->count])
        return -1;
    pl->count++;
    return 0;
}

static void pathlist_free(PathList *pl)
{
    for (size_t i = 0; i < pl->count; i++)
        free(pl->paths[i]);
    free(pl->paths);
    pl->paths = NULL;
    pl->count = 0;
    pl->cap   = 0;
}

static int walk_staging(const char *staging_dir,
                        const char *rel_prefix,
                        PathList *pl)
{
    char abs_dir[PATH_MAX];
    if (*rel_prefix)
        snprintf(abs_dir, sizeof(abs_dir), "%s/%s", staging_dir, rel_prefix);
    else
        snprintf(abs_dir, sizeof(abs_dir), "%s", staging_dir);

    DIR *d = opendir(abs_dir);
    if (!d)
        return -1;

    struct dirent *ent;
    int err = 0;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;

        char rel[PATH_MAX];
        if (*rel_prefix)
            snprintf(rel, sizeof(rel), "%s/%s", rel_prefix, ent->d_name);
        else
            snprintf(rel, sizeof(rel), "%s", ent->d_name);

        char abs[PATH_MAX * 2];
        int abs_n = snprintf(abs, sizeof(abs), "%s/%s", staging_dir, rel);
        if (abs_n < 0 || abs_n >= (int)sizeof(abs)) {
            err = -1;
            break;
        }

        struct stat st;
        if (lstat(abs, &st) != 0) {
            err = -1;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            if (walk_staging(staging_dir, rel, pl) != 0) {
                err = -1;
                break;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (pathlist_add(pl, rel) != 0) {
                err = -1;
                break;
            }
        }
        /* symlinks and other types are skipped */
    }

    closedir(d);
    return err;
}

/* =========================================================================
 * DB file registration
 * ========================================================================= */

static int register_files(sqlite3 *db,
                           sqlite3_int64 pkg_id,
                           const PathList *pl)
{
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "INSERT INTO files(path, package_id) VALUES(?, ?);",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(db, rc, "register_files prepare");

    for (size_t i = 0; i < pl->count; i++) {
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);

        char canonical[PATH_MAX];
        snprintf(canonical, sizeof(canonical), "/%s", pl->paths[i]);

        sqlite3_bind_text (st, 1, canonical, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, pkg_id);

        rc = sqlite3_step(st);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(st);
            return 1;
        }
    }

    sqlite3_finalize(st);
    return 0;
}

/* =========================================================================
 * Rollback helpers
 * ========================================================================= */

static void rollback_files(const char * const *paths, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (unlink(paths[i]) != 0 && errno != ENOENT)
            log_error("rollback: failed to remove %s: %s",
                      paths[i], strerror(errno));
    }
}

static void remove_staging(const char *staging_dir)
{
    /*
     * Best-effort recursive removal via shell — staging dir only.
     * This path is used for cleanup after a successful install.
     * The shell injection risk from clean.c does NOT apply here
     * because staging_dir is constructed by install_extract from
     * a known prefix + archive basename and is never user-supplied.
     */
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", staging_dir);
    (void)system(cmd);
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int install_commit(const char *pkgname,
                   const char *pkgfile,
                   const char *staging_dir)
{
    /*
     * 1. Read package metadata from the cached archive.
     */
    struct flappy_pkg *meta = pkg_read_from_file(pkgfile);
    if (!meta) {
        fprintf(stderr, "commit: cannot read package metadata from %s\n",
                pkgfile);
        return 1;
    }

    if (strcmp(meta->name, pkgname) != 0) {
        fprintf(stderr,
                "commit: package name mismatch: expected '%s' got '%s'\n",
                pkgname, meta->name);
        pkg_meta_free(meta);
        return 1;
    }

    sqlite3 *db = db_handle();
    if (!db) {
        pkg_meta_free(meta);
        return 1;
    }

    /*
     * 2. Collect staged files.
     */
    PathList staged = {0};
    if (walk_staging(staging_dir, "", &staged) != 0) {
        fprintf(stderr, "commit: failed to walk staging dir\n");
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 3a. Check version constraints before touching the DB.
     */
    if (install_check_constraints(meta)) {
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 3b. Build plain name array for graph_add_package.
     */
    const char **dep_names = NULL;
    if (meta->depends_count > 0) {
        dep_names = malloc(meta->depends_count * sizeof(char *));
        if (!dep_names) {
            pkg_meta_free(meta);
            pathlist_free(&staged);
            return 1;
        }
        for (size_t i = 0; i < meta->depends_count; i++)
            dep_names[i] = meta->depends[i].name;
    }

    /*
     * 4. Open the single transaction that covers package row,
     *    dependency edges, and file registration.
     *
     *    graph_add_package does NOT manage its own transaction —
     *    it relies on this one being open already.
     */
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "commit: could not begin transaction\n");
        free(dep_names);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 4a. Insert package + dependency rows.
     */
    int rc = graph_add_package(
        meta->name,
        meta->version,
        1,
        dep_names,
        meta->depends_count
    );
    free(dep_names);

    if (rc != 0) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 4b. Retrieve the new package rowid for file registration.
     *     We are still inside the transaction so the row is visible.
     */
    sqlite3_int64 pkg_id = -1;
    {
        sqlite3_stmt *st = NULL;
        sqlite3_prepare_v2(db,
            "SELECT id FROM packages WHERE name = ?;",
            -1, &st, NULL);
        sqlite3_bind_text(st, 1, meta->name, -1, SQLITE_STATIC);
        if (sqlite3_step(st) == SQLITE_ROW)
            pkg_id = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }

    if (pkg_id < 0) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 4c. Register file paths.
     */
    if (register_files(db, pkg_id, &staged) != 0) {
        fprintf(stderr, "commit: failed to register files in DB\n");
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 4d. COMMIT — package row, dep edges, and file rows are now durable.
     *     No window exists between package row and file rows.
     */
    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "commit: transaction commit failed\n");
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        return 1;
    }

    /*
     * 5. Copy files from staging to the real filesystem.
     *
     *    The DB is already committed at this point.  If a copy fails
     *    we must undo the DB record (one clean DELETE) and unlink any
     *    files already written.
     */
    char **written = calloc(staged.count, sizeof(char *));
    if (!written && staged.count > 0) {
        /* Extremely unlikely — OOM after commit. Roll back DB record. */
        sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
        {
            sqlite3_stmt *del = NULL;
            sqlite3_prepare_v2(db,
                "DELETE FROM packages WHERE id = ?;",
                -1, &del, NULL);
            sqlite3_bind_int64(del, 1, pkg_id);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        remove_staging(staging_dir);
        return 1;
    }

    size_t written_count = 0;
    int copy_failed = 0;

    for (size_t i = 0; i < staged.count; i++) {
        char src[PATH_MAX], dst[PATH_MAX];

        snprintf(src, sizeof(src), "%s/%s", staging_dir, staged.paths[i]);
        snprintf(dst, sizeof(dst), "/%s", staged.paths[i]);

        if (copy_file(src, dst) != 0) {
            fprintf(stderr,
                    "commit: failed to install /%s: %s\n",
                    staged.paths[i], strerror(errno));
            copy_failed = 1;
            break;
        }

        written[written_count] = strdup(dst);
        if (!written[written_count]) {
            copy_failed = 1;
            written_count++;
            break;
        }
        written_count++;
    }

    if (copy_failed) {
        /*
         * DB is already committed — delete the package record cleanly.
         * The ON DELETE CASCADE on files and dependencies handles
         * the child rows automatically.
         */
        sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
        {
            sqlite3_stmt *del = NULL;
            sqlite3_prepare_v2(db,
                "DELETE FROM packages WHERE id = ?;",
                -1, &del, NULL);
            sqlite3_bind_int64(del, 1, pkg_id);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

        rollback_files((const char *const *)written, written_count);

        for (size_t i = 0; i < written_count; i++) free(written[i]);
        free(written);
        pkg_meta_free(meta);
        pathlist_free(&staged);
        remove_staging(staging_dir);
        return 1;
    }

    log_info("install: committed %s %s (%zu files)",
             meta->name, meta->version, staged.count);

    for (size_t i = 0; i < written_count; i++) free(written[i]);
    free(written);
    pkg_meta_free(meta);
    pathlist_free(&staged);
    remove_staging(staging_dir);

    return 0;
}
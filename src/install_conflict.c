/*
 * install_conflict.c - File conflict detection against staged paths
 *
 * Previously read the .FILES manifest from the package archive and
 * checked those paths against the installed DB.  This meant that if
 * .FILES and the actual archive content diverged (malformed package,
 * hand-edited archive), conflicts could be silently missed.
 *
 * This version is called AFTER install_extract has run.  It walks the
 * staging directory — the definitive list of what would actually be
 * installed — and checks each real path against the DB.
 *
 * The function signature is now:
 *
 *   int install_conflict_staged(const char *pkgname,
 *                               const char *staging_dir);
 *
 * install.c calls this after extraction and before commit.
 * Atomicity is preserved: nothing has been written to the real
 * filesystem yet (staging is separate), so an abort here is clean.
 *
 * Returns:
 *   0  no conflicts
 *   1  conflict detected or error
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

/* =========================================================================
 * Recursive staging walker
 *
 * Collects canonical FS paths (with leading /) for every non-directory
 * entry under staging_dir.
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
        if (!tmp) return -1;
        pl->paths = tmp;
        pl->cap   = newcap;
    }
    pl->paths[pl->count] = strdup(path);
    if (!pl->paths[pl->count]) return -1;
    pl->count++;
    return 0;
}

static void pathlist_free(PathList *pl)
{
    for (size_t i = 0; i < pl->count; i++) free(pl->paths[i]);
    free(pl->paths);
    pl->paths = NULL;
    pl->count = 0;
    pl->cap   = 0;
}

static int walk_staged(const char *staging_dir,
                       const char *rel_prefix,
                       PathList   *pl)
{
    char abs_dir[PATH_MAX];
    if (*rel_prefix)
        snprintf(abs_dir, sizeof(abs_dir), "%s/%s", staging_dir, rel_prefix);
    else
        snprintf(abs_dir, sizeof(abs_dir), "%s", staging_dir);

    DIR *d = opendir(abs_dir);
    if (!d) return -1;

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

        char abs[PATH_MAX];
        int n = snprintf(abs, sizeof(abs), "%s/%s", staging_dir, rel);
        if (n < 0 || n >= (int)sizeof(abs)) { err = -1; break; }

        struct stat st;
        if (lstat(abs, &st) != 0) { err = -1; break; }

        if (S_ISDIR(st.st_mode)) {
            if (walk_staged(staging_dir, rel, pl) != 0) {
                err = -1; break;
            }
        } else {
            /* Regular files, symlinks, and any other non-dir entries */
            /* PATH_MAX already consumed by rel; prepend '/' safely */
            if (strlen(rel) >= PATH_MAX - 1) {
                err = -1; break;
            }
            char canonical[PATH_MAX + 1];
            canonical[0] = '/';
            memcpy(canonical + 1, rel, strlen(rel) + 1);
            if (pathlist_add(pl, canonical) != 0) {
                err = -1; break;
            }
        }
    }

    closedir(d);
    return err;
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int install_conflict_staged(const char *pkgname, const char *staging_dir)
{
    PathList pl = {0};

    if (walk_staged(staging_dir, "", &pl) != 0) {
        fprintf(stderr, "conflict: failed to walk staging directory\n");
        pathlist_free(&pl);
        return 1;
    }

    if (pl.count == 0) {
        /* Empty staging — nothing to conflict with */
        pathlist_free(&pl);
        return 0;
    }

    sqlite3 *db = db_handle();
    if (!db) {
        pathlist_free(&pl);
        return 1;
    }

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p.name FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "WHERE f.path = ? AND p.name != ?;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK) {
        db_die(db, rc, "conflict prepare");
    }

    int conflict = 0;

    for (size_t i = 0; i < pl.count && !conflict; i++) {
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_text(st, 1, pl.paths[i], -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 2, pkgname,     -1, SQLITE_STATIC);

        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *owner = sqlite3_column_text(st, 0);
            fprintf(stderr,
                    "conflict: %s is already owned by %s\n",
                    pl.paths[i],
                    owner ? (const char *)owner : "unknown");
            conflict = 1;
        }
    }

    sqlite3_finalize(st);
    pathlist_free(&pl);

    return conflict;
}
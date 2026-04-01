/*
 * clean.c - Package cache cleanup
 *
 * flappy clean       : remove staging directory contents only
 * flappy clean --all : remove staging + cached package files
 *
 * Staging dir  : /var/cache/flappy/staging/
 * Package cache: /var/cache/flappy/packages/
 *
 * Exit codes:
 *   0 - success
 *   1 - one or more files could not be removed
 *
 * SECURITY NOTE:
 *   The previous implementation used system("rm -rf \"<path>\"") to
 *   remove staging subdirectories.  This has been replaced with a
 *   pure POSIX recursive removal (remove_tree / clean_directory)
 *   that never invokes a shell, eliminating any shell-injection
 *   surface regardless of how directory entry names are formed.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

#define STAGING_DIR  "/var/cache/flappy/staging"
#define PACKAGES_DIR "/var/cache/flappy/packages"

/* =========================================================================
 * remove_tree
 *
 * Recursively removes everything rooted at the open directory fd `dfd`
 * (which corresponds to the path `path`, used only for error messages).
 * Removes the directory itself via the parent fd `parent_dfd` +
 * `dirname` after emptying it.
 *
 * Uses openat/unlinkat/fdopendir so no path strings are passed to a
 * shell at any point.
 *
 * Returns count of errors encountered.
 * ========================================================================= */

static int remove_tree(int parent_dfd, const char *dirname, const char *path)
{
    int dfd = openat(parent_dfd, dirname, O_RDONLY | O_DIRECTORY);
    if (dfd < 0) {
        if (errno == ENOENT)
            return 0;
        fprintf(stderr, "clean: cannot open dir %s: %s\n",
                path, strerror(errno));
        return 1;
    }

    /* fdopendir takes ownership of dfd; do not close dfd separately */
    DIR *d = fdopendir(dfd);
    if (!d) {
        fprintf(stderr, "clean: fdopendir failed for %s: %s\n",
                path, strerror(errno));
        close(dfd);
        return 1;
    }

    int errors = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;

        /*
         * Build a display path for error messages only — never
         * passed to a shell.
         */
        char child_path[PATH_MAX];
        int n = snprintf(child_path, sizeof(child_path),
                         "%s/%s", path, ent->d_name);
        if (n < 0 || n >= (int)sizeof(child_path)) {
            errors++;
            continue;
        }

        /*
         * Determine entry type.  Use fstatat with AT_SYMLINK_NOFOLLOW
         * so symlinks are removed directly rather than followed.
         */
        struct stat st;
        if (fstatat(dfd, ent->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            fprintf(stderr, "clean: stat failed for %s: %s\n",
                    child_path, strerror(errno));
            errors++;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /* Recurse, then remove the now-empty directory */
            errors += remove_tree(dfd, ent->d_name, child_path);
        } else {
            /* Regular file, symlink, or other non-directory */
            if (unlinkat(dfd, ent->d_name, 0) != 0 && errno != ENOENT) {
                fprintf(stderr, "clean: cannot remove %s: %s\n",
                        child_path, strerror(errno));
                errors++;
            }
        }
    }

    closedir(d); /* also closes dfd */

    /* Remove the directory entry itself from its parent */
    if (errors == 0) {
        if (unlinkat(parent_dfd, dirname, AT_REMOVEDIR) != 0 &&
                errno != ENOENT) {
            fprintf(stderr, "clean: cannot remove dir %s: %s\n",
                    path, strerror(errno));
            errors++;
        }
    }

    return errors;
}

/* =========================================================================
 * clean_directory
 *
 * Removes all entries inside `dir` but leaves the directory itself.
 * Returns count of errors.
 * ========================================================================= */

static int clean_directory(const char *dir)
{
    int dfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dfd < 0) {
        if (errno == ENOENT)
            return 0; /* nothing to clean */
        fprintf(stderr, "clean: cannot open %s: %s\n",
                dir, strerror(errno));
        return 1;
    }

    DIR *d = fdopendir(dfd);
    if (!d) {
        fprintf(stderr, "clean: fdopendir failed for %s: %s\n",
                dir, strerror(errno));
        close(dfd);
        return 1;
    }

    int errors = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;

        char child_path[PATH_MAX];
        int n = snprintf(child_path, sizeof(child_path),
                         "%s/%s", dir, ent->d_name);
        if (n < 0 || n >= (int)sizeof(child_path)) {
            errors++;
            continue;
        }

        struct stat st;
        if (fstatat(dfd, ent->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            fprintf(stderr, "clean: stat failed for %s: %s\n",
                    child_path, strerror(errno));
            errors++;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /*
             * Staging subdirectories (.stage dirs) — remove recursively.
             * remove_tree removes the directory itself when done.
             */
            errors += remove_tree(dfd, ent->d_name, child_path);
        } else {
            if (unlinkat(dfd, ent->d_name, 0) != 0 && errno != ENOENT) {
                fprintf(stderr, "clean: cannot remove %s: %s\n",
                        child_path, strerror(errno));
                errors++;
            }
        }
    }

    closedir(d); /* also closes dfd */
    return errors;
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int clean_cache(int all)
{
    int errors = 0;

    /* Always clean staging */
    errors += clean_directory(STAGING_DIR);
    if (errors == 0)
        printf("cleaned staging directory\n");
    else
        fprintf(stderr, "clean: errors while cleaning staging\n");

    if (all) {
        int pkg_errors = clean_directory(PACKAGES_DIR);
        if (pkg_errors == 0)
            printf("removed cached packages\n");
        else
            fprintf(stderr, "clean: errors while cleaning package cache\n");
        errors += pkg_errors;
    }

    return errors > 0 ? 1 : 0;
}
/*
 * clean.c - Package cache cleanup
 *
 * flappy clean       : remove staging directory contents only
 * flappy clean --all : remove staging + cached package files
 *
 * Staging dir : /var/cache/flappy/staging/
 * Package cache: /var/cache/flappy/packages/
 *
 * Exit codes:
 *   0 - success
 *   1 - one or more files could not be removed
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

#define STAGING_DIR  "/var/cache/flappy/staging"
#define PACKAGES_DIR "/var/cache/flappy/packages"

/* =========================================================================
 * clean_directory
 *
 * Removes all entries inside `dir` (non-recursive for files,
 * uses rm -rf for subdirectories to handle staging leftovers).
 * Returns count of errors.
 * ========================================================================= */

static int clean_directory(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) {
        if (errno == ENOENT)
            return 0; /* nothing to clean */
        fprintf(stderr, "clean: cannot open %s: %s\n",
                dir, strerror(errno));
        return 1;
    }

    struct dirent *ent;
    int errors = 0;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;

        char path[PATH_MAX];
        int n = snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        if (n < 0 || n >= (int)sizeof(path)) {
            errors++;
            continue;
        }

        struct stat st;
        if (lstat(path, &st) != 0) {
            errors++;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /* staging dirs are subdirectories — remove recursively */
            char cmd[PATH_MAX + 16];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
            if (system(cmd) != 0) {
                fprintf(stderr, "clean: failed to remove %s\n", path);
                errors++;
            }
        } else {
            if (unlink(path) != 0) {
                fprintf(stderr, "clean: cannot remove %s: %s\n",
                        path, strerror(errno));
                errors++;
            }
        }
    }

    closedir(d);
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
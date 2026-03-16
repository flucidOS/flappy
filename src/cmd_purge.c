/*
 * cmd_purge.c - CLI handler for `flappy purge`
 *
 * Full removal: removes all files including /etc configs.
 * Refuses if reverse dependencies exist.
 *
 * With --force: bypasses reverse-dep check,
 * logs a warning, and removes anyway.
 *
 * Usage:
 *   flappy purge <package>
 *   flappy purge --force <package>
 */

#include "flappy.h"
#include "remove.h"

#include <stdio.h>
#include <string.h>

int cmd_purge(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: flappy purge [--force] <package>\n");
        return 2;
    }

    int force = 0;
    const char *pkgname = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0)
            force = 1;
        else
            pkgname = argv[i];
    }

    if (!pkgname) {
        fprintf(stderr, "usage: flappy purge [--force] <package>\n");
        return 2;
    }

    db_open_or_die();
    int rc = purge_package(pkgname, force);
    db_close();

    return rc;
}
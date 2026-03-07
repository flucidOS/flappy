/*
 * cmd_install.c - CLI entry for `flappy install`
 *
 * Responsibilities:
 *   - Validate CLI arguments
 *   - Pass package name to installer
 *   - Handle user-level errors
 */

#include "flappy.h"
#include "install.h"

#include <stdio.h>

int cmd_install(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: flappy install <package>\n");
        return 2;
    }

    const char *pkgname = argv[0];

    return install_package(pkgname);
}
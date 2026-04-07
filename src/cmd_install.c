/*
 * cmd_install.c - CLI entry for `flappy install`
 *
 * Routes through resolve_and_install() which:
 *   1. Computes the full transitive dependency closure from repo.db
 *   2. Filters out already-installed packages
 *   3. Installs the remainder in dependency-first topological order
 *
 * Each package in the resolved list goes through the standard pipeline:
 *   guard → lookup → download → verify → extract → conflict → commit
 *
 * All atomicity and integrity guarantees are preserved per-package.
 * If any package in the chain fails, installation stops and the
 * remaining packages are not attempted.
 */

#include "flappy.h"
#include "resolve.h"

#include <stdio.h>

int cmd_install(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: flappy install <package>\n");
        return 2;
    }

    return resolve_and_install(argv[0]);
}
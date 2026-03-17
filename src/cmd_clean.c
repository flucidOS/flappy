/*
 * cmd_clean.c - CLI handler for `flappy clean`
 *
 * flappy clean       : remove staging directory only
 * flappy clean --all : remove staging + package cache
 *
 * Usage:
 *   flappy clean [--all]
 *
 * Exit codes:
 *   0 - success
 *   1 - failure
 */

#include "flappy.h"
#include "maintenance.h"

#include <stdio.h>
#include <string.h>

int cmd_clean(int argc, char **argv)
{
    int all = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0)
            all = 1;
    }

    return clean_cache(all);
}
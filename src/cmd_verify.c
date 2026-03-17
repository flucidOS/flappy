/*
 * cmd_verify.c - CLI handler for `flappy verify`
 *
 * Checks every installed file exists on disk.
 * Reports missing or invalid files with owning package.
 * Never mutates anything.
 *
 * Usage:
 *   flappy verify
 *
 * Exit codes:
 *   0 - system consistent
 *   1 - inconsistencies found
 */

#include "flappy.h"
#include "maintenance.h"

int cmd_verify(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    db_open_or_die();
    int rc = verify_system();
    db_close();

    return rc;
}
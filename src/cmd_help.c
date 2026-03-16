/*
 * cmd_help.c - Display help for Flappy
 */

#include "flappy.h"
#include <stdio.h>

int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf(
        "Flappy - Package manager for FlucidOS\n\n"
        "Usage:\n"
        "  flappy <command> [args]\n\n"

        "Core Commands:\n"
        "  help                  Show this help message\n"
        "  version               Show version information\n"
        "  --init-db             Initialize installed database\n\n"

        "Query Commands:\n"
        "  list                  List installed packages\n"
        "  info <pkg>            Show package info\n"
        "  files <pkg>           List package files\n"
        "  owns <path>           Show which package owns a file\n"
        "  inspect <pkg>         Inspect package metadata\n"
        "  depends <pkg>         Show direct dependencies\n"
        "  rdepends <pkg>        Show reverse dependencies\n"
        "  orphans               List unused dependency packages\n\n"

        "Repository Commands:\n"
        "  update                Update repository metadata (root required)\n"
        "  search [term]         Search repository packages\n"
        "  upgrade               Show available upgrades (dry-run)\n\n"

        "Install Commands:\n"
        "  install <pkg>         Install a package\n\n"

        "Removal Commands:\n"
        "  remove <pkg>          Remove package, keep config files\n"
        "  purge <pkg>           Remove package and config files\n"
        "  purge --force <pkg>   Force removal even if dependents exist\n"
        "  autoremove            Remove orphaned dependency packages\n\n"
    );

    return 0;
}
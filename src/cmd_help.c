#include "flappy.h"
#include <stdio.h>

int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;

    printf(
        "Flappy - Package manager for FlucidOS\n\n"
        "Usage:\n"
        "  flappy <command> [args]\n\n"
        "Core:\n"
        "  help\n"
        "  version\n"
        "  --init-db\n\n"
        "Query:\n"
        "  list\n"
        "  info <pkg>\n"
        "  files <pkg>\n"
        "  owns <path>\n"
        "  inspect <pkg>\n"
        "  depends <pkg>\n"
        "  rdepends <pkg>\n"
        "  orphans\n\n"
        "Repository:\n"
        "  update\n"
        "  search [term]\n"
        "  upgrade\n\n"
        "Install:\n"
        "  install <pkg>\n\n"
        "Removal:\n"
        "  remove <pkg>\n"
        "  purge <pkg>\n"
        "  purge --force <pkg>\n"
        "  autoremove\n\n"
        "Maintenance:\n"
        "  verify\n"
        "  clean\n"
        "  clean --all\n\n"
    );
    return 0;
}
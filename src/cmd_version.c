#include "flappy.h"
#include <stdio.h>

int cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("flappy %s\n", FLAPPY_VERSION);
    return 0;
}
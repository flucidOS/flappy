/*
 * install_guard.c - Installation environment checks
 */

#include <unistd.h>
#include <stdio.h>

int install_guard(void)
{
    /* require root */

    if (geteuid() != 0) {
        fprintf(stderr, "install requires root privileges\n");
        return 1;
    }

    return 0;
}
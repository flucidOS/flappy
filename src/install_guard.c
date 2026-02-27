#define _POSIX_C_SOURCE 200809L

#include "install_guard.h"
#include "flappy.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define FLAPPY_CACHE_DIR      "/var/cache/flappy"
#define FLAPPY_CACHE_PKG_DIR  "/var/cache/flappy/pkg"
#define FLAPPY_STAGING_DIR    "/var/cache/flappy/staging"
#define FLAPPY_LOCK_PATH      "/var/lib/flappy/install.lock"

static int lock_fd = -1;

/* -------------------------------------------------------------
 * Require root
 * ------------------------------------------------------------- */

int install_require_root(void)
{
    if (geteuid() != 0) {
        fprintf(stderr, "install requires root privileges\n");
        return 1;
    }

    return 0;
}

/* -------------------------------------------------------------
 * Create directory if missing
 * ------------------------------------------------------------- */

static int mkdir_if_missing(const char *path)
{
    if (mkdir(path, 0755) == -1) {
        if (errno == EEXIST)
            return 0;

        fprintf(stderr, "failed to create %s: %s\n",
                path, strerror(errno));
        return 1;
    }

    return 0;
}

/* -------------------------------------------------------------
 * Prepare required directories
 * ------------------------------------------------------------- */

int install_prepare_directories(void)
{
    if (mkdir_if_missing(FLAPPY_CACHE_DIR))
        return 1;

    if (mkdir_if_missing(FLAPPY_CACHE_PKG_DIR))
        return 1;

    if (mkdir_if_missing(FLAPPY_STAGING_DIR))
        return 1;

    return 0;
}

/* -------------------------------------------------------------
 * Acquire global install lock
 * ------------------------------------------------------------- */

int install_acquire_lock(void)
{
    lock_fd = open(
        FLAPPY_LOCK_PATH,
        O_CREAT | O_EXCL | O_WRONLY,
        0600
    );

    if (lock_fd == -1) {

        if (errno == EEXIST) {
            fprintf(stderr,
                "another install operation is in progress\n");
        } else {
            fprintf(stderr,
                "failed to acquire install lock: %s\n",
                strerror(errno));
        }

        return 1;
    }

    return 0;
}

/* -------------------------------------------------------------
 * Release global install lock
 * ------------------------------------------------------------- */

void install_release_lock(void)
{
    if (lock_fd != -1) {
        close(lock_fd);
        unlink(FLAPPY_LOCK_PATH);
        lock_fd = -1;
    }
}
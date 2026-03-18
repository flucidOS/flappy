#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define FLAPPY_CACHE_DIR "/var/cache/flappy"
#define FLAPPY_DB_DIR    "/var/lib/flappy"

static void ensure_dir(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror(path);
        exit(1);
    }
}

void flappy_env_init(void) {
    ensure_dir("/var/cache");
    ensure_dir("/var/lib");

    ensure_dir(FLAPPY_CACHE_DIR);
    ensure_dir(FLAPPY_DB_DIR);
}
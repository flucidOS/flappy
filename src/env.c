/*
 * env.c - Runtime environment initialisation
 *
 * Creates the directory and file hierarchy flappy requires before
 * log_init() or any other subsystem runs.
 *
 * FIX (issue #14):
 *   Previously flappy_env_init created cache and lib directories but
 *   not the log file.  log_init() opens LOG_PATH in append mode and
 *   calls exit(1) if it fails.  On a clean system where /var/log
 *   exists but flappy.log does not, the very first flappy invocation
 *   would die at log_init with "logging unavailable" — before any
 *   useful diagnostic could be printed.
 *
 *   flappy_env_init now creates the log file (mode 0600, owned by
 *   root) if it does not already exist, matching the permissions set
 *   by `make install`.  If the create fails (e.g. not root), a
 *   diagnostic is printed to stderr and the process exits — the same
 *   behaviour as before, but with a clear message rather than a
 *   confusing "logging unavailable" error.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAPPY_CACHE_DIR "/var/cache/flappy"
#define FLAPPY_DB_DIR    "/var/lib/flappy"
#define FLAPPY_LOG_PATH  "/var/log/flappy.log"

static void ensure_dir(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "flappy: cannot create %s: %s\n",
                path, strerror(errno));
        exit(1);
    }
}

/*
 * ensure_log_file
 *
 * Creates FLAPPY_LOG_PATH if it does not already exist.
 * Sets mode 0600 and ownership root:root to match `make install`.
 * Exits with a diagnostic on failure.
 */
static void ensure_log_file(void) {
    /* O_CREAT | O_EXCL succeeds only if the file does not exist.
     * If it already exists, open with O_RDONLY just to confirm
     * accessibility — log_init will open it in append mode. */
    int fd = open(FLAPPY_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) {
        fprintf(stderr,
                "flappy: cannot create log file %s: %s\n"
                "flappy: run as root or create the file manually\n",
                FLAPPY_LOG_PATH, strerror(errno));
        exit(1);
    }
    close(fd);

    /* Ensure correct ownership and mode even if file pre-existed
     * with wrong permissions (e.g. created by a non-root test run). */
    if (geteuid() == 0) {
        if (chown(FLAPPY_LOG_PATH, 0, 0) != 0)
            fprintf(stderr, "flappy: warning: cannot set ownership on %s: %s\n",
                    FLAPPY_LOG_PATH, strerror(errno));
        if (chmod(FLAPPY_LOG_PATH, 0600) != 0)
            fprintf(stderr, "flappy: warning: cannot set mode on %s: %s\n",
                    FLAPPY_LOG_PATH, strerror(errno));
    }
}

void flappy_env_init(void) {
    ensure_dir("/var/cache");
    ensure_dir("/var/lib");
    ensure_dir("/var/log");

    ensure_dir(FLAPPY_CACHE_DIR);
    ensure_dir(FLAPPY_DB_DIR);

    ensure_log_file();
}
#include "flappy.h"

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define LOG_PATH "/var/log/flappy.log"

/*
 * G_LOG_FP — process-lifetime log file handle.
 *
 * Opened once in log_init() and never closed (the OS reclaims it on
 * process exit).  Previously every log_info/log_error call did
 * fopen + fclose, causing unnecessary syscall overhead and a small
 * window between open and write on each call.
 */
static FILE *G_LOG_FP = NULL;

/*
 * write_log - Write a formatted log message
 */
static void write_log(const char *level, const char *fmt, va_list ap) {
    if (!G_LOG_FP) {
        /* Should not happen after log_init, but guard defensively */
        fprintf(stderr, "Fatal: log not initialised\n");
        exit(1);
    }

    fprintf(G_LOG_FP, "[%s] ", level);
    vfprintf(G_LOG_FP, fmt, ap);
    fprintf(G_LOG_FP, "\n");
    fflush(G_LOG_FP);
}

/*
 * log_init - Open the log file once for the process lifetime.
 *
 * Terminates if the log file cannot be opened.
 */
void log_init(void) {
    G_LOG_FP = fopen(LOG_PATH, "a");
    if (!G_LOG_FP) {
        fprintf(stderr,
                "Fatal: logging unavailable (%s): %s\n",
                LOG_PATH, strerror(errno));
        exit(1);
    }
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    write_log("INFO", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    write_log("ERROR", fmt, ap);
    va_end(ap);
}
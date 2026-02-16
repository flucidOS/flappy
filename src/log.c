#include "flappy.h"

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define LOG_PATH "/var/log/flappy.log"

/*
 * write_log - Write a formatted log message to the log file
 * @level: Log level string (e.g., "INFO", "ERROR")
 * @fmt: Format string for the log message
 * @ap: Variable argument list
 *
 * Appends a timestamped log entry to the log file with the specified level.
 * Terminates the program if the log file cannot be opened.
 */
static void write_log(const char *level, const char *fmt, va_list ap) {
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) {
        fprintf(stderr,
                "Fatal: cannot open log file %s: %s\n",
                LOG_PATH, strerror(errno));
        exit(1);
    }

    fprintf(fp, "[%s] ", level);
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");

    fclose(fp);
}

/*
 * log_init - Initialize the logging system
 *
 * Verifies that the log file is accessible and can be written to.
 * Terminates the program if logging cannot be initialized.
 */
void log_init(void) {
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) {
        fprintf(stderr,
                "Fatal: logging unavailable (%s): %s\n",
                LOG_PATH, strerror(errno));
        exit(1);
    }
    fclose(fp);
}

/*
 * log_info - Log an informational message
 * @fmt: Format string
 * ...: Variable arguments
 */
void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    write_log("INFO", fmt, ap);
    va_end(ap);
}

/*
 * log_error - Log an error message
 * @fmt: Format string
 * ...: Variable arguments
 */
void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    write_log("ERROR", fmt, ap);
    va_end(ap);
}

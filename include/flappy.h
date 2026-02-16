#ifndef FLAPPY_H
#define FLAPPY_H

#include <stdio.h>

/* =====================
 * Identity
 * ===================== */
#define FLAPPY_VERSION "0.1.0"
#define FLAPPY_TAGLINE "Package manager for FlucidOS"

/* =====================
 * Logging
 * ===================== */
void log_init(void);
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

/* =====================
 * CLI
 * ===================== */
int cli_dispatch(int argc, char **argv);

/* =====================
 * Commands
 * ===================== */
int cmd_help(void);
int cmd_version(void);

#endif /* FLAPPY_H */

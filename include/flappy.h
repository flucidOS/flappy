#ifndef FLAPPY_H
#define FLAPPY_H

#include <stdio.h>
#include <stdarg.h>
#include <sqlite3.h>

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
int cmd_help(int argc, char **argv);
int cmd_version(int argc, char **argv);
int cmd_list(int argc, char **argv);
int cmd_info(int argc, char **argv);
int cmd_files(int argc, char **argv);
int cmd_owns(int argc, char **argv);

/* =====================
 * Database
 * ===================== */
#define FLAPPY_DB_DIR "/var/lib/flappy"
#define FLAPPY_DB_PATH "/var/lib/flappy/flappy.db"
#define FLAPPY_SCHEMA_VERSION 1

/* DB access*/
sqlite3 *db_handle(void);

/* DB bootstrap (install-time) */
int db_bootstrap_install(void);

/* DB runtime (Trail 2.2 will fill these) */
void db_open_or_die(void);
void db_close(void);


#endif /* FLAPPY_H */



/*
 * DB lifecycle invariant:
 * Every flappy invocation opens and closes the database exactly once
 * per command. No persistent DB state is allowed.
 */

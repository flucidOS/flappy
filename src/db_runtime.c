/**
 * db_runtime.c - Database runtime management
 *
 * Handles SQLite database initialization, connection lifecycle, and schema validation.
 * Maintains a global database connection that persists throughout application runtime.
 */

/**
 * db_open_or_die - Open database connection and validate schema version
 *
 * Opens the SQLite database specified by FLAPPY_DB_PATH and validates that the
 * schema version matches the expected FLAPPY_SCHEMA_VERSION. If any step fails
 * or schema mismatch is detected, logs an error and terminates the application.
 *
 * Fatal errors trigger immediate application exit with exit code 1.
 * Errors include: database open failure, schema metadata query failure,
 * missing meta table, or schema version mismatch.
 *
 * Returns: void (terminates on error)
 */

/**
 * db_close - Close and cleanup database connection
 *
 * Safely closes the global SQLite database connection and clears the reference.
 * Safe to call when database is already closed or never opened.
 *
 * Returns: void
 */
#include "flappy.h"
#include "db_guard.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

/* PRIVATE: not visible outside this file */
static sqlite3 *G_DB = NULL;

sqlite3 *db_handle(void) {
    return G_DB;
}

void db_open_or_die(void) {
    int rc = sqlite3_open(FLAPPY_DB_PATH, &G_DB);
    if (rc != SQLITE_OK)
        db_die(G_DB, rc, "open");

    sqlite3_stmt *st = NULL;
    rc = sqlite3_prepare_v2(
        G_DB,
        "SELECT schema_version FROM meta LIMIT 1;",
        -1, &st, NULL
    );
    if (rc != SQLITE_OK)
        db_die(G_DB, rc, "meta prepare");

    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW)
        db_die(G_DB, rc, "meta missing");

    int v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);

    if (v != FLAPPY_SCHEMA_VERSION) {
        log_error("schema version mismatch: got=%d expected=%d",
                  v, FLAPPY_SCHEMA_VERSION);
        fprintf(stderr, "Fatal: database schema mismatch\n");
        sqlite3_close(G_DB);
        exit(1);
    }
}

void db_close(void) {
    if (G_DB) sqlite3_close(G_DB);
    G_DB = NULL;
}


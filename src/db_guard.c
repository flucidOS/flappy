#include "db_guard.h"
#include "flappy.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * db_die - Handle fatal database errors and terminate the program
 * @db: SQLite database handle (may be NULL)
 * @rc: SQLite return code indicating the error
 * @ctx: Context string describing where the error occurred
 *
 * Logs the error details and safely closes the database before exiting.
 * If no database handle is available, uses a default error message.
 */
void db_die(sqlite3 *db, int rc, const char *ctx) {
    /* Get error message from SQLite or use fallback if db handle is unavailable */
    const char *msg = db ? sqlite3_errmsg(db) : "no db handle";
    
    /* Log the error with context, return code, and SQLite error message */
    log_error("sqlite fatal (%s): rc=%d msg=%s", ctx, rc, msg);
    
    /* Print user-friendly error message to stderr */
    fprintf(stderr, "Fatal: database error (%s)\n", ctx);
    
    /* Close database if handle exists, then exit with error status */
    if (db) sqlite3_close(db);
    exit(1);
}

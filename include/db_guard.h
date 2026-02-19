#ifndef DB_GUARD_H
#define DB_GUARD_H

#include <sqlite3.h>
/**
 * @file db_guard.h
 * @brief Database error handling and guard utilities for SQLite3
 * 
 * This header file provides error handling functions for SQLite3 database
 * operations, ensuring proper error reporting and context management.
 */

/**
 * @brief Handles SQLite3 error conditions and terminates execution if needed
 * 
 * Evaluates the SQLite3 return code and reports any errors with context information.
 * If an error condition is detected, logs the error message and terminates the program.
 * 
 * @param db        Pointer to the SQLite3 database connection
 * @param rc        SQLite3 return code to evaluate (SQLITE_OK indicates success)
 * @param ctx       Context string describing the operation that was attempted,
 *                  used for error reporting to help identify the failure source
 * 
 * @note This function may terminate the program if a critical error is detected
 * @see https://www.sqlite.org/rescode.html for SQLite3 return codes
 */
void db_die(sqlite3 *db, int rc, const char *ctx);

#endif

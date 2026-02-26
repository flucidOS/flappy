/*
 * repo_search.c - Repository search engine (Trail-5)
 *
 * Responsibilities:
 *   - Open repo.db read-only
 *   - Normalize input to lowercase
 *   - Perform deterministic prefix search
 *   - Print results in BINARY alphabetical order
 *
 * Design guarantees:
 *   - No root required
 *   - No mutation of repo DB
 *   - Prefix-only search (term%)
 *   - Deterministic ORDER BY name COLLATE BINARY
 *   - Fail hard if repo missing
 *
 * This module does NOT:
 *   - Interpret dependency metadata
 *   - Perform fuzzy search
 *   - Perform substring search
 */

#define _POSIX_C_SOURCE 200809L

#include "repo.h"
#include "flappy.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* =========================================================================
 * Utility: Normalize Input to Lowercase
 * ========================================================================= */

static void normalize_lower(char *s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

/* =========================================================================
 * Public Entry: repo_search
 * ========================================================================= */

int repo_search(const char *term)
{
    /* Ensure repository database exists */
    if (access(FLAPPY_REPO_DB_PATH, R_OK) != 0) {
        fprintf(stderr,
                "repository metadata not available (run 'flappy update')\n");
        return 1;
    }

    sqlite3 *db = NULL;

    if (sqlite3_open_v2(
            FLAPPY_REPO_DB_PATH,
            &db,
            SQLITE_OPEN_READONLY,
            NULL) != SQLITE_OK) {

        fprintf(stderr, "failed to open repository database\n");
        if (db)
            sqlite3_close(db);
        return 1;
    }

    sqlite3_stmt *st = NULL;
    int rc;

    /* -------------------------------------------------------------
     * Case 1: No search term → list all packages
     * ------------------------------------------------------------- */

    if (!term || *term == '\0') {

        rc = sqlite3_prepare_v2(
            db,
            "SELECT name, version "
            "FROM packages "
            "ORDER BY name COLLATE BINARY;",
            -1, &st, NULL
        );

        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            return 1;
        }
    }
    else {

        /* Strict lowercase normalization */
        char *pattern = strdup(term);
        if (!pattern) {
            sqlite3_close(db);
            return 1;
        }

        normalize_lower(pattern);

        /* Build prefix pattern: term% */
        size_t len = strlen(pattern);

        char *like = malloc(len + 2);
        if (!like) {
            free(pattern);
            sqlite3_close(db);
            return 1;
        }

        memcpy(like, pattern, len);
        like[len] = '%';
        like[len + 1] = '\0';

        rc = sqlite3_prepare_v2(
            db,
            "SELECT name, version "
            "FROM packages "
            "WHERE name LIKE ? "
            "ORDER BY name COLLATE BINARY;",
            -1, &st, NULL
        );

        if (rc != SQLITE_OK) {
            free(pattern);
            free(like);
            sqlite3_close(db);
            return 1;
        }

        sqlite3_bind_text(st, 1, like, -1, SQLITE_TRANSIENT);

        free(pattern);
        free(like);
    }

    /* -------------------------------------------------------------
     * Execute query
     * ------------------------------------------------------------- */

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {

        const unsigned char *name =
            sqlite3_column_text(st, 0);

        const unsigned char *version =
            sqlite3_column_text(st, 1);

        if (name && version)
            printf("%s %s\n", name, version);
    }

    if (rc != SQLITE_DONE) {
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_finalize(st);
    sqlite3_close(db);

    return 0;
}
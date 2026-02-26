/*
 * repo_upgrade.c - Upgrade detection engine (Trail-5)
 *
 * Responsibilities:
 *   - Compare installed packages vs repository versions
 *   - Print available upgrades (dry-run only)
 *
 * Design guarantees:
 *   - No root required
 *   - No modification of installed DB
 *   - No resolver logic
 *   - Deterministic alphabetical ordering
 *
 * This module does NOT:
 *   - Perform installations
 *   - Resolve dependencies
 *   - Interpret version constraints
 */

#define _POSIX_C_SOURCE 200809L

#include "repo.h"
#include "flappy.h"
#include "version.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* =========================================================================
 * repo_upgrade
 * ========================================================================= */

int repo_upgrade(void)
{
    /* Ensure repository metadata exists */
    if (access(FLAPPY_REPO_DB_PATH, R_OK) != 0) {
        fprintf(stderr,
                "repository metadata not available (run 'flappy update')\n");
        return 1;
    }

    /* Ensure installed database exists */
    if (access(FLAPPY_DB_PATH, R_OK) != 0) {
        fprintf(stderr,
                "installed database not found\n");
        return 1;
    }

    sqlite3 *installed_db = NULL;
    sqlite3 *repo_db = NULL;

    /* Open installed DB (read-only) */
    if (sqlite3_open_v2(
            FLAPPY_DB_PATH,
            &installed_db,
            SQLITE_OPEN_READONLY,
            NULL) != SQLITE_OK) {

        fprintf(stderr, "failed to open installed database\n");
        if (installed_db)
            sqlite3_close(installed_db);
        return 1;
    }

    /* Open repository DB (read-only) */
    if (sqlite3_open_v2(
            FLAPPY_REPO_DB_PATH,
            &repo_db,
            SQLITE_OPEN_READONLY,
            NULL) != SQLITE_OK) {

        fprintf(stderr, "failed to open repository database\n");
        sqlite3_close(installed_db);
        if (repo_db)
            sqlite3_close(repo_db);
        return 1;
    }

    /* Prepare installed packages query (deterministic order) */
    sqlite3_stmt *st_inst = NULL;

    int rc = sqlite3_prepare_v2(
        installed_db,
        "SELECT name, version "
        "FROM packages "
        "ORDER BY name COLLATE BINARY;",
        -1, &st_inst, NULL
    );

    if (rc != SQLITE_OK) {
        sqlite3_close(installed_db);
        sqlite3_close(repo_db);
        return 1;
    }

    /* Prepare repository lookup statement */
    sqlite3_stmt *st_repo = NULL;

    rc = sqlite3_prepare_v2(
        repo_db,
        "SELECT version FROM packages WHERE name = ?;",
        -1, &st_repo, NULL
    );

    if (rc != SQLITE_OK) {
        sqlite3_finalize(st_inst);
        sqlite3_close(installed_db);
        sqlite3_close(repo_db);
        return 1;
    }

    // int upgrades_found = 0;

    /* Iterate installed packages */
    while ((rc = sqlite3_step(st_inst)) == SQLITE_ROW) {

        const char *name =
            (const char *)sqlite3_column_text(st_inst, 0);

        const char *installed_version =
            (const char *)sqlite3_column_text(st_inst, 1);

        if (!name || !installed_version)
            continue;

        /* Reset and bind repo lookup */
        sqlite3_reset(st_repo);
        sqlite3_clear_bindings(st_repo);

        sqlite3_bind_text(
            st_repo,
            1,
            name,
            -1,
            SQLITE_STATIC
        );

        char *best_version = NULL;

        /* Determine highest available repo version */
        while (sqlite3_step(st_repo) == SQLITE_ROW) {

            const char *repo_version =
                (const char *)sqlite3_column_text(st_repo, 0);

            if (!repo_version)
                continue;

            if (!version_is_valid(repo_version))
                continue;

            if (!best_version) {
                best_version = strdup(repo_version);
                if (!best_version)
                    break;
                continue;
            }

            if (version_cmp(repo_version, best_version) > 0) {
                char *tmp = strdup(repo_version);
                if (!tmp)
                    break;

                free(best_version);
                best_version = tmp;
            }
        }

        /* Compare best repo version with installed version */
        if (best_version &&
            version_cmp(best_version, installed_version) > 0) {

            printf("%s %s -> %s\n",
                   name,
                   installed_version,
                   best_version);

            // upgrades_found = 1;
        }

        if (best_version)
            free(best_version);
    }

    if (rc != SQLITE_DONE) {
        sqlite3_finalize(st_repo);
        sqlite3_finalize(st_inst);
        sqlite3_close(installed_db);
        sqlite3_close(repo_db);
        return 1;
    }

    // if (!upgrades_found) {
    //     printf("System is up to date.\n");
    // }

    sqlite3_finalize(st_repo);
    sqlite3_finalize(st_inst);
    sqlite3_close(installed_db);
    sqlite3_close(repo_db);

    return 0;
}   
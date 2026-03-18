/*
 * repo_upgrade.c - Upgrade detection (planner, not executor)
 *
 * UX contract:
 *   [INFO] checking for updates...
 *
 *   Packages to upgrade:
 *
 *     curl      8.4.0 -> 8.5.0
 *     openssl   3.0.0 -> 3.1.0
 *
 *   Summary:
 *     total packages: 2
 *
 *   Run 'flappy install <pkg>' to upgrade packages
 *
 *   No updates:
 *   [INFO] system is up to date
 *
 *   Repo missing:
 *   [ERROR] repository not available (run update first)
 */

#define _POSIX_C_SOURCE 200809L

#include "repo.h"
#include "flappy.h"
#include "version.h"
#include "ui.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int repo_upgrade(void)
{
    if (access(FLAPPY_REPO_DB_PATH, R_OK) != 0) {
        ui_error("repository not available (run update first)");
        return 1;
    }

    if (access(FLAPPY_DB_PATH, R_OK) != 0) {
        ui_error("installed database not found");
        return 1;
    }

    ui_info("checking for updates...");

    sqlite3 *installed_db = NULL;
    sqlite3 *repo_db = NULL;

    if (sqlite3_open_v2(FLAPPY_DB_PATH, &installed_db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        ui_error("failed to open installed database");
        if (installed_db) sqlite3_close(installed_db);
        return 1;
    }

    if (sqlite3_open_v2(FLAPPY_REPO_DB_PATH, &repo_db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        ui_error("failed to open repository database");
        sqlite3_close(installed_db);
        if (repo_db) sqlite3_close(repo_db);
        return 1;
    }

    sqlite3_stmt *st_inst = NULL;
    int rc = sqlite3_prepare_v2(installed_db,
        "SELECT name, version FROM packages ORDER BY name COLLATE BINARY;",
        -1, &st_inst, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(installed_db);
        sqlite3_close(repo_db);
        return 1;
    }

    sqlite3_stmt *st_repo = NULL;
    rc = sqlite3_prepare_v2(repo_db,
        "SELECT version FROM packages WHERE name = ?;",
        -1, &st_repo, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(st_inst);
        sqlite3_close(installed_db);
        sqlite3_close(repo_db);
        return 1;
    }

    /* Collect upgrades first so we can print the header once */
    typedef struct { char *name; char *from; char *to; } Upgrade;
    Upgrade *upgrades = NULL;
    int upgrade_count = 0;
    int upgrade_cap   = 0;

    while ((rc = sqlite3_step(st_inst)) == SQLITE_ROW) {
        const char *name    = (const char *)sqlite3_column_text(st_inst, 0);
        const char *iv      = (const char *)sqlite3_column_text(st_inst, 1);
        if (!name || !iv) continue;

        sqlite3_reset(st_repo);
        sqlite3_clear_bindings(st_repo);
        sqlite3_bind_text(st_repo, 1, name, -1, SQLITE_STATIC);

        char *best = NULL;
        while (sqlite3_step(st_repo) == SQLITE_ROW) {
            const char *rv = (const char *)sqlite3_column_text(st_repo, 0);
            if (!rv || !version_is_valid(rv)) continue;
            if (!best) { best = strdup(rv); continue; }
            if (version_cmp(rv, best) > 0) {
                char *tmp = strdup(rv);
                free(best);
                best = tmp;
            }
        }

        if (best && version_cmp(best, iv) > 0) {
            if (upgrade_count >= upgrade_cap) {
                int nc = upgrade_cap ? upgrade_cap * 2 : 8;
                Upgrade *tmp = realloc(upgrades, nc * sizeof(Upgrade));
                if (!tmp) { free(best); break; }
                upgrades = tmp;
                upgrade_cap = nc;
            }
            upgrades[upgrade_count].name = strdup(name);
            upgrades[upgrade_count].from = strdup(iv);
            upgrades[upgrade_count].to   = best;
            upgrade_count++;
        } else {
            free(best);
        }
    }

    sqlite3_finalize(st_repo);
    sqlite3_finalize(st_inst);
    sqlite3_close(installed_db);
    sqlite3_close(repo_db);

    if (upgrade_count == 0) {
        ui_info("system is up to date");
        free(upgrades);
        return 0;
    }

    fprintf(stdout, "\nPackages to upgrade:\n\n");
    for (int i = 0; i < upgrade_count; i++) {
        fprintf(stdout, "  %-16s %s -> %s\n",
                upgrades[i].name,
                upgrades[i].from,
                upgrades[i].to);
        free(upgrades[i].name);
        free(upgrades[i].from);
        free(upgrades[i].to);
    }
    free(upgrades);

    fprintf(stdout, "\nSummary:\n");
    fprintf(stdout, "  total packages: %d\n", upgrade_count);
    fprintf(stdout, "\nRun 'flappy install <pkg>' to upgrade packages\n");

    return 0;
}
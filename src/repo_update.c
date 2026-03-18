/*
 * repo_update.c - Repository metadata updater
 *
 * UX contract:
 *   [INFO] updating repository metadata...
 *   downloading repo.db
 *   [progress bar]
 *   ✔ download complete
 *   verifying repository...
 *   ✔ verified
 *   [INFO] repository updated
 *
 *   Failure:
 *   [ERROR] failed to download repo.db
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "version.h"
#include "repo.h"
#include "ui.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int run_cmd(const char *cmd)
{
    return system(cmd) != 0 ? 1 : 0;
}

static int download_file(const char *url, const char *out_path)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -fsSL \"%s\" -o \"%s\"", url, out_path);
    return run_cmd(cmd);
}

static int compute_sha256(const char *path, char out[65])
{
    char cmd[1024];
    FILE *fp;
    snprintf(cmd, sizeof(cmd), "sha256sum \"%s\"", path);
    fp = popen(cmd, "r");
    if (!fp) return 1;
    if (fscanf(fp, "%64s", out) != 1) { pclose(fp); return 1; }
    pclose(fp);
    return 0;
}

static int read_sha_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    if (fscanf(f, "%64s", out) != 1) { fclose(f); return 1; }
    fclose(f);
    return 0;
}

static int validate_repo_schema(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT value FROM meta WHERE key='schema_version';",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return 1;

    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) { sqlite3_finalize(st); return 1; }

    int version = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return version != FLAPPY_REPO_SCHEMA_VERSION ? 1 : 0;
}

static int validate_repo_packages(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT name, version FROM packages;",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return 1;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *name    = (const char *)sqlite3_column_text(st, 0);
        const char *version = (const char *)sqlite3_column_text(st, 1);
        if (!name || !*name) { sqlite3_finalize(st); return 1; }
        if (!version_is_valid(version)) { sqlite3_finalize(st); return 1; }
    }
    sqlite3_finalize(st);
    return 0;
}

int repo_update(const char *base_url)
{
    char url_db[1024], url_sha[1024];

    snprintf(url_db,  sizeof(url_db),  "%s/repo.db",        base_url);
    snprintf(url_sha, sizeof(url_sha), "%s/repo.db.sha256",  base_url);

    ui_info("updating repository metadata...");

    ui_step("downloading repo.db");
    if (download_file(url_db, FLAPPY_REPO_TMP_PATH)) {
        ui_error("failed to download repo.db");
        return 1;
    }
    ui_ok("download complete");

    if (download_file(url_sha, FLAPPY_REPO_SHA_PATH ".tmp")) {
        unlink(FLAPPY_REPO_TMP_PATH);
        ui_error("failed to download checksum");
        return 1;
    }

    ui_step("verifying repository...");

    char expected[65], actual[65];

    if (read_sha_file(FLAPPY_REPO_SHA_PATH ".tmp", expected)) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("cannot read checksum file");
        return 1;
    }

    if (compute_sha256(FLAPPY_REPO_TMP_PATH, actual)) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("cannot compute checksum");
        return 1;
    }

    if (strcmp(expected, actual) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("checksum mismatch — repository may be corrupt");
        return 1;
    }

    sqlite3 *repo_db = NULL;
    if (sqlite3_open(FLAPPY_REPO_TMP_PATH, &repo_db) != SQLITE_OK) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("cannot open downloaded repository database");
        return 1;
    }

    if (validate_repo_schema(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("invalid repository schema");
        return 1;
    }

    if (validate_repo_packages(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("invalid repository metadata");
        return 1;
    }

    sqlite3_close(repo_db);

    if (rename(FLAPPY_REPO_TMP_PATH, FLAPPY_REPO_DB_PATH) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        ui_error("failed to install repository database");
        return 1;
    }

    rename(FLAPPY_REPO_SHA_PATH ".tmp", FLAPPY_REPO_SHA_PATH);

    ui_ok("verified");
    ui_info("repository updated");
    return 0;
}
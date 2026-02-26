/*
 * repo_update.c - Repository metadata updater
 *
 * Responsibilities:
 *   - Download repo.db and repo.db.sha256
 *   - Verify SHA256 checksum
 *   - Validate schema version
 *   - Validate package metadata
 *   - Atomically replace existing repo database
 *
 * Security model:
 *   - Trust GitHub transport (HTTPS)
 *   - Verify integrity using sha256sum
 *   - Fail hard on any structural violation
 *
 * Design goal:
 *   Minimal maintenance.
 *   Deterministic behavior.
 *   No external crypto libraries.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "version.h"
#include "repo.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------
 * Utility: run shell command safely
 * ------------------------------------------------------------- */

static int run_cmd(const char *cmd)
{
    int rc = system(cmd);
    if (rc != 0)
        return 1;
    return 0;
}

/* -------------------------------------------------------------
 * Download file using curl
 * ------------------------------------------------------------- */

static int download_file(const char *url, const char *out_path)
{
    char cmd[1024];

    snprintf(cmd, sizeof(cmd),
             "curl -fsSL \"%s\" -o \"%s\"",
             url, out_path);

    return run_cmd(cmd);
}

/* -------------------------------------------------------------
 * Compute SHA256 using system sha256sum
 * ------------------------------------------------------------- */

static int compute_sha256(const char *path, char out[65])
{
    char cmd[1024];
    FILE *fp;

    snprintf(cmd, sizeof(cmd),
             "sha256sum \"%s\"", path);

    fp = popen(cmd, "r");
    if (!fp)
        return 1;

    if (fscanf(fp, "%64s", out) != 1) {
        pclose(fp);
        return 1;
    }

    pclose(fp);
    return 0;
}

/* -------------------------------------------------------------
 * Read expected SHA file
 * ------------------------------------------------------------- */

static int read_sha_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 1;

    if (fscanf(f, "%64s", out) != 1) {
        fclose(f);
        return 1;
    }

    fclose(f);
    return 0;
}

/* -------------------------------------------------------------
 * Validate repo schema version
 * ------------------------------------------------------------- */

static int validate_repo_schema(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        db,
        "SELECT value FROM meta WHERE key='schema_version';",
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        return 1;

    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        return 1;
    }

    int version = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);

    if (version != FLAPPY_REPO_SCHEMA_VERSION)
        return 1;

    return 0;
}

/* -------------------------------------------------------------
 * Validate repo packages
 * ------------------------------------------------------------- */

static int validate_repo_packages(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        db,
        "SELECT name, version FROM packages;",
        -1, &st, NULL);

    if (rc != SQLITE_OK)
        return 1;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {

        const char *name =
            (const char *)sqlite3_column_text(st, 0);

        const char *version =
            (const char *)sqlite3_column_text(st, 1);

        if (!name || !*name) {
            sqlite3_finalize(st);
            return 1;
        }

        if (!version_is_valid(version)) {
            sqlite3_finalize(st);
            return 1;
        }
    }

    sqlite3_finalize(st);
    return 0;
}

/* -------------------------------------------------------------
 * repo_update - Public entry point
 * ------------------------------------------------------------- */

int repo_update(const char *base_url)
{
    char url_db[1024];
    char url_sha[1024];

    snprintf(url_db, sizeof(url_db),
             "%s/repo.db", base_url);

    snprintf(url_sha, sizeof(url_sha),
             "%s/repo.db.sha256", base_url);

    printf("Updating repository metadata...\n");

    /* Download repo.db */
    if (download_file(url_db, FLAPPY_REPO_TMP_PATH)) {
        fprintf(stderr, "Failed to download repo.db\n");
        return 1;
    }

    /* Download SHA */
    if (download_file(url_sha, FLAPPY_REPO_SHA_PATH ".tmp")) {
        unlink(FLAPPY_REPO_TMP_PATH);
        fprintf(stderr, "Failed to download checksum\n");
        return 1;
    }

    /* Verify SHA */
    char expected[65];
    char actual[65];

    if (read_sha_file(FLAPPY_REPO_SHA_PATH ".tmp", expected)) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        return 1;
    }

    if (compute_sha256(FLAPPY_REPO_TMP_PATH, actual)) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        return 1;
    }

    if (strcmp(expected, actual) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        fprintf(stderr, "Checksum mismatch\n");
        return 1;
    }

    /* Open repo DB */
    sqlite3 *repo_db = NULL;

    if (sqlite3_open(FLAPPY_REPO_TMP_PATH, &repo_db) != SQLITE_OK) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        return 1;
    }

    /* Validate schema */
    if (validate_repo_schema(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        fprintf(stderr, "Invalid repo schema\n");
        return 1;
    }

    /* Validate package metadata */
    if (validate_repo_packages(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        fprintf(stderr, "Invalid repo metadata\n");
        return 1;
    }

    sqlite3_close(repo_db);

    /* Atomic replace */
    if (rename(FLAPPY_REPO_TMP_PATH, FLAPPY_REPO_DB_PATH) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(FLAPPY_REPO_SHA_PATH ".tmp");
        return 1;
    }

    rename(FLAPPY_REPO_SHA_PATH ".tmp", FLAPPY_REPO_SHA_PATH);

    printf("Repository updated successfully.\n");
    return 0;
}
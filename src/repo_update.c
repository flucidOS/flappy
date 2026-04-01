/*
 * repo_update.c - Repository metadata updater
 *
 * CHANGES from original:
 *
 *   1. download_file now uses libcurl directly (same path as
 *      install_download.c) instead of system("curl -fsSL ...").
 *      This eliminates the shell-injection surface and makes the
 *      download consistent with how packages are fetched.
 *
 *   2. SHA256 verification now calls sha256_file() from sha256.c
 *      instead of popen("sha256sum ...").  This means:
 *        - no dependency on sha256sum being present on the target
 *        - no PATH lookup for an external binary
 *        - identical EVP implementation used for both repo and
 *          package integrity checks
 *        - no risk of fscanf silently accepting a truncated hash
 *
 *   3. compute_sha256 and read_sha_file helpers are removed; their
 *      roles are now filled by sha256_file() and a simple fread.
 *
 * UX contract (unchanged):
 *   [INFO] updating repository metadata...
 *   downloading repo.db
 *   [progress bar]
 *   ✔ download complete
 *   verifying repository...
 *   ✔ verified
 *   [INFO] repository updated
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "version.h"
#include "repo.h"
#include "sha256.h"
#include "ui.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* =========================================================================
 * libcurl download helper
 *
 * Downloads `url` to the file at `out_path`.
 * Shows a progress bar when stdout is a TTY (matches install_download.c).
 * Returns 0 on success, 1 on failure.
 * ========================================================================= */

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

/*
 * download_file_ex
 *
 * Core download helper.
 * show_progress=1 : hooks up ui_curl_progress_cb when stdout is a TTY.
 * show_progress=0 : always silent (used for small sidecar files).
 */
static int download_file_ex(const char *url, const char *out_path,
                            int show_progress)
{
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        ui_error("cannot create %s: %s", out_path, strerror(errno));
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(f);
        ui_error("curl init failed");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      f);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR,    1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (show_progress && ui_is_tty()) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ui_curl_progress_cb);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     NULL);
    } else {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        if (show_progress && ui_is_tty())
            fprintf(stderr, "\n");
        ui_error("download failed: %s", curl_easy_strerror(res));
        unlink(out_path);
        return 1;
    }

    return 0;
}

/* Convenience wrappers */
static int download_file(const char *url, const char *out_path)
{
    return download_file_ex(url, out_path, 1);
}

static int download_file_silent(const char *url, const char *out_path)
{
    return download_file_ex(url, out_path, 0);
}

/* =========================================================================
 * Read expected SHA256 from a .sha256 sidecar file
 *
 * The sidecar contains exactly 64 hex characters followed by
 * optional whitespace / filename (sha256sum output format).
 * We read exactly 64 characters and verify they are all hex digits.
 *
 * Returns 0 on success, 1 on failure.
 * ========================================================================= */

static int read_sha_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ui_error("cannot open checksum file %s: %s", path, strerror(errno));
        return 1;
    }

    /* Read exactly 64 characters */
    size_t n = fread(out, 1, 64, f);
    fclose(f);

    if (n != 64) {
        ui_error("checksum file too short (got %zu bytes, expected 64)", n);
        return 1;
    }

    out[64] = '\0';

    /* Validate: must be all lowercase hex */
    for (int i = 0; i < 64; i++) {
        char c = out[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            ui_error("checksum file contains invalid character at position %d", i);
            return 1;
        }
        /* Normalise to lowercase for consistent strcmp */
        if (c >= 'A' && c <= 'F')
            out[i] = (char)(c + 32);
    }

    return 0;
}

/* =========================================================================
 * Repository DB schema / package validation
 * ========================================================================= */

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

/* =========================================================================
 * Public entry
 * ========================================================================= */

int repo_update(const char *base_url)
{
    /* Build URLs — no shell interpolation at any point */
    char url_db[1024], url_sha[1024];
    snprintf(url_db,  sizeof(url_db),  "%s/repo.db",        base_url);
    snprintf(url_sha, sizeof(url_sha), "%s/repo.db.sha256", base_url);

    ui_info("updating repository metadata...");

    /* --- Download repo.db --- */
    ui_progress_init("repo.db");
    if (download_file(url_db, FLAPPY_REPO_TMP_PATH) != 0) {
        ui_error("failed to download repo.db");
        return 1;
    }
    ui_progress_finish();

    /* --- Download checksum sidecar (silent — 65 bytes, no bar needed) --- */
    const char *sha_tmp = FLAPPY_REPO_SHA_PATH ".tmp";

    if (download_file_silent(url_sha, sha_tmp) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        ui_error("failed to download checksum");
        return 1;
    }

    /* --- Verify integrity --- */
    ui_step("verifying repository...");

    char expected[65], actual[65];

    if (read_sha_file(sha_tmp, expected) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        return 1;
    }

    if (sha256_file(FLAPPY_REPO_TMP_PATH, actual) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        return 1;
    }

    if (strcmp(expected, actual) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        ui_error("checksum mismatch — repository may be corrupt\n"
                 "  expected: %s\n"
                 "  actual:   %s", expected, actual);
        return 1;
    }

    ui_ok("verified");

    /* --- Validate schema and package metadata --- */
    sqlite3 *repo_db = NULL;
    if (sqlite3_open(FLAPPY_REPO_TMP_PATH, &repo_db) != SQLITE_OK) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        ui_error("cannot open downloaded repository database");
        return 1;
    }

    if (validate_repo_schema(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        ui_error("invalid repository schema");
        return 1;
    }

    if (validate_repo_packages(repo_db)) {
        sqlite3_close(repo_db);
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        ui_error("invalid repository metadata");
        return 1;
    }

    sqlite3_close(repo_db);

    /* --- Atomically install --- */
    if (rename(FLAPPY_REPO_TMP_PATH, FLAPPY_REPO_DB_PATH) != 0) {
        unlink(FLAPPY_REPO_TMP_PATH);
        unlink(sha_tmp);
        ui_error("failed to install repository database");
        return 1;
    }

    rename(sha_tmp, FLAPPY_REPO_SHA_PATH);

    ui_info("repository updated");
    return 0;
}
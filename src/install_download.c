/*
 * install_download.c - Download with progress bar
 *
 * CACHE HIT BEHAVIOUR (fix for issue #13):
 *
 * Previously, a cached file was accepted purely by existence and
 * non-zero size.  The cache is keyed only by filename, so if the
 * repository is updated and the same filename is reused for a
 * different build, the stale cached file would be used and the
 * subsequent install_verify call would fail with a bare
 * "checksum mismatch" — giving the operator no indication that
 * the fix is `flappy clean --all`.
 *
 * The cache hit path now computes the SHA256 of the cached file
 * and compares it against the expected checksum from repo.db
 * before deciding to skip the download.  On mismatch the cached
 * file is deleted and a fresh download is performed.  The
 * operator sees a clear diagnostic rather than a cryptic failure
 * one step later.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "repo.h"
#include "sha256.h"
#include "ui.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define CACHE_DIR "/var/cache/flappy/packages"

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

static int ensure_cache_dir(void)
{
    if (mkdir(CACHE_DIR, 0755) == -1 && errno != EEXIST) {
        ui_error("cannot create cache dir: %s", strerror(errno));
        return 1;
    }
    return 0;
}

static void read_base_url(char *out, size_t out_size)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *st = NULL;

    strncpy(out, FLAPPY_DEFAULT_REPO_URL, out_size - 1);
    out[out_size - 1] = '\0';

    if (sqlite3_open_v2(FLAPPY_REPO_DB_PATH, &db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        return;

    if (sqlite3_prepare_v2(db,
            "SELECT value FROM meta WHERE key = 'base_url';",
            -1, &st, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }

    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *url = (const char *)sqlite3_column_text(st, 0);
        if (url && *url) {
            strncpy(out, url, out_size - 1);
            out[out_size - 1] = '\0';
        }
    }

    sqlite3_finalize(st);
    sqlite3_close(db);
}

/*
 * do_download
 *
 * Downloads `url` into the file at `local_path`, showing a progress
 * bar on TTY.  Returns 0 on success, 1 on failure.
 */
static int do_download(const char *url, const char *local_path,
                       const char *filename)
{
    FILE *f = fopen(local_path, "wb");
    if (!f) {
        ui_error("cannot open cache file: %s", strerror(errno));
        return 1;
    }

    ui_progress_init(filename);

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

    if (ui_is_tty()) {
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
        fprintf(stderr, "\n");
        ui_error("failed to download %s: %s", filename,
                 curl_easy_strerror(res));
        return 1;
    }

    ui_progress_finish();
    log_info("download: cached %s", local_path);
    return 0;
}

int install_download(const char *filename, char *local_path,
                     const char *expected_checksum)
{
    if (ensure_cache_dir())
        return 1;

    int n = snprintf(local_path, 512, "%s/%s", CACHE_DIR, filename);
    if (n < 0 || n >= 512) {
        ui_error("local path too long");
        return 1;
    }

    char base_url[512];
    read_base_url(base_url, sizeof(base_url));

    size_t blen = strlen(base_url);
    if (blen > 0 && base_url[blen - 1] == '/')
        base_url[blen - 1] = '\0';

    char url[1024];
    n = snprintf(url, sizeof(url), "%s/packages/%s", base_url, filename);
    if (n < 0 || n >= (int)sizeof(url)) {
        ui_error("URL too long");
        return 1;
    }

    /* ---------------------------------------------------------------
     * Cache hit check.
     *
     * A cached file is only reused if its SHA256 matches the expected
     * checksum from repo.db.  On mismatch we delete the stale file,
     * log a clear diagnostic, and fall through to a fresh download.
     *
     * This prevents a silent "checksum mismatch" failure one step
     * later when the repository has been updated but the local cache
     * still holds an old build under the same filename.
     * --------------------------------------------------------------- */
    struct stat cache_st;
    if (stat(local_path, &cache_st) == 0 && cache_st.st_size > 0) {

        char cached_hash[65];
        if (sha256_file(local_path, cached_hash) == 0 &&
                strcmp(cached_hash, expected_checksum) == 0) {
            fprintf(stderr, "downloading %s\n", filename);
            ui_ok("cached");
            log_info("download: using cached %s", local_path);
            return 0;
        }

        /* Stale or corrupt cache entry */
        log_info("download: cached %s failed checksum — re-downloading",
                 local_path);
        ui_warn("cached file is stale or corrupt — re-downloading %s",
                filename);
        if (unlink(local_path) != 0 && errno != ENOENT) {
            ui_error("cannot remove stale cache entry %s: %s",
                     local_path, strerror(errno));
            return 1;
        }
    }

    return do_download(url, local_path, filename);
}
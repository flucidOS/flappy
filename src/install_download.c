/*
 * install_download.c - Download with progress bar
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "repo.h"
#include "ui.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

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

int install_download(const char *filename, char *local_path)
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

    /* If already cached, skip download */
    struct stat cache_st;
    if (stat(local_path, &cache_st) == 0 && cache_st.st_size > 0) {
        fprintf(stderr, "downloading %s\n", filename);
        ui_ok("cached");
        log_info("download: using cached %s", local_path);
        return 0;
    }

    FILE *f = fopen(local_path, "wb");
    if (!f) {
        ui_error("cannot open cache file: %s", strerror(errno));
        return 1;
    }

    /* Init progress bar */
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
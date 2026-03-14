/*
 * install_download.c - Download package file
 *
 * Downloads the package from the repository into the local cache.
 * Creates the cache directory if it does not exist.
 *
 * Cache location : /var/cache/flappy/packages/
 * Package URL    : derived from repo.db filename field,
 *                  fetched from <base_url>/packages/<filename>
 *
 * The base URL is read from repo.db so we know exactly where
 * this repo's packages live.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "repo.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define CACHE_DIR  "/var/cache/flappy/packages"

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

static int ensure_cache_dir(void)
{
    if (mkdir(CACHE_DIR, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "download: cannot create cache dir %s: %s\n",
                CACHE_DIR, strerror(errno));
        return 1;
    }
    return 0;
}

/*
 * read_base_url
 *
 * Reads the base_url from repo.db meta table.
 * Falls back to FLAPPY_DEFAULT_REPO_URL if not set.
 */
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
        fprintf(stderr, "download: local path too long\n");
        return 1;
    }

    char base_url[512];
    read_base_url(base_url, sizeof(base_url));

    /* Strip trailing slash from base_url if present */
    size_t blen = strlen(base_url);
    if (blen > 0 && base_url[blen - 1] == '/')
        base_url[blen - 1] = '\0';

    char url[1024];
    n = snprintf(url, sizeof(url), "%s/packages/%s", base_url, filename);
    if (n < 0 || n >= (int)sizeof(url)) {
        fprintf(stderr, "download: URL too long\n");
        return 1;
    }

    FILE *f = fopen(local_path, "wb");
    if (!f) {
        fprintf(stderr, "download: cannot open %s: %s\n",
                local_path, strerror(errno));
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(f);
        fprintf(stderr, "download: curl init failed\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,   write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,       f);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR,     1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,  1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        fprintf(stderr, "download: failed to fetch %s: %s\n",
                url, curl_easy_strerror(res));
        return 1;
    }

    log_info("download: cached %s", local_path);
    return 0;
}
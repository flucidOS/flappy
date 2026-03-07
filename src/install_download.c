/*
 * install_download.c - Download package file
 */

#include <curl/curl.h>

#include <stdio.h>
#include <string.h>

#define CACHE_DIR "/var/lib/flappy/cache/packages/"
#define REPO_URL  "https://repo.flucid.org/packages/"

static size_t write_data(void *ptr,
                         size_t size,
                         size_t nmemb,
                         FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

int install_download(const char *filename,
                     char *local_path)
{
    char url[512];

    sprintf(local_path,
        "%s%s",
        CACHE_DIR,
        filename);

    sprintf(url,
        "%s%s",
        REPO_URL,
        filename);

    FILE *f = fopen(local_path, "wb");
    if (!f)
        return 1;

    CURL *curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        fprintf(stderr, "download failed\n");
        return 1;
    }

    return 0;
}
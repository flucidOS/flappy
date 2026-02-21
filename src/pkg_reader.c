#define _POSIX_C_SOURCE 200809L

#include "pkg_meta.h"
#include "flappy.h"

#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PKGINFO_SIZE   (64 * 1024)
#define MAX_PKGINFO_LINE   4096
#define READ_CHUNK_SIZE    8192

static int validate_line_lengths(const char *buf);

/*
 * pkg_read_from_file
 *
 * Strict archive structure enforcement:
 *  - Exactly one .PKGINFO at archive root
 *  - Must be regular file
 *  - No nested/shadow .PKGINFO
 *  - No absolute paths
 *  - No path traversal
 */
struct flappy_pkg *pkg_read_from_file(const char *path)
{
    struct archive *a = archive_read_new();
    if (!a) {
        log_error("Failed to allocate archive reader");
        return NULL;
    }

    archive_read_support_format_tar(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, path, 10240) != ARCHIVE_OK) {
        log_error("Cannot open archive: %s", archive_error_string(a));
        archive_read_free(a);
        return NULL;
    }

    struct archive_entry *entry;
    int found = 0;

    char *buffer = NULL;
    size_t total = 0;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

        const char *name = archive_entry_pathname(entry);

        if (!name) {
            log_error("Archive entry has no name");
            archive_read_free(a);
            free(buffer);
            return NULL;
        }

        /* Reject absolute paths */
        if (name[0] == '/') {
            log_error("Archive contains absolute path: %s", name);
            archive_read_free(a);
            free(buffer);
            return NULL;
        }

        /* Reject path traversal */
        if (strstr(name, "..") != NULL) {
            log_error("Archive contains unsafe path: %s", name);
            archive_read_free(a);
            free(buffer);
            return NULL;
        }

        /* Strict metadata enforcement */
        if (strcmp(name, ".PKGINFO") == 0) {

            if (archive_entry_filetype(entry) != AE_IFREG) {
                log_error(".PKGINFO is not a regular file");
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            found++;

            if (found > 1) {
                log_error("Archive contains multiple .PKGINFO entries");
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            char chunk[READ_CHUNK_SIZE];
            ssize_t n;

            while ((n = archive_read_data(a, chunk, sizeof(chunk))) > 0) {

                if (total + (size_t)n > MAX_PKGINFO_SIZE) {
                    log_error(".PKGINFO exceeds maximum allowed size");
                    archive_read_free(a);
                    free(buffer);
                    return NULL;
                }

                char *tmp = realloc(buffer, total + (size_t)n + 1);
                if (!tmp) {
                    log_error("Out of memory while reading .PKGINFO");
                    archive_read_free(a);
                    free(buffer);
                    return NULL;
                }

                buffer = tmp;
                memcpy(buffer + total, chunk, (size_t)n);
                total += (size_t)n;
            }

            if (n < 0) {
                log_error("Error reading .PKGINFO: %s",
                          archive_error_string(a));
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            if (total == 0) {
                log_error(".PKGINFO is empty");
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            buffer[total] = '\0';
        }
        else if (strstr(name, ".PKGINFO") != NULL) {
            /* Any shadow or nested PKGINFO is illegal */
            log_error("Invalid .PKGINFO location: %s", name);
            archive_read_free(a);
            free(buffer);
            return NULL;
        }
        else {
            /* Ignore all other entries */
            archive_read_data_skip(a);
        }
    }

    if (archive_errno(a) != 0) {
        log_error("Archive read error: %s",
                  archive_error_string(a));
        archive_read_free(a);
        free(buffer);
        return NULL;
    }

    archive_read_free(a);

    if (found == 0) {
        log_error("Archive missing required .PKGINFO");
        free(buffer);
        return NULL;
    }

    if (validate_line_lengths(buffer) != 0) {
        free(buffer);
        return NULL;
    }

    struct flappy_pkg *pkg = pkg_parse(buffer, total);
    free(buffer);

    return pkg;
}

/*
 * validate_line_lengths
 *
 * Enforces per-line size cap.
 */
static int validate_line_lengths(const char *buf)
{
    const char *line_start = buf;
    const char *p = buf;

    while (*p) {
        if (*p == '\n') {
            size_t len = (size_t)(p - line_start);
            if (len > MAX_PKGINFO_LINE) {
                log_error("PKGINFO line exceeds maximum length");
                return -1;
            }
            line_start = p + 1;
        }
        p++;
    }

    if (p > line_start) {
        size_t len = (size_t)(p - line_start);
        if (len > MAX_PKGINFO_LINE) {
            log_error("PKGINFO line exceeds maximum length");
            return -1;
        }
    }

    return 0;
}
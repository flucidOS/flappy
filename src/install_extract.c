/*
 * install_extract.c - Extract package into staging directory
 *
 * Responsibilities:
 *   - Create a per-package staging directory under STAGING_BASE
 *   - Validate every archive entry path before writing
 *   - Reject absolute paths, path traversal, and forbidden roots
 *   - Extract files and populate staging_dir (out param)
 *
 * Forbidden install roots:
 *   /proc  /dev  /sys  /home  /root
 *
 * Allowed install roots:
 *   usr/  etc/  var/  opt/
 *
 * staging_dir must be at least 512 bytes.
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"

#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#define STAGING_BASE "/var/cache/flappy/staging"

/* Forbidden top-level path components */
static const char * const FORBIDDEN[] = {
    "proc", "dev", "sys", "home", "root", NULL
};

/* Allowed top-level path components */
static const char * const ALLOWED[] = {
    "usr", "etc", "var", "opt", NULL
};

/* =========================================================================
 * Metadata entry check
 *
 * Returns 1 if this entry should be skipped (metadata or root dir).
 * flappycook stores entries as ./.PKGINFO, ./.FILES, ./ etc.
 * ========================================================================= */

static int is_metadata_entry(const char *path)
{
    return (strcmp(path, "./.PKGINFO") == 0 ||
            strcmp(path, ".PKGINFO")   == 0 ||
            strcmp(path, "./.FILES")   == 0 ||
            strcmp(path, ".FILES")     == 0 ||
            strcmp(path, "./")         == 0 ||
            strcmp(path, ".")          == 0);
}

/* =========================================================================
 * Path validation
 * ========================================================================= */

/*
 * path_is_safe
 *
 * Returns 1 if path is safe to extract, 0 otherwise.
 * Expects path with ./ prefix already stripped.
 *
 * Rules:
 *   - Must not be empty
 *   - Must not be absolute
 *   - Must not contain ".." component
 *   - Must not start with a forbidden root
 *   - Must start with an allowed root
 */
static int path_is_safe(const char *path)
{
    if (!path || *path == '\0')
        return 0;

    /* Reject absolute paths */
    if (path[0] == '/')
        return 0;

    /* Reject path traversal anywhere in the path */
    if (strstr(path, "..") != NULL)
        return 0;

    /* Extract first component */
    char first[64];
    const char *slash = strchr(path, '/');

    if (slash) {
        size_t len = (size_t)(slash - path);
        if (len >= sizeof(first))
            return 0;
        memcpy(first, path, len);
        first[len] = '\0';
    } else {
        /* bare filename at root with no allowed prefix */
        return 0;
    }

    /* Reject forbidden roots */
    for (int i = 0; FORBIDDEN[i]; i++) {
        if (strcmp(first, FORBIDDEN[i]) == 0)
            return 0;
    }

    /* Must match an allowed root */
    for (int i = 0; ALLOWED[i]; i++) {
        if (strcmp(first, ALLOWED[i]) == 0)
            return 1;
    }

    return 0;
}

/* =========================================================================
 * Directory helpers
 * ========================================================================= */

static int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);

    if (len >= sizeof(tmp))
        return -1;

    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int install_extract(const char *pkgfile,
                    char *staging_dir)
{
    /* Create STAGING_BASE if needed */
    if (mkdir_p(STAGING_BASE, 0755) != 0) {
        fprintf(stderr, "extract: cannot create staging base: %s\n",
                strerror(errno));
        return 1;
    }

    /* Build unique staging path: STAGING_BASE/<basename>.stage */
    const char *base = strrchr(pkgfile, '/');
    base = base ? base + 1 : pkgfile;

    int n = snprintf(staging_dir, 512, "%s/%s.stage", STAGING_BASE, base);
    if (n < 0 || n >= 512) {
        fprintf(stderr, "extract: staging path too long\n");
        return 1;
    }

    if (mkdir_p(staging_dir, 0755) != 0) {
        fprintf(stderr, "extract: cannot create staging dir %s: %s\n",
                staging_dir, strerror(errno));
        return 1;
    }

    /* Open archive */
    struct archive *a = archive_read_new();
    if (!a)
        return 1;

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);

    if (archive_read_open_filename(a, pkgfile, 65536) != ARCHIVE_OK) {
        fprintf(stderr, "extract: cannot open archive: %s\n",
                archive_error_string(a));
        archive_read_free(a);
        return 1;
    }

    /* Writer for disk extraction */
    struct archive *disk = archive_write_disk_new();
    if (!disk) {
        archive_read_free(a);
        return 1;
    }

    archive_write_disk_set_options(disk,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_OWNER);

    struct archive_entry *entry;
    int rc = 0;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

        const char *path = archive_entry_pathname(entry);

        if (!path) {
            fprintf(stderr, "extract: entry with no pathname\n");
            rc = 1;
            break;
        }

        /* Skip metadata entries (./.PKGINFO, ./.FILES, ./, etc.) */
        if (is_metadata_entry(path)) {
            archive_read_data_skip(a);
            continue;
        }

        /* Strip leading ./ from tar paths (flappycook convention) */
        if (path[0] == '.' && path[1] == '/')
            path += 2;

        /* Skip empty paths (e.g. directory entries that became empty) */
        if (path[0] == '\0') {
            archive_read_data_skip(a);
            continue;
        }

        /* Validate path */
        if (!path_is_safe(path)) {
            fprintf(stderr, "extract: rejected unsafe path: %s\n", path);
            rc = 1;
            break;
        }

        /* Rewrite path into staging dir */
        char dest[PATH_MAX];
        int written = snprintf(dest, sizeof(dest), "%s/%s",
                               staging_dir, path);

        if (written < 0 || written >= (int)sizeof(dest)) {
            fprintf(stderr, "extract: destination path too long\n");
            rc = 1;
            break;
        }

        archive_entry_set_pathname(entry, dest);

        /* Write header (creates parent directories as needed) */
        if (archive_write_header(disk, entry) != ARCHIVE_OK) {
            fprintf(stderr, "extract: write header failed: %s\n",
                    archive_error_string(disk));
            rc = 1;
            break;
        }

        /* Copy file data */
        const void *block;
        size_t      block_size;
        la_int64_t  offset;

        for (;;) {
            int r = archive_read_data_block(a, &block, &block_size, &offset);
            if (r == ARCHIVE_EOF)
                break;
            if (r != ARCHIVE_OK) {
                fprintf(stderr, "extract: read error: %s\n",
                        archive_error_string(a));
                rc = 1;
                break;
            }
            if (archive_write_data_block(disk, block, block_size, offset)
                    != ARCHIVE_OK) {
                fprintf(stderr, "extract: write error: %s\n",
                        archive_error_string(disk));
                rc = 1;
                break;
            }
        }

        if (rc)
            break;

        archive_write_finish_entry(disk);
    }

    archive_read_free(a);
    archive_write_free(disk);

    if (rc)
        return 1;

    log_info("extract: staged to %s", staging_dir);
    return 0;
}
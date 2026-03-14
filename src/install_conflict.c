/*
 * install_conflict.c - File conflict detection
 *
 * Reads the .FILES entry from the package archive and checks
 * every listed path against the installed DB's files table.
 *
 * A conflict exists when a path is already owned by a
 * *different* package (re-installing the same package is
 * handled separately and is not a conflict here).
 *
 * Must be called BEFORE extraction so the system is never
 * partially modified when a conflict is found.
 *
 * Returns:
 *   0  no conflicts
 *   1  conflict detected or error
 */

#define _POSIX_C_SOURCE 200809L

#include "flappy.h"
#include "db_guard.h"

#include <archive.h>
#include <archive_entry.h>
#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES_SIZE (256 * 1024)
#define READ_CHUNK     8192

/* =========================================================================
 * Read .FILES from archive into a buffer
 * ========================================================================= */

static char *read_files_entry(const char *pkgfile, size_t *out_size)
{
    struct archive *a = archive_read_new();
    if (!a)
        return NULL;

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);

    if (archive_read_open_filename(a, pkgfile, 65536) != ARCHIVE_OK) {
        archive_read_free(a);
        return NULL;
    }

    struct archive_entry *entry;
    char   *buffer = NULL;
    size_t  total  = 0;
    int     found  = 0;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

        const char *name = archive_entry_pathname(entry);

        if (!name || (strcmp(name, ".FILES") != 0 &&
                      strcmp(name, "./.FILES") != 0)) {
            archive_read_data_skip(a);
            continue;
        }

        found = 1;

        char chunk[READ_CHUNK];
        ssize_t n;

        while ((n = archive_read_data(a, chunk, sizeof(chunk))) > 0) {

            if (total + (size_t)n > MAX_FILES_SIZE) {
                log_error("conflict: .FILES exceeds size limit");
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            char *tmp = realloc(buffer, total + (size_t)n + 1);
            if (!tmp) {
                archive_read_free(a);
                free(buffer);
                return NULL;
            }

            buffer = tmp;
            memcpy(buffer + total, chunk, (size_t)n);
            total += (size_t)n;
        }

        break; /* found .FILES, stop scanning */
    }

    archive_read_free(a);

    if (!found) {
        /* .FILES is optional in this design; no conflict possible */
        *out_size = 0;
        return NULL;
    }

    if (buffer)
        buffer[total] = '\0';

    *out_size = total;
    return buffer;
}

/* =========================================================================
 * Public entry
 * ========================================================================= */

int install_conflict(const char *pkgname, const char *pkgfile)
{
    size_t  size   = 0;
    char   *buffer = read_files_entry(pkgfile, &size);

    if (!buffer) {
        /* No .FILES present — skip conflict check */
        return 0;
    }

    sqlite3 *db = db_handle();
    if (!db) {
        free(buffer);
        return 1;
    }

    sqlite3_stmt *st = NULL;

    /*
     * For each path in .FILES, find which package owns it.
     * A conflict is when that package is not pkgname itself.
     */
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT p.name FROM files f "
        "JOIN packages p ON f.package_id = p.id "
        "WHERE f.path = ? AND p.name != ?;",
        -1, &st, NULL
    );

    if (rc != SQLITE_OK) {
        db_die(db, rc, "conflict prepare");
    }

    int conflict = 0;

    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\n", &saveptr);

    while (line && !conflict) {

        /* Strip trailing CR */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r')
            line[len - 1] = '\0';

        /* Skip blank lines and metadata markers */
        if (*line == '\0' || *line == '#') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_text(st, 1, line, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 2, pkgname, -1, SQLITE_STATIC);

        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *owner = sqlite3_column_text(st, 0);
            fprintf(stderr,
                    "conflict: /%s is already owned by %s\n",
                    line, owner ? (const char *)owner : "unknown");
            conflict = 1;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    sqlite3_finalize(st);
    free(buffer);

    return conflict;
}
/*
 * install_extract.c - Extract package using libarchive
 */

#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>

int install_extract(const char *pkgfile,
                    char *staging_dir)
{
    (void)staging_dir;

    struct archive *a;
    struct archive_entry *entry;

    a = archive_read_new();

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);

    if (archive_read_open_filename(a, pkgfile, 10240))
        return 1;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

        const char *path = archive_entry_pathname(entry);

        printf("extracting %s\n", path);

        archive_read_data_skip(a);
    }

    archive_read_free(a);

    return 0;
}
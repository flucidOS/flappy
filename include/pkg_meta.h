/* include/pkg_meta.h */

#ifndef FLAPPY_PKG_META_H
#define FLAPPY_PKG_META_H

#include <stddef.h>

/**
 * @struct flappy_pkg
 * @brief Represents metadata for a Flappy package.
 *
 * This structure contains comprehensive information about a software package,
 * including its identification, dependencies, conflicts, and other metadata.
 *
 * @member name
 *   Pointer to a null-terminated string containing the package name.
 *
 * @member version
 *   Pointer to a null-terminated string representing the package version.
 *
 * @member arch
 *   Pointer to a null-terminated string specifying the target architecture
 *   (e.g., "x86_64", "arm64").
 *
 * @member desc
 *   Pointer to a null-terminated string containing a human-readable description
 *   of the package.
 *
 * @member depends
 *   Pointer to an array of null-terminated strings listing package dependencies.
 *   Each string represents a package name that this package requires.
 *
 * @member depends_count
 *   The number of entries in the depends array.
 *
 * @member conflicts
 *   Pointer to an array of null-terminated strings listing packages that conflict
 *   with this package and cannot be installed simultaneously.
 *
 * @member conflicts_count
 *   The number of entries in the conflicts array.
 *
 * @member provides
 *   Pointer to an array of null-terminated strings representing virtual packages
 *   or capabilities provided by this package.
 *
 * @member provides_count
 *   The number of entries in the provides array.
 *
 * @member size
 *   The size of the package in bytes.
 */
struct flappy_pkg {
    char *name;
    char *version;
    char *arch;
    char *desc;

    char **depends;
    size_t depends_count;

    char **conflicts;
    size_t conflicts_count;

    char **provides;
    size_t provides_count;

    size_t size;
};

void pkg_meta_free(struct flappy_pkg *pkg);

/* Parser entry */
struct flappy_pkg *pkg_parse(char *buffer, size_t size);
struct flappy_pkg *pkg_read_from_file(const char *path);

#endif

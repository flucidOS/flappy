#include "flappy.h"
#include "pkg_meta.h"

#include <stdio.h>

/**
 * print_pkg - Display package metadata to stdout
 * @pkg: Pointer to the package structure to display
 *
 * Prints package information including name, version, architecture,
 * description, size, and lists of dependencies, conflicts, and provisions.
 */
static void print_pkg(const struct flappy_pkg *pkg)
{
    printf("Name: %s\n", pkg->name);
    printf("Version: %s\n", pkg->version);
    printf("Arch: %s\n", pkg->arch);

    if (pkg->desc)
        printf("Description: %s\n", pkg->desc);

    printf("Size: %zu\n", pkg->size);

    if (pkg->depends_count > 0) {
        printf("Depends:\n");
        for (size_t i = 0; i < pkg->depends_count; i++)
            printf("  - %s\n", pkg->depends[i]);
    }

    if (pkg->conflicts_count > 0) {
        printf("Conflicts:\n");
        for (size_t i = 0; i < pkg->conflicts_count; i++)
            printf("  - %s\n", pkg->conflicts[i]);
    }

    if (pkg->provides_count > 0) {
        printf("Provides:\n");
        for (size_t i = 0; i < pkg->provides_count; i++)
            printf("  - %s\n", pkg->provides[i]);
    }
}

/**
 * cmd_inspect - Inspect and display package metadata
 * @argc: Argument count
 * @argv: Argument vector; argv[0] should be the package file path
 *
 * Reads a package file and prints its metadata. Returns 0 on success,
 * 1 if the package file cannot be read, and 2 if no file was specified.
 */
int cmd_inspect(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "inspect requires a package file\n");
        return 2;
    }

    const char *path = argv[0];

    struct flappy_pkg *pkg = pkg_read_from_file(path);
    if (!pkg)
        return 1;

    print_pkg(pkg);
    pkg_meta_free(pkg);

    return 0;
}
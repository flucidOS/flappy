#include "flappy.h"
#include "pkg_meta.h"
#include <stdio.h>

static void print_pkg(const struct flappy_pkg *pkg)
{
    printf("Name:    %s\n", pkg->name);
    printf("Version: %s\n", pkg->version);
    printf("Arch:    %s\n", pkg->arch);

    if (pkg->desc)
        printf("Desc:    %s\n", pkg->desc);

    printf("Size:    %zu\n", pkg->size);

    if (pkg->depends_count > 0) {
        printf("Depends:\n");
        for (size_t i = 0; i < pkg->depends_count; i++) {
            const struct dep_entry *d = &pkg->depends[i];
            if (d->op != DEP_OP_NONE && d->version)
                printf("  %s %s %s\n",
                       d->name,
                       d->op == DEP_OP_GE ? ">=" :
                       d->op == DEP_OP_LE ? "<=" :
                       d->op == DEP_OP_GT ? ">"  :
                       d->op == DEP_OP_LT ? "<"  : "=",
                       d->version);
            else
                printf("  %s\n", d->name);
        }
    }

    if (pkg->conflicts_count > 0) {
        printf("Conflicts:\n");
        for (size_t i = 0; i < pkg->conflicts_count; i++)
            printf("  %s\n", pkg->conflicts[i]);
    }

    if (pkg->provides_count > 0) {
        printf("Provides:\n");
        for (size_t i = 0; i < pkg->provides_count; i++)
            printf("  %s\n", pkg->provides[i]);
    }
}

int cmd_inspect(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "inspect requires a package file\n");
        return 2;
    }

    struct flappy_pkg *pkg = pkg_read_from_file(argv[0]);
    if (!pkg) return 1;

    print_pkg(pkg);
    pkg_meta_free(pkg);
    return 0;
}
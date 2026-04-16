#ifndef FLAPPY_PKG_META_H
#define FLAPPY_PKG_META_H

#include <stddef.h>

/*
 * Dependency constraint operators.
 * DEP_OP_NONE means no version constraint — any installed version satisfies.
 */
typedef enum {
    DEP_OP_NONE = 0,   /* depend = libssl          (any version) */
    DEP_OP_GE,         /* depend = libssl >= 3.0   */
    DEP_OP_LE,         /* depend = libssl <= 3.0   */
    DEP_OP_GT,         /* depend = libssl > 3.0    */
    DEP_OP_LT,         /* depend = libssl < 3.0    */
    DEP_OP_EQ          /* depend = libssl = 3.0    */
} dep_op_t;

/*
 * A single dependency entry.
 */
struct dep_entry {
    char    *name;
    dep_op_t op;
    char    *version;
};

/*
 * Package metadata.
 */
struct flappy_pkg {
    char *name;
    char *version;
    char *arch;
    char *desc;

    struct dep_entry *depends;
    size_t            depends_count;

    char **conflicts;
    size_t conflicts_count;

    char **provides;
    size_t provides_count;

    size_t size;

    /*
     * has_install: 1 if the package contains a .INSTALL hook script.
     * Set by pkg_parser when it encounters an "install" field in .PKGINFO.
     * Used by install_commit to extract and run hooks.
     */
    int has_install;
};

void pkg_meta_free(struct flappy_pkg *pkg);

struct flappy_pkg *pkg_parse(char *buffer, size_t size);
struct flappy_pkg *pkg_read_from_file(const char *path);

#endif
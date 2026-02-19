#define _POSIX_C_SOURCE 200809L

#include "pkg_meta.h"
#include "flappy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* =========================
   Temporary parse structure
   ========================= */

struct pkg_parse_tmp {
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
    int size_set;
};

/* =========================
   Utility helpers
   ========================= */

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;

    if (*s == 0)
        return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';
    return s;
}

static void tmp_free(struct pkg_parse_tmp *t) {
    free(t->name);
    free(t->version);
    free(t->arch);
    free(t->desc);

    for (size_t i = 0; i < t->depends_count; i++)
        free(t->depends[i]);
    free(t->depends);

    for (size_t i = 0; i < t->conflicts_count; i++)
        free(t->conflicts[i]);
    free(t->conflicts);

    for (size_t i = 0; i < t->provides_count; i++)
        free(t->provides[i]);
    free(t->provides);
}

/* Append helper for list fields */
static void append_string(char ***arr, size_t *count, const char *value) {
    char **tmp = realloc(*arr, (*count + 1) * sizeof(char *));
    if (!tmp) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    *arr = tmp;
    (*arr)[*count] = strdup(value);
    if (!(*arr)[*count]) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    (*count)++;
}

/* =========================
   Parser implementation
   ========================= */

struct flappy_pkg *pkg_parse(char *buffer, size_t size) {
    (void)size;

    struct pkg_parse_tmp tmp = {0};

    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\n", &saveptr);

    while (line) {
        /* strip CR if present */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r')
            line[len - 1] = '\0';

        char *trimmed = trim(line);

        if (*trimmed == '\0') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq) {
            log_error("Malformed line in PKGINFO: %s", trimmed);
            tmp_free(&tmp);
            return NULL;
        }

        *eq = '\0';

        char *key = trim(trimmed);
        char *value = trim(eq + 1);

        if (*key == '\0') {
            log_error("Empty key in PKGINFO");
            tmp_free(&tmp);
            return NULL;
        }

        /* Single-value fields */
        if (strcmp(key, "pkgname") == 0) {
            if (tmp.name) {
                log_error("Duplicate pkgname");
                tmp_free(&tmp);
                return NULL;
            }
            tmp.name = strdup(value);
        }
        else if (strcmp(key, "pkgver") == 0) {
            if (tmp.version) {
                log_error("Duplicate pkgver");
                tmp_free(&tmp);
                return NULL;
            }
            tmp.version = strdup(value);
        }
        else if (strcmp(key, "arch") == 0) {
            if (tmp.arch) {
                log_error("Duplicate arch");
                tmp_free(&tmp);
                return NULL;
            }
            tmp.arch = strdup(value);
        }
        else if (strcmp(key, "pkgdesc") == 0) {
            if (tmp.desc) {
                log_error("Duplicate pkgdesc");
                tmp_free(&tmp);
                return NULL;
            }
            tmp.desc = strdup(value);
        }
        else if (strcmp(key, "size") == 0) {
            if (tmp.size_set) {
                log_error("Duplicate size");
                tmp_free(&tmp);
                return NULL;
            }

            errno = 0;
            char *end = NULL;
            unsigned long long v = strtoull(value, &end, 10);

            if (errno != 0 || end == value || *end != '\0') {
                log_error("Invalid size value: %s", value);
                tmp_free(&tmp);
                return NULL;
            }

            tmp.size = (size_t)v;
            tmp.size_set = 1;
        }

        /* List fields */
        else if (strcmp(key, "depend") == 0) {
            append_string(&tmp.depends, &tmp.depends_count, value);
        }
        else if (strcmp(key, "conflict") == 0) {
            append_string(&tmp.conflicts, &tmp.conflicts_count, value);
        }
        else if (strcmp(key, "provide") == 0) {
            append_string(&tmp.provides, &tmp.provides_count, value);
        }

        /* Unknown keys */
        else {
            log_info("Unknown PKGINFO key: %s", key);
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    /* =========================
       Semantic validation
       ========================= */

    if (!tmp.name || !tmp.version || !tmp.arch) {
        log_error("Missing required PKGINFO fields");
        tmp_free(&tmp);
        return NULL;
    }

    /* =========================
       Final construction
       ========================= */

    struct flappy_pkg *pkg = malloc(sizeof(*pkg));
    if (!pkg) {
        fprintf(stderr, "Out of memory\n");
        tmp_free(&tmp);
        return NULL;
    }

    pkg->name = tmp.name;
    pkg->version = tmp.version;
    pkg->arch = tmp.arch;
    pkg->desc = tmp.desc;

    pkg->depends = tmp.depends;
    pkg->depends_count = tmp.depends_count;

    pkg->conflicts = tmp.conflicts;
    pkg->conflicts_count = tmp.conflicts_count;

    pkg->provides = tmp.provides;
    pkg->provides_count = tmp.provides_count;

    pkg->size = tmp.size;

    return pkg;
}

/* =========================
   Free function
   ========================= */

void pkg_meta_free(struct flappy_pkg *pkg) {
    if (!pkg) return;

    free(pkg->name);
    free(pkg->version);
    free(pkg->arch);
    free(pkg->desc);

    for (size_t i = 0; i < pkg->depends_count; i++)
        free(pkg->depends[i]);
    free(pkg->depends);

    for (size_t i = 0; i < pkg->conflicts_count; i++)
        free(pkg->conflicts[i]);
    free(pkg->conflicts);

    for (size_t i = 0; i < pkg->provides_count; i++)
        free(pkg->provides[i]);
    free(pkg->provides);

    free(pkg);
}

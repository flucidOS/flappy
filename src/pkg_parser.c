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
    char *version;  /* pkgver, later optionally suffixed with -pkgrel */
    char *pkgrel;   /* stored separately until finalisation */
    char *arch;
    char *desc;

    struct dep_entry *depends;
    size_t            depends_count;

    char **conflicts;
    size_t conflicts_count;

    char **provides;
    size_t provides_count;

    size_t size;
    int    size_set;
};

/* =========================
   Utility helpers
   ========================= */

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void tmp_free(struct pkg_parse_tmp *t) {
    free(t->name);
    free(t->version);
    free(t->pkgrel);
    free(t->arch);
    free(t->desc);

    for (size_t i = 0; i < t->depends_count; i++) {
        free(t->depends[i].name);
        free(t->depends[i].version);
    }
    free(t->depends);

    for (size_t i = 0; i < t->conflicts_count; i++)
        free(t->conflicts[i]);
    free(t->conflicts);

    for (size_t i = 0; i < t->provides_count; i++)
        free(t->provides[i]);
    free(t->provides);
}

static void append_string(char ***arr, size_t *count, const char *value) {
    char **tmp = realloc(*arr, (*count + 1) * sizeof(char *));
    if (!tmp) { fprintf(stderr, "Out of memory\n"); exit(1); }
    *arr = tmp;
    (*arr)[*count] = strdup(value);
    if (!(*arr)[*count]) { fprintf(stderr, "Out of memory\n"); exit(1); }
    (*count)++;
}

/* =========================
   Dependency constraint parser
   ========================= */

static dep_op_t parse_op(const char *s, size_t *op_len)
{
    if (strncmp(s, ">=", 2) == 0) { *op_len = 2; return DEP_OP_GE; }
    if (strncmp(s, "<=", 2) == 0) { *op_len = 2; return DEP_OP_LE; }
    if (strncmp(s, ">",  1) == 0) { *op_len = 1; return DEP_OP_GT; }
    if (strncmp(s, "<",  1) == 0) { *op_len = 1; return DEP_OP_LT; }
    if (strncmp(s, "=",  1) == 0) { *op_len = 1; return DEP_OP_EQ; }
    *op_len = 0;
    return DEP_OP_NONE;
}

static int append_dep(struct dep_entry **arr,
                      size_t           *count,
                      const char       *value)
{
    struct dep_entry *tmp = realloc(*arr,
                                    (*count + 1) * sizeof(struct dep_entry));
    if (!tmp) return 0;
    *arr = tmp;

    struct dep_entry *e = &(*arr)[*count];
    e->name    = NULL;
    e->version = NULL;
    e->op      = DEP_OP_NONE;

    /* Parse name (up to first whitespace) */
    const char *p = value;
    while (*p && !isspace((unsigned char)*p)) p++;

    size_t name_len = (size_t)(p - value);
    if (name_len == 0) return 0;

    e->name = malloc(name_len + 1);
    if (!e->name) return 0;
    memcpy(e->name, value, name_len);
    e->name[name_len] = '\0';

    /* Skip whitespace */
    while (isspace((unsigned char)*p)) p++;

    if (*p == '\0') {
        /* No constraint */
        (*count)++;
        return 1;
    }

    /* Parse operator */
    size_t op_len = 0;
    e->op = parse_op(p, &op_len);

    if (e->op == DEP_OP_NONE) {
        log_error("pkg_parser: unrecognised operator in depend: %s", value);
        free(e->name);
        return 0;
    }

    p += op_len;
    while (isspace((unsigned char)*p)) p++;

    if (*p == '\0') {
        log_error("pkg_parser: operator with no version in depend: %s", value);
        free(e->name);
        return 0;
    }

    e->version = strdup(p);
    if (!e->version) {
        free(e->name);
        return 0;
    }

    (*count)++;
    return 1;
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
        char *key   = trim(trimmed);
        char *value = trim(eq + 1);

        if (*key == '\0') {
            log_error("Empty key in PKGINFO");
            tmp_free(&tmp);
            return NULL;
        }

        if (strcmp(key, "pkgname") == 0) {
            if (tmp.name) { log_error("Duplicate pkgname"); tmp_free(&tmp); return NULL; }
            tmp.name = strdup(value);
        }
        else if (strcmp(key, "pkgver") == 0) {
            if (tmp.version) { log_error("Duplicate pkgver"); tmp_free(&tmp); return NULL; }
            tmp.version = strdup(value);
        }
        else if (strcmp(key, "pkgrel") == 0) {
            /*
             * pkgrel is the package release number (e.g. 1, 2).
             * It is appended to pkgver as "pkgver-pkgrel" at
             * finalisation so the installed version stored in the DB
             * matches the composite format the repository uses.
             * Silently ignoring pkgrel meant the DB stored "2.12"
             * while the repo stored "2.12-1", breaking upgrade detection.
             */
            if (tmp.pkgrel) { log_error("Duplicate pkgrel"); tmp_free(&tmp); return NULL; }
            tmp.pkgrel = strdup(value);
        }
        else if (strcmp(key, "arch") == 0) {
            if (tmp.arch) { log_error("Duplicate arch"); tmp_free(&tmp); return NULL; }
            tmp.arch = strdup(value);
        }
        else if (strcmp(key, "pkgdesc") == 0) {
            if (tmp.desc) { log_error("Duplicate pkgdesc"); tmp_free(&tmp); return NULL; }
            tmp.desc = strdup(value);
        }
        else if (strcmp(key, "size") == 0) {
            if (tmp.size_set) { log_error("Duplicate size"); tmp_free(&tmp); return NULL; }
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
        else if (strcmp(key, "depend") == 0) {
            if (!append_dep(&tmp.depends, &tmp.depends_count, value)) {
                log_error("Failed to parse depend field: %s", value);
                tmp_free(&tmp);
                return NULL;
            }
        }
        else if (strcmp(key, "conflict") == 0) {
            append_string(&tmp.conflicts, &tmp.conflicts_count, value);
        }
        else if (strcmp(key, "provide") == 0) {
            append_string(&tmp.provides, &tmp.provides_count, value);
        }
        else {
            log_info("Unknown PKGINFO key: %s", key);
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (!tmp.name || !tmp.version || !tmp.arch) {
        log_error("Missing required PKGINFO fields");
        tmp_free(&tmp);
        return NULL;
    }

    /*
     * Finalise version: if pkgrel is present, produce "pkgver-pkgrel".
     * This matches the composite version format used in repo.db so that
     * flappy upgrade compares apples to apples.
     */
    if (tmp.pkgrel) {
        size_t vlen = strlen(tmp.version);
        size_t rlen = strlen(tmp.pkgrel);
        /* "pkgver" + "-" + "pkgrel" + NUL */
        char *composite = malloc(vlen + 1 + rlen + 1);
        if (!composite) {
            fprintf(stderr, "Out of memory\n");
            tmp_free(&tmp);
            return NULL;
        }
        memcpy(composite, tmp.version, vlen);
        composite[vlen] = '-';
        memcpy(composite + vlen + 1, tmp.pkgrel, rlen);
        composite[vlen + 1 + rlen] = '\0';

        free(tmp.version);
        tmp.version = composite;

        free(tmp.pkgrel);
        tmp.pkgrel = NULL;
    }

    struct flappy_pkg *pkg = malloc(sizeof(*pkg));
    if (!pkg) {
        fprintf(stderr, "Out of memory\n");
        tmp_free(&tmp);
        return NULL;
    }

    pkg->name    = tmp.name;
    pkg->version = tmp.version;
    pkg->arch    = tmp.arch;
    pkg->desc    = tmp.desc;

    pkg->depends       = tmp.depends;
    pkg->depends_count = tmp.depends_count;

    pkg->conflicts       = tmp.conflicts;
    pkg->conflicts_count = tmp.conflicts_count;

    pkg->provides       = tmp.provides;
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

    for (size_t i = 0; i < pkg->depends_count; i++) {
        free(pkg->depends[i].name);
        free(pkg->depends[i].version);
    }
    free(pkg->depends);

    for (size_t i = 0; i < pkg->conflicts_count; i++)
        free(pkg->conflicts[i]);
    free(pkg->conflicts);

    for (size_t i = 0; i < pkg->provides_count; i++)
        free(pkg->provides[i]);
    free(pkg->provides);

    free(pkg);
}
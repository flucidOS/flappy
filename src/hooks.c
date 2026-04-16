/*
 * hooks.c - Package hook execution
 *
 * SECURITY NOTE
 *
 * This module executes arbitrary bash code from package .INSTALL scripts.
 * This is an explicit design decision — the same decision pacman makes.
 * The trust model is:
 *
 *   HTTPS transport → repo.db SHA256 → package SHA256 → .INSTALL execution
 *
 * The hook script is only executed after the package has passed SHA256
 * verification.  Execution requires root (install_guard enforces this).
 * The script runs as root in the real filesystem environment.
 *
 * This is a deliberate departure from PHILOSOPHY.md's "no post-install
 * scripts" position, made explicitly by the project maintainer.
 */

#define _POSIX_C_SOURCE 200809L

#include "hooks.h"
#include "flappy.h"

#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* =========================================================================
 * hook_path
 * ========================================================================= */

void hook_path(const char *pkgname, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%s.install", FLAPPY_HOOKS_DIR, pkgname);
}

/* =========================================================================
 * run_hook
 *
 * Sources the script, checks whether the function is defined with
 * `declare -f`, and calls it only if so.  Missing functions are not
 * errors — consistent with pacman behaviour.
 *
 * Arguments are passed via environment variables so that version strings
 * never appear in the command string itself, avoiding any injection risk.
 * ========================================================================= */

int run_hook(const char *script_path,
             const char *function,
             const char *arg1,
             const char *arg2)
{
    if (!script_path || !function)
        return 0;

    struct stat st;
    if (stat(script_path, &st) != 0)
        return 0;   /* no hook file — not an error */

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "bash -c '"
        "source \"$FLAPPY_HOOK_SCRIPT\"; "
        "if declare -f \"$FLAPPY_HOOK_FUNC\" > /dev/null 2>&1; then "
        "  \"$FLAPPY_HOOK_FUNC\" "
        "  ${FLAPPY_HOOK_ARG1:+\"$FLAPPY_HOOK_ARG1\"} "
        "  ${FLAPPY_HOOK_ARG2:+\"$FLAPPY_HOOK_ARG2\"}; "
        "fi'");

    setenv("FLAPPY_HOOK_SCRIPT", script_path, 1);
    setenv("FLAPPY_HOOK_FUNC",   function,    1);

    if (arg1) setenv("FLAPPY_HOOK_ARG1", arg1, 1);
    else       unsetenv("FLAPPY_HOOK_ARG1");

    if (arg2) setenv("FLAPPY_HOOK_ARG2", arg2, 1);
    else       unsetenv("FLAPPY_HOOK_ARG2");

    int rc = system(cmd);

    unsetenv("FLAPPY_HOOK_SCRIPT");
    unsetenv("FLAPPY_HOOK_FUNC");
    unsetenv("FLAPPY_HOOK_ARG1");
    unsetenv("FLAPPY_HOOK_ARG2");

    if (rc == -1) {
        fprintf(stderr, "hook: system() failed for %s/%s\n",
                script_path, function);
        return 1;
    }

    int exit_code = WEXITSTATUS(rc);
    if (exit_code != 0) {
        fprintf(stderr, "[ERROR] hook %s() exited with code %d\n",
                function, exit_code);
        log_error("hook %s() in %s exited %d", function, script_path, exit_code);
        return 1;
    }

    return 0;
}

/* =========================================================================
 * hook_install_from_pkg
 *
 * Reads the package archive and extracts .INSTALL to
 * FLAPPY_HOOKS_DIR/<pkgname>.install.
 *
 * Returns:
 *   0    script extracted, or package has no .INSTALL (not an error)
 *  -1    archive could not be read
 *   1    write failed
 * ========================================================================= */

int hook_install_from_pkg(const char *pkgname, const char *pkgfile)
{
    if (mkdir(FLAPPY_HOOKS_DIR, 0700) == -1 && errno != EEXIST) {
        fprintf(stderr, "hook: cannot create %s: %s\n",
                FLAPPY_HOOKS_DIR, strerror(errno));
        return 1;
    }

    struct archive *a = archive_read_new();
    if (!a)
        return -1;

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);

    if (archive_read_open_filename(a, pkgfile, 65536) != ARCHIVE_OK) {
        fprintf(stderr, "hook: cannot open %s: %s\n",
                pkgfile, archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    struct archive_entry *entry;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (!name) {
            archive_read_data_skip(a);
            continue;
        }

        int is_install = (strcmp(name, ".INSTALL")  == 0 ||
                          strcmp(name, "./.INSTALL") == 0);

        if (!is_install) {
            archive_read_data_skip(a);
            continue;
        }

        if (archive_entry_filetype(entry) != AE_IFREG) {
            archive_read_data_skip(a);
            continue;
        }

        char out_path[256];
        hook_path(pkgname, out_path, sizeof(out_path));

        int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0700);
        if (fd < 0) {
            fprintf(stderr, "hook: cannot create %s: %s\n",
                    out_path, strerror(errno));
            archive_read_free(a);
            return 1;
        }

        char buf[8192];
        ssize_t n;
        int write_err = 0;

        while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) {
            if (write(fd, buf, (size_t)n) != n) {
                write_err = 1;
                break;
            }
        }

        close(fd);

        if (write_err || n < 0) {
            fprintf(stderr, "hook: write error for %s\n", out_path);
            unlink(out_path);
            archive_read_free(a);
            return 1;
        }

        log_info("hook: installed .INSTALL -> %s", out_path);
        archive_read_free(a);
        return 0;   /* found and extracted */
    }

    archive_read_free(a);
    return 0;   /* no .INSTALL — not an error */
}

/* =========================================================================
 * hook_remove
 * ========================================================================= */

void hook_remove(const char *pkgname)
{
    char path[256];
    hook_path(pkgname, path, sizeof(path));

    if (unlink(path) == 0)
        log_info("hook: removed %s", path);
    /* ENOENT is fine — package had no hook */
}
/*
 * install.c - Package installation orchestrator
 *
 * Pipeline order (revised):
 *
 *   guard → lookup → download → verify → extract → conflict → commit
 *
 * Conflict detection was previously run before extraction, using the
 * .FILES manifest from the archive as the source of paths to check.
 * If .FILES diverged from the actual archive content (malformed or
 * hand-crafted package), conflicts could be missed.
 *
 * Moving conflict detection after extraction means install_conflict
 * checks against the real files on disk in the staging directory,
 * which is the definitive list of what would actually be installed.
 *
 * The atomicity invariant is preserved: no files are written to the
 * real filesystem until install_commit, which happens after conflict
 * detection passes.  Extraction still goes to the staging directory
 * only, so a conflict abort leaves the system unchanged (staging is
 * cleaned on abort).
 *
 * UX contract:
 *   resolving package...
 *   downloading <file>
 *   [progress bar]
 *   ✔ download complete
 *   verifying package integrity...
 *   ✔ verified
 *   extracting files...
 *   checking file conflicts...
 *   ✔ no conflicts
 *   ✔ installed: <pkg> <ver>
 */

#include "install.h"
#include "flappy.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int install_guard(void);
int install_lookup(const char *pkg, char *filename, char *checksum);
int install_download(const char *filename, char *local_path,
                     const char *expected_checksum);
int install_verify(const char *path, const char *checksum);
int install_conflict_staged(const char *pkgname, const char *staging_dir);
int install_extract(const char *pkgfile, char *staging_dir);
int install_commit(const char *pkgname, const char *pkgfile,
                   const char *staging_dir);

/* Forward declaration for staging cleanup on abort */
static void abort_cleanup(const char *staging_dir);

int install_package(const char *pkgname)
{
    char filename[256];
    char checksum[128];
    char pkgpath[512];
    char staging[512];

    staging[0] = '\0';

    ui_step("resolving package...");

    if (install_guard()) {
        ui_error("root privileges required");
        return 1;
    }

    if (install_lookup(pkgname, filename, checksum)) {
        ui_error("package not found in repository: %s", pkgname);
        return 1;
    }

    if (install_download(filename, pkgpath, checksum))
        return 1;

    ui_step("verifying package integrity...");
    if (install_verify(pkgpath, checksum)) {
        ui_error("package integrity verification failed");
        return 1;
    }
    ui_ok("verified");

    /* Extract first so conflict check runs against real staged paths */
    ui_step("extracting files...");
    if (install_extract(pkgpath, staging)) {
        ui_error("extraction failed");
        return 1;
    }

    db_open_or_die();

    ui_step("checking file conflicts...");
    if (install_conflict_staged(pkgname, staging)) {
        db_close();
        abort_cleanup(staging);
        return 1;
    }
    ui_ok("no conflicts");

    if (install_commit(pkgname, pkgpath, staging)) {
        db_close();
        return 1;
    }

    db_close();

    ui_ok("installed: %s", pkgname);
    return 0;
}

static void abort_cleanup(const char *staging_dir)
{
    if (!staging_dir || staging_dir[0] == '\0')
        return;
    char cmd[512 + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", staging_dir);
    (void)system(cmd);
}
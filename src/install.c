/*
 * install.c - Package installation orchestrator
 *
 * UX contract:
 *   resolving package...
 *   downloading <file>
 *   [progress bar]
 *   ✔ download complete
 *   verifying package integrity...
 *   ✔ verified
 *   checking file conflicts...
 *   ✔ no conflicts
 *   extracting files...
 *   ✔ installed: <pkg> <ver>
 */

#include "install.h"
#include "flappy.h"
#include "ui.h"

#include <stdio.h>

int install_guard(void);
int install_lookup(const char *pkg, char *filename, char *checksum);
int install_download(const char *filename, char *local_path);
int install_verify(const char *path, const char *checksum);
int install_conflict(const char *pkgname, const char *pkgfile);
int install_extract(const char *pkgfile, char *staging_dir);
int install_commit(const char *pkgname, const char *pkgfile,
                   const char *staging_dir);


int install_package(const char *pkgname)
{
    char filename[256];
    char checksum[128];
    char pkgpath[512];
    char staging[512];

    ui_step("resolving package...");

    if (install_guard()) {
        ui_error("root privileges required");
        return 1;
    }

    if (install_lookup(pkgname, filename, checksum)) {
        ui_error("package not found in repository: %s", pkgname);
        return 1;
    }

    /* download with progress bar */
    if (install_download(filename, pkgpath))
        return 1;

    /* verify */
    ui_step("verifying package integrity...");
    if (install_verify(pkgpath, checksum)) {
        ui_error("package integrity verification failed");
        return 1;
    }
    ui_ok("verified");

    db_open_or_die();

    /* conflict check */
    ui_step("checking file conflicts...");
    if (install_conflict(pkgname, pkgpath)) {
        db_close();
        return 1;
    }
    ui_ok("no conflicts");

    /* extract */
    ui_step("extracting files...");
    if (install_extract(pkgpath, staging)) {
        ui_error("extraction failed");
        db_close();
        return 1;
    }

    /* commit */
    if (install_commit(pkgname, pkgpath, staging)) {
        db_close();
        return 1;
    }

    db_close();

    ui_ok("installed: %s", pkgname);
    return 0;
}
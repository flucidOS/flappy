/*
 * install.c - Package installation orchestrator
 *
 * Pipeline:
 *   guard → lookup → download → verify → extract → commit
 */

#include "install.h"
#include "flappy.h"

#include <stdio.h>

/* internal steps */
int install_guard(void);
int install_lookup(const char *pkg,
                   char *filename,
                   char *checksum);

int install_download(const char *filename,
                     char *local_path);

int install_verify(const char *path,
                   const char *checksum);

int install_extract(const char *pkgfile,
                    char *staging_dir);

int install_commit(const char *pkgname,
                   const char *staging_dir);


int install_package(const char *pkgname)
{
    char filename[256];
    char checksum[128];
    char pkgpath[512];
    char staging[512];

    printf("Resolving package...\n");

    if (install_guard())
        return 1;

    if (install_lookup(pkgname, filename, checksum))
        return 1;

    printf("Downloading %s...\n", filename);

    if (install_download(filename, pkgpath))
        return 1;

    printf("Verifying integrity...\n");

    if (install_verify(pkgpath, checksum))
        return 1;

    printf("Extracting package...\n");

    if (install_extract(pkgpath, staging))
        return 1;

    printf("Installing files...\n");

    if (install_commit(pkgname, staging))
        return 1;

    printf("Installation complete.\n");

    return 0;
}
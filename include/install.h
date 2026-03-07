#ifndef INSTALL_H
#define INSTALL_H

/*
 * install.h - Package installation engine
 *
 * This module performs atomic package installation.
 *
 * Install pipeline:
 *   guard → lookup → download → verify → extract → commit
 */

int install_package(const char *pkgname);

#endif
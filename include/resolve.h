#ifndef RESOLVE_H
#define RESOLVE_H

/*
 * resolve.h - Dependency resolver
 *
 * resolve_and_install(pkgname)
 *
 *   Computes the full transitive dependency closure of `pkgname`
 *   from repo.db, filters out already-installed packages, and
 *   installs the remainder in dependency-first topological order.
 *
 *   Each package goes through the standard install pipeline
 *   (guard → lookup → download → verify → extract → conflict → commit),
 *   so all existing atomicity and integrity guarantees are preserved.
 *
 *   Returns:
 *     0   all packages installed successfully
 *     1   resolution failed (cycle, missing dep, install error)
 */
int resolve_and_install(const char *pkgname);

#endif /* RESOLVE_H */
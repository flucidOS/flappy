#ifndef INSTALL_CONSTRAINTS_H
#define INSTALL_CONSTRAINTS_H

#include "pkg_meta.h"

/*
 * install_check_constraints
 *
 * Verifies that all version-constrained dependencies declared by pkg
 * are satisfied by currently installed versions.
 *
 * Returns:
 *   0  all constraints satisfied (or unconstrained)
 *   1  one or more constraints violated
 */
int install_check_constraints(const struct flappy_pkg *pkg);

#endif
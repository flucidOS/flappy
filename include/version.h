#ifndef VERSION_H
#define VERSION_H

#include "pkg_meta.h"

/*
 * version_is_valid
 *
 * Returns 1 if version string matches numeric dot format [0-9]+(\.[0-9]+)*
 */
int version_is_valid(const char *v);

/*
 * version_cmp
 *
 * Returns:
 *   1  if a > b
 *   0  if equal
 *  -1  if a < b
 */
int version_cmp(const char *a, const char *b);

/*
 * version_satisfies
 *
 * Returns 1 if `installed` satisfies the constraint (op, required).
 * If op is DEP_OP_NONE, always returns 1.
 * Returns 0 if either version is invalid or constraint not met.
 */
int version_satisfies(const char *installed,
                      dep_op_t    op,
                      const char *required);

#endif
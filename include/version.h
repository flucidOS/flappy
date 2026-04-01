#ifndef VERSION_H
#define VERSION_H

#include "pkg_meta.h"
#include <limits.h>

/*
 * version_is_valid
 *
 * Returns 1 if version string matches numeric dot format [0-9]+(\.[0-9]+)*
 * Returns 0 otherwise.
 */
int version_is_valid(const char *v);

/*
 * version_cmp
 *
 * Strict numeric dot-segment comparison.
 *
 * Returns:
 *    1      if a > b
 *    0      if a == b
 *   -1      if a < b
 *   INT_MIN if either string fails version_is_valid()
 *
 * Callers must check for INT_MIN before using the result in a
 * relational expression.  version_satisfies() does this correctly.
 * Passing an invalid version string to version_cmp and treating the
 * result as {-1, 0, 1} is a caller bug.
 */
int version_cmp(const char *a, const char *b);

/*
 * version_satisfies
 *
 * Returns 1 if `installed` satisfies the constraint (op, required).
 * Returns 0 if:
 *   - the constraint is not met
 *   - either version string is invalid (version_cmp returned INT_MIN)
 * If op is DEP_OP_NONE, always returns 1 (unconstrained dependency).
 */
int version_satisfies(const char *installed,
                      dep_op_t    op,
                      const char *required);

#endif /* VERSION_H */
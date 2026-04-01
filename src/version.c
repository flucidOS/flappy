/*
 * version.c - Strict numeric version comparison
 *
 * Implements deterministic numeric dot-segment comparison.
 *
 * Rules:
 *   - Version format: [0-9]+(\.[0-9]+)*
 *   - No alpha, beta, rc, or metadata allowed
 *   - Missing trailing segments are treated as 0
 *
 * Comparison contract:
 *   return  1      if a > b
 *   return  0      if a == b
 *   return -1      if a < b
 *   return INT_MIN if either version string is invalid
 *
 * Returning 0 on invalid input (the previous behaviour) was wrong:
 * it caused version_satisfies with DEP_OP_GE to return "satisfied"
 * for a package with a malformed installed version string, silently
 * bypassing the constraint check.  INT_MIN is unambiguous — it is
 * outside the normal {-1, 0, 1} range and callers can detect it.
 */

#include "version.h"
#include "pkg_meta.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
 * version_is_valid
 *
 * Validates that version string matches:
 *   [0-9]+(\.[0-9]+)*
 *
 * Returns:
 *   1 if valid
 *   0 if invalid
 */
int version_is_valid(const char *v)
{
    if (!v || !*v)
        return 0;

    int seen_digit = 0;

    for (size_t i = 0; v[i]; i++) {

        if (isdigit((unsigned char)v[i])) {
            seen_digit = 1;
            continue;
        }

        if (v[i] == '.') {
            /* dot cannot be first or follow another dot */
            if (!seen_digit)
                return 0;

            /* next must be digit */
            if (!isdigit((unsigned char)v[i + 1]))
                return 0;

            seen_digit = 0;
            continue;
        }

        /* any other character is invalid */
        return 0;
    }

    return seen_digit;
}

/*
 * parse_segment
 *
 * Parses numeric segment starting at *p.
 * Advances pointer past the digits and the following dot (if any).
 */
static long parse_segment(const char **p)
{
    long value = 0;

    while (**p && isdigit((unsigned char)**p)) {
        value = value * 10 + (**p - '0');
        (*p)++;
    }

    /* skip dot if present */
    if (**p == '.')
        (*p)++;

    return value;
}

/*
 * version_cmp
 *
 * Strict numeric dot-segment comparison.
 *
 * Returns:
 *   1      if a > b
 *   0      if equal
 *  -1      if a < b
 *  INT_MIN if either version string fails version_is_valid()
 *
 * Callers that receive INT_MIN must treat the comparison as failed
 * rather than as "equal".  version_satisfies() handles this correctly.
 */
int version_cmp(const char *a, const char *b)
{
    if (!version_is_valid(a) || !version_is_valid(b))
        return INT_MIN;

    const char *pa = a;
    const char *pb = b;

    while (*pa || *pb) {

        long seg_a = 0;
        long seg_b = 0;

        if (*pa)
            seg_a = parse_segment(&pa);

        if (*pb)
            seg_b = parse_segment(&pb);

        if (seg_a > seg_b)
            return 1;

        if (seg_a < seg_b)
            return -1;
    }

    return 0;
}

/*
 * version_satisfies
 *
 * Evaluates whether `installed` satisfies the constraint (op, required).
 *
 * Returns:
 *   1  constraint satisfied (or op is DEP_OP_NONE)
 *   0  constraint not satisfied, invalid version strings, or
 *      version_cmp returned INT_MIN (invalid input)
 */
int version_satisfies(const char *installed,
                      dep_op_t    op,
                      const char *required)
{
    if (op == DEP_OP_NONE)
        return 1;

    /* version_cmp validates both strings internally; check its return */
    int cmp = version_cmp(installed, required);

    if (cmp == INT_MIN)
        return 0; /* invalid version — constraint cannot be satisfied */

    switch (op) {
    case DEP_OP_GE: return cmp >= 0;
    case DEP_OP_LE: return cmp <= 0;
    case DEP_OP_GT: return cmp >  0;
    case DEP_OP_LT: return cmp <  0;
    case DEP_OP_EQ: return cmp == 0;
    default:        return 0;
    }
}
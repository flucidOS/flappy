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
 *   return  1  if a > b
 *   return  0  if a == b
 *   return -1  if a < b
 */

#include <ctype.h>
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
 * Advances pointer to next segment.
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
 *   1  if a > b
 *   0  if equal
 *  -1  if a < b
 *
 * If either version is invalid, comparison fails hard
 * and returns 0 (caller should validate beforehand).
 */
int version_cmp(const char *a, const char *b)
{
    if (!version_is_valid(a) || !version_is_valid(b))
        return 0;

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
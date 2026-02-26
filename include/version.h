#ifndef VERSION_H
#define VERSION_H

/*
 * version_is_valid
 *
 * Returns 1 if version string matches numeric dot format.
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

#endif
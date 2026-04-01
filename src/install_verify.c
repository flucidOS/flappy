/*
 * install_verify.c - Package integrity verification via SHA256
 *
 * Delegates to sha256_file() in sha256.c (shared with repo_update.c).
 * The EVP implementation lives there; this file only handles the
 * comparison and the caller-facing error messages.
 */

#include "sha256.h"

#include <stdio.h>
#include <string.h>

/*
 * install_verify
 *
 * Computes SHA256 of the file at `path` and compares it against
 * the expected lowercase hex string in `checksum`.
 *
 * Returns:
 *   0  match
 *   1  mismatch or any error
 */
int install_verify(const char *path, const char *checksum)
{
    char actual[65];

    if (sha256_file(path, actual) != 0) {
        /* sha256_file already printed the reason */
        return 1;
    }

    if (strcmp(actual, checksum) != 0) {
        fprintf(stderr,
                "verify: checksum mismatch\n"
                "  expected: %s\n"
                "  actual:   %s\n",
                checksum, actual);
        return 1;
    }

    return 0;
}
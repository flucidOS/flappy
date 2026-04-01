#ifndef SHA256_H
#define SHA256_H

/*
 * sha256.h - Shared SHA256 file digest helper
 *
 * Used by install_verify.c and repo_update.c so both code paths
 * use the same OpenSSL EVP implementation.
 *
 * sha256_file(path, out)
 *
 *   Computes the SHA256 digest of the file at `path` and writes
 *   the result as a 64-character lowercase hex string into `out`.
 *   `out` must be at least 65 bytes (64 hex chars + NUL).
 *
 *   Returns:
 *     0  success
 *     1  any error (open, read, EVP failure); reason printed to stderr
 */
int sha256_file(const char *path, char out[65]);

#endif /* SHA256_H */

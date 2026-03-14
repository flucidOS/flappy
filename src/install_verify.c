/*
 * install_verify.c - Package integrity verification via SHA256
 *
 * Uses OpenSSL EVP digest API.
 * Makefile already links -lssl -lcrypto.
 *
 * Reads the file in chunks to avoid loading
 * the entire package into memory.
 */

#include <openssl/evp.h>

#include <stdio.h>
#include <string.h>

#define READ_CHUNK 65536

/*
 * hex_encode
 *
 * Converts raw digest bytes into lowercase hex string.
 * out must be at least (digest_len * 2 + 1) bytes.
 */
static void hex_encode(const unsigned char *digest,
                       unsigned int digest_len,
                       char *out)
{
    static const char hex[] = "0123456789abcdef";

    for (unsigned int i = 0; i < digest_len; i++) {
        out[i * 2]     = hex[(digest[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }

    out[digest_len * 2] = '\0';
}

/*
 * install_verify
 *
 * Computes SHA256 of the file at `path` and compares
 * it against the expected hex string in `checksum`.
 *
 * Returns:
 *   0  on match
 *   1  on mismatch or any error
 */
int install_verify(const char *path,
                   const char *checksum)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "verify: cannot open %s\n", path);
        return 1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fprintf(stderr, "verify: EVP_MD_CTX_new failed\n");
        fclose(f);
        return 1;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "verify: EVP_DigestInit failed\n");
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return 1;
    }

    unsigned char buf[READ_CHUNK];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            fprintf(stderr, "verify: EVP_DigestUpdate failed\n");
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return 1;
        }
    }

    if (ferror(f)) {
        fprintf(stderr, "verify: read error on %s\n", path);
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return 1;
    }

    fclose(f);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        fprintf(stderr, "verify: EVP_DigestFinal failed\n");
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    EVP_MD_CTX_free(ctx);

    /* SHA256 produces 32 bytes = 64 hex chars */
    char actual[65];
    hex_encode(digest, digest_len, actual);

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
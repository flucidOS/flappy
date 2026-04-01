/*
 * sha256.c - Shared SHA256 file digest helper
 *
 * Extracted from install_verify.c so that both install_verify.c and
 * repo_update.c use the same OpenSSL EVP implementation.
 *
 * Previously repo_update.c used popen("sha256sum ...") which:
 *   - depends on sha256sum existing on the target
 *   - uses an uncontrolled PATH lookup
 *   - could silently accept a truncated hash via fscanf("%64s")
 *   - has a different security boundary than the package integrity check
 *
 * This file centralises the implementation once.
 */

#include "sha256.h"

#include <openssl/evp.h>

#include <stdio.h>
#include <string.h>

#define READ_CHUNK 65536

/*
 * hex_encode
 *
 * Converts raw digest bytes into a lowercase hex string.
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

int sha256_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "sha256: cannot open %s\n", path);
        return 1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fprintf(stderr, "sha256: EVP_MD_CTX_new failed\n");
        fclose(f);
        return 1;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "sha256: EVP_DigestInit failed\n");
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return 1;
    }

    unsigned char buf[READ_CHUNK];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            fprintf(stderr, "sha256: EVP_DigestUpdate failed\n");
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return 1;
        }
    }

    if (ferror(f)) {
        fprintf(stderr, "sha256: read error on %s\n", path);
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return 1;
    }

    fclose(f);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        fprintf(stderr, "sha256: EVP_DigestFinal failed\n");
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    EVP_MD_CTX_free(ctx);

    hex_encode(digest, digest_len, out);
    return 0;
}

/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
 * Copyright (c) 2019      Duncan Overbruck <mail@duncano.de>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "flappy_api_impl.h"
#include "fetch.h"

static int
verify_binpkg(struct flappy_handle *xhp, flappy_dictionary_t pkgd)
{
	char binfile[PATH_MAX];
	struct flappy_repo *repo;
	const char *pkgver, *repoloc, *sha256;
	ssize_t l;
	int rv = 0;

	flappy_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);
	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	l = flappy_pkg_path(xhp, binfile, sizeof(binfile), pkgd);
	if (l < 0)
		return -l;

	/*
	 * For pkgs in local repos check the sha256 hash.
	 * For pkgs in remote repos check the RSA signature.
	 */
	if ((repo = flappy_rpool_get_repo(repoloc)) == NULL) {
		rv = errno;
		flappy_dbg_printf("%s: failed to get repository "
			"%s: %s\n", pkgver, repoloc, strerror(errno));
		return rv;
	}
	if (repo->is_remote) {
		/* remote repo */
		flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY, 0, pkgver,
			"%s: verifying RSA signature...", pkgver);

		if (!flappy_verify_file_signature(repo, binfile)) {
			rv = EPERM;
			flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL, rv, pkgver,
				"%s: the RSA signature is not valid!", pkgver);
			flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL, rv, pkgver,
				"%s: removed pkg archive and its signature.", pkgver);
			(void)remove(binfile);
			if (flappy_strlcat(binfile, ".sig2", sizeof(binfile)) < sizeof(binfile))
				(void)remove(binfile);
			return rv;
		}
	} else {
		/* local repo */
		flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY, 0, pkgver,
			"%s: verifying SHA256 hash...", pkgver);
		flappy_dictionary_get_cstring_nocopy(pkgd, "filename-sha256", &sha256);
		if ((rv = flappy_file_sha256_check(binfile, sha256)) != 0) {
			if (rv == ERANGE) {
				flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL,
				    rv, pkgver,
				    "%s: checksum does not match repository index",
				    pkgver);
			} else {
				flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL,
				    rv, pkgver, "%s: failed to checksum: %s",
				    pkgver, strerror(errno));
			}
			return rv;
		}

	}

	return 0;
}

static int
download_binpkg(struct flappy_handle *xhp, flappy_dictionary_t repo_pkgd)
{
	struct flappy_repo *repo;
	char buf[PATH_MAX];
	char *sigsuffix;
	const char *pkgver, *arch, *fetchstr, *repoloc;
	unsigned char digest[FLAPPY_SHA256_DIGEST_SIZE] = {0};
	int rv = 0;

	flappy_dictionary_get_cstring_nocopy(repo_pkgd, "repository", &repoloc);
	if (!flappy_repository_is_remote(repoloc))
		return ENOTSUP;

	flappy_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	flappy_dictionary_get_cstring_nocopy(repo_pkgd, "architecture", &arch);

	snprintf(buf, sizeof buf, "%s/%s.%s.flappy.sig2", repoloc, pkgver, arch);
	sigsuffix = buf+(strlen(buf)-sizeof (".sig2")+1);

	flappy_set_cb_state(xhp, FLAPPY_STATE_DOWNLOAD, 0, pkgver,
		"Downloading `%s' signature (from `%s')...", pkgver, repoloc);

	if (flappy_fetch_file(xhp, buf, NULL) == -1) {
		rv = fetchLastErrCode ? fetchLastErrCode : errno;
		fetchstr = flappy_fetch_error_string();
		flappy_set_cb_state(xhp, FLAPPY_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to download `%s' signature from `%s': %s",
			pkgver, repoloc, fetchstr ? fetchstr : strerror(rv));
		return rv;
	}

	*sigsuffix = '\0';

	flappy_set_cb_state(xhp, FLAPPY_STATE_DOWNLOAD, 0, pkgver,
		"Downloading `%s' package (from `%s')...", pkgver, repoloc);

	if (flappy_fetch_file_sha256(xhp, buf, NULL, digest, sizeof digest) == -1) {
		rv = fetchLastErrCode ? fetchLastErrCode : errno;
		fetchstr = flappy_fetch_error_string();
		flappy_set_cb_state(xhp, FLAPPY_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to download `%s' package from `%s': %s",
			pkgver, repoloc, fetchstr ? fetchstr : strerror(rv));
		return rv;
	}

	flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY, 0, pkgver,
		"%s: verifying RSA signature...", pkgver);

	snprintf(buf, sizeof buf, "%s/%s.%s.flappy.sig2", xhp->cachedir, pkgver, arch);
	sigsuffix = buf+(strlen(buf)-sizeof (".sig2")+1);

	if ((repo = flappy_rpool_get_repo(repoloc)) == NULL) {
		rv = errno;
		flappy_dbg_printf("%s: failed to get repository "
			"%s: %s\n", pkgver, repoloc, strerror(errno));
		return rv;
	}

	/*
	 * If digest is not set, binary package was not downloaded,
	 * i.e. 304 not modified, verify by file instead.
	 */
	rv = 0;
	if (fetchLastErrCode == FETCH_UNCHANGED) {
		*sigsuffix = '\0';
		if (!flappy_verify_file_signature(repo, buf)) {
			rv = EPERM;
			/* remove binpkg */
			(void)remove(buf);
			/* remove signature */
			*sigsuffix = '.';
			(void)remove(buf);
		}
	} else {
		if (!flappy_verify_signature(repo, buf, digest)) {
			rv = EPERM;
			/* remove signature */
			(void)remove(buf);
			/* remove binpkg */
			*sigsuffix = '\0';
			(void)remove(buf);
		}
	}

	if (rv == EPERM) {
		flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL, rv, pkgver,
			"%s: the RSA signature is not valid!", pkgver);
		flappy_set_cb_state(xhp, FLAPPY_STATE_VERIFY_FAIL, rv, pkgver,
			"%s: removed pkg archive and its signature.", pkgver);
	}

	return rv;
}

int
flappy_transaction_fetch(struct flappy_handle *xhp, flappy_object_iterator_t iter)
{
	flappy_array_t fetch = NULL, verify = NULL;
	flappy_object_t obj;
	flappy_trans_type_t ttype;
	const char *repoloc;
	int rv = 0;
	unsigned int i, n;

	flappy_object_iterator_reset(iter);

	while ((obj = flappy_object_iterator_next(iter)) != NULL) {
		ttype = flappy_transaction_pkg_type(obj);
		if (ttype == FLAPPY_TRANS_REMOVE || ttype == FLAPPY_TRANS_HOLD ||
		    ttype == FLAPPY_TRANS_CONFIGURE) {
			continue;
		}
		flappy_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);

		/*
		 * Download binary package and signature if either one
		 * of them don't exist.
		 */
		if (flappy_repository_is_remote(repoloc) &&
		    !flappy_remote_binpkg_exists(xhp, obj)) {
			if (!fetch && !(fetch = flappy_array_create())) {
				rv = errno;
				goto out;
			}
			flappy_array_add(fetch, obj);
			continue;
		}

		/*
		 * Verify binary package from local repository or cache.
		 */
		if (!verify && !(verify = flappy_array_create())) {
			rv = errno;
			goto out;
		}
		flappy_array_add(verify, obj);
	}
	flappy_object_iterator_reset(iter);

	/*
	 * Download binary packages (if they come from a remote repository)
	 * and don't exist already.
	 */
	n = flappy_array_count(fetch);
	if (n) {
		flappy_set_cb_state(xhp, FLAPPY_STATE_TRANS_DOWNLOAD, 0, NULL, NULL);
		flappy_dbg_printf("[trans] downloading %d packages.\n", n);
	}
	for (i = 0; i < n; i++) {
		if ((rv = download_binpkg(xhp, flappy_array_get(fetch, i))) != 0) {
			flappy_dbg_printf("[trans] failed to download binpkgs: "
				"%s\n", strerror(rv));
			goto out;
		}
	}

	/*
	 * Check binary package integrity.
	 */
	n = flappy_array_count(verify);
	if (n) {
		flappy_set_cb_state(xhp, FLAPPY_STATE_TRANS_VERIFY, 0, NULL, NULL);
		flappy_dbg_printf("[trans] verifying %d packages.\n", n);
	}
	for (i = 0; i < n; i++) {
		if ((rv = verify_binpkg(xhp, flappy_array_get(verify, i))) != 0) {
			flappy_dbg_printf("[trans] failed to check binpkgs: "
				"%s\n", strerror(rv));
			goto out;
		}
	}

out:
	if (fetch)
		flappy_object_release(fetch);
	if (verify)
		flappy_object_release(verify);
	return rv;
}

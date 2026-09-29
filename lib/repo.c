/*-
 * Copyright (c) 2012-2020 Juan Romero Pardines.
 * Copyright (c) 2023 Duncan Overbruck <mail@duncano.de>.
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

#include <sys/file.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include "flappy_api_impl.h"

/**
 * @file lib/repo.c
 * @brief Repository functions
 * @defgroup repo Repository functions
 */

int
flappy_repo_lock(const char *repodir, const char *arch)
{
	char path[PATH_MAX];
	int fd;
	int r;

	if (flappy_repository_is_remote(repodir))
		return -EINVAL;

	r = snprintf(path, sizeof(path), "%s/%s-repodata.lock", repodir, arch);
	if (r < 0 || (size_t)r > sizeof(path)) {
		return -ENAMETOOLONG;
	}

	fd = open(path, O_WRONLY|O_CREAT|O_CLOEXEC, 0660);
	if (fd == -1)
		return -errno;

	if (flock(fd, LOCK_EX|LOCK_NB) == 0)
		return fd;
	if (errno != EWOULDBLOCK) {
		r = -errno;
		close(fd);
		return r;
	}

	flappy_warn_printf("repository locked: %s: waiting...", repodir);
	if (flock(fd, LOCK_EX) == -1) {
		r = -errno;
		close(fd);
		return r;
	}

	return fd;
}

void
flappy_repo_unlock(const char *repodir, const char *arch, int fd)
{
	char path[PATH_MAX];
	int r;

	if (fd != -1)
		close(0);

	r = snprintf(path, sizeof(path), "%s/%s-repodata.lock", repodir, arch);
	if (r < 0 || (size_t)r > sizeof(path))
		return;
	unlink(path);
}

static int
repo_read_next(struct flappy_repo *repo, struct archive *ar, struct archive_entry **entry)
{
	int r;

	r = archive_read_next_header(ar, entry);
	if (r == ARCHIVE_FATAL) {
		flappy_error_printf("failed to read repository: %s: %s\n",
		    repo->uri, archive_error_string(ar));
		return -flappy_archive_errno(ar);
	} else if (r == ARCHIVE_WARN) {
		flappy_warn_printf("reading repository: %s: %s\n",
		    repo->uri, archive_error_string(ar));
		return 0;
	} else if (r == ARCHIVE_EOF) {
		return -EIO;
	}
	return 0;
}

static int
repo_read_index(struct flappy_repo *repo, struct archive *ar)
{
	struct archive_entry *entry;
	const char *pname;
	char *buf;
	int r;

	r = repo_read_next(repo, ar, &entry);
	if (r < 0)
		return r;

	/* index.plist */
	pname = archive_entry_pathname(entry);
	if (!pname)
		flappy_unreachable();
	if (strcmp(pname, FLAPPY_REPODATA_INDEX) != 0) {
		flappy_error_printf("failed to read repository index: %s: unexpected archive entry\n",
		    repo->uri);
		r = -EINVAL;
		return r;
	}

	if (archive_entry_size(entry) == 0) {
		r = archive_read_data_skip(ar);
		if (r == ARCHIVE_FATAL) {
			flappy_error_printf("failed to read repository: %s: archive error: %s\n",
			    repo->uri, archive_error_string(ar));
			return -flappy_archive_errno(ar);
		}
		repo->index = flappy_dictionary_create();
		return 0;
	}

	buf = flappy_archive_get_file(ar, entry);
	if (!buf) {
		r = -errno;
		flappy_error_printf(
		    "failed to open repository: %s: failed to read index: %s\n",
		    repo->uri, strerror(-r));
		return r;
	}
	repo->index = flappy_dictionary_internalize(buf);
	r = -errno;
	free(buf);
	if (!repo->index) {
		if (!r)
			r = -EINVAL;
		flappy_error_printf(
		    "failed to open repository: %s: failed to parse index: %s\n",
		    repo->uri, strerror(-r));
		return r;
	}

	flappy_dictionary_make_immutable(repo->index);
	return 0;
}

static int
repo_read_meta(struct flappy_repo *repo, struct archive *ar)
{
	struct archive_entry *entry;
	const char *pname;
	char *buf;
	int r;

	r = repo_read_next(repo, ar, &entry);
	if (r < 0)
		return r;

	pname = archive_entry_pathname(entry);
	if (!pname)
		flappy_unreachable();
	if (strcmp(pname, FLAPPY_REPODATA_META) != 0) {
		flappy_error_printf("failed to read repository metadata: %s: unexpected archive entry\n",
		    repo->uri);
		r = -EINVAL;
		return r;
	}
	if (archive_entry_size(entry) == 0) {
		r = archive_read_data_skip(ar);
		if (r == ARCHIVE_FATAL) {
			flappy_error_printf("failed to read repository: %s: archive error: %s\n",
			    repo->uri, archive_error_string(ar));
			return -flappy_archive_errno(ar);
		}
		repo->idxmeta = NULL;
		return 0;
	}

	buf = flappy_archive_get_file(ar, entry);
	if (!buf) {
		r = -errno;
		flappy_error_printf(
		    "failed to read repository metadata: %s: failed to read "
		    "metadata: %s\n",
		    repo->uri, strerror(-r));
		return r;
	}
	/* for backwards compatibility check if the content is DEADBEEF. */
	if (strcmp(buf, "DEADBEEF") == 0) {
		free(buf);
		return 0;
	}

	errno = 0;
	repo->idxmeta = flappy_dictionary_internalize(buf);
	r = -errno;
	free(buf);
	if (!repo->idxmeta) {
		if (!r)
			r = -EINVAL;
		flappy_error_printf(
		    "failed to read repository metadata: %s: failed to parse "
		    "metadata: %s\n",
		    repo->uri, strerror(-r));
		return r;
	}

	repo->is_signed = true;
	flappy_dictionary_make_immutable(repo->idxmeta);
	return 0;
}

static int
repo_read_stage(struct flappy_repo *repo, struct archive *ar)
{
	struct archive_entry *entry;
	const char *pname;
	int r;

	r = repo_read_next(repo, ar, &entry);
	if (r < 0) {
		/* XXX: backwards compatibility missing */
		if (r == -EIO) {
			repo->stage = flappy_dictionary_create();
			return 0;
		}
		return r;
	}

	pname = archive_entry_pathname(entry);
	if (!pname)
		flappy_unreachable();
	if (strcmp(pname, FLAPPY_REPODATA_STAGE) != 0) {
		flappy_error_printf("failed to read repository stage: %s: unexpected archive entry\n",
		    repo->uri);
		r = -EINVAL;
		return r;
	}
	if (archive_entry_size(entry) == 0) {
		repo->stage = flappy_dictionary_create();
		return 0;
	}

	repo->stage = flappy_archive_get_dictionary(ar, entry);
	if (!repo->stage) {
		flappy_error_printf("failed to open repository: %s: reading stage: %s\n",
		    repo->uri, archive_error_string(ar));
		return -EIO;
	}
	flappy_dictionary_make_immutable(repo->stage);
	return 0;
}

static int
repo_read(struct flappy_repo *repo, struct archive *ar)
{
	int r;

	r = repo_read_index(repo, ar);
	if (r < 0)
		return r;
	r = repo_read_meta(repo, ar);
	if (r < 0)
		return r;
	r = repo_read_stage(repo, ar);
	if (r < 0)
		return r;

	return r;
}

static int
repo_open_local(struct flappy_repo *repo, struct archive *ar)
{
	char path[PATH_MAX];
	int r;

	if (repo->is_remote) {
		char *cachedir;
		cachedir = flappy_get_remote_repo_string(repo->uri);
		if (!cachedir) {
			r = -EINVAL;
			flappy_error_printf("failed to open repository: %s: invalid repository url\n",
			    repo->uri);
			goto err;
		}
		r = snprintf(path, sizeof(path), "%s/%s/%s-repodata",
		    repo->xhp->metadir, cachedir, repo->arch);
		free(cachedir);
	} else {
		r = snprintf(path, sizeof(path), "%s/%s-repodata", repo->uri, repo->arch);
	}
	if (r < 0 || (size_t)r >= sizeof(path)) {
		r = -ENAMETOOLONG;
		flappy_error_printf("failed to open repository: %s: repository path too long\n",
		    repo->uri);
		goto err;
	}

	r = flappy_archive_read_open(ar, path);
	if (r < 0) {
		if (r != -ENOENT) {
			flappy_error_printf("failed to open repodata: %s: %s\n",
			    path, strerror(-r));
		}
		goto err;
	}

	return 0;
err:
	return r;
}

static int
repo_open_remote(struct flappy_repo *repo, struct archive *ar)
{
	char url[PATH_MAX];
	int r;

	r = snprintf(url, sizeof(url), "%s/%s-repodata", repo->uri, repo->arch);
	if (r < 0 || (size_t)r >= sizeof(url)) {
		flappy_error_printf("failed to open repository: %s: repository url too long\n",
		    repo->uri);
		return -ENAMETOOLONG;
	}

	r = flappy_archive_read_open_remote(ar, url);
	if (r < 0) {
		flappy_error_printf("failed to open repository: %s: %s\n", repo->uri, strerror(-r));
		return r;
	}

	return 0;
}

static int
repo_open(struct flappy_handle *xhp, struct flappy_repo *repo)
{
	struct archive *ar;
	int r;

	ar = flappy_archive_read_new();
	if (!ar) {
		r = -errno;
		flappy_error_printf("failed to open repo: %s\n", strerror(-r));
		return r;
	}

	if (repo->is_remote && (xhp->flags & FLAPPY_FLAG_REPOS_MEMSYNC))
		r = repo_open_remote(repo, ar);
	else
		r = repo_open_local(repo, ar);
	if (r < 0)
		goto err;

	r = repo_read(repo, ar);
	if (r < 0)
		goto err;

	r = archive_read_close(ar);
	if (r < 0) {
		flappy_error_printf("failed to open repository: %s: closing archive: %s\n",
		    repo->uri, archive_error_string(ar));
		goto err;
	}

	archive_read_free(ar);
	return 0;
err:
	archive_read_free(ar);
	return r;
}

bool
flappy_repo_store(struct flappy_handle *xhp, const char *repo)
{
	char *url = NULL;

	assert(xhp);
	assert(repo);

	if (xhp->repositories == NULL) {
		xhp->repositories = flappy_array_create();
		assert(xhp->repositories);
	}
	/*
	 * If it's a local repo and path is relative, make it absolute.
	 */
	if (!flappy_repository_is_remote(repo)) {
		if (repo[0] != '/' && repo[0] != '\0') {
			if ((url = realpath(repo, NULL)) == NULL)
				flappy_dbg_printf("[repo] %s: realpath %s\n", __func__, repo);
		}
	}
	if (flappy_match_string_in_array(xhp->repositories, url ? url : repo)) {
		flappy_dbg_printf("[repo] `%s' already stored\n", url ? url : repo);
		if (url)
			free(url);
		return false;
	}
	if (flappy_array_add_cstring(xhp->repositories, url ? url : repo)) {
		flappy_dbg_printf("[repo] `%s' stored successfully\n", url ? url : repo);
		if (url)
			free(url);
		return true;
	}
	if (url)
		free(url);

	return false;
}

bool
flappy_repo_remove(struct flappy_handle *xhp, const char *repo)
{
	char *url;
	bool rv = false;

	assert(xhp);
	assert(repo);

	if (xhp->repositories == NULL)
		return false;

	url = strdup(repo);
	if (flappy_remove_string_from_array(xhp->repositories, repo)) {
		if (url)
			flappy_dbg_printf("[repo] `%s' removed\n", url);
		rv = true;
	}
	free(url);

	return rv;
}

static int
repo_merge_stage(struct flappy_repo *repo)
{
	flappy_dictionary_t idx;
	flappy_object_t keysym;
	flappy_object_iterator_t iter;
	int r = 0;

	idx = flappy_dictionary_copy_mutable(repo->index);
	if (!idx)
		return -errno;

	iter = flappy_dictionary_iterator(repo->stage);
	if (!iter) {
		r = -errno;
		goto err1;
	}

	while ((keysym = flappy_object_iterator_next(iter))) {
		const char *pkgname = flappy_dictionary_keysym_cstring_nocopy(keysym);
		flappy_dictionary_t pkgd = flappy_dictionary_get_keysym(repo->stage, keysym);
		if (!flappy_dictionary_set(idx, pkgname, pkgd)) {
			r = -errno;
			goto err2;
		}
	}

	flappy_object_iterator_release(iter);
	repo->idx = idx;
	return 0;
err2:
	flappy_object_iterator_release(iter);
err1:
	flappy_object_release(idx);
	return r;
}

struct flappy_repo *
flappy_repo_open(struct flappy_handle *xhp, const char *url)
{
	struct flappy_repo *repo;
	int r;

	repo = calloc(1, sizeof(*repo));
	if (!repo) {
		r = -errno;
		flappy_error_printf("failed to open repository: %s\n", strerror(-r));
		errno = -r;
		return NULL;
	}
	repo->xhp = xhp;
	repo->uri = url;
	repo->arch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;
	repo->is_remote = flappy_repository_is_remote(url);

	r = repo_open(xhp, repo);
	if (r < 0) {
		free(repo);
		errno = -r;
		return NULL;
	}

	if (flappy_dictionary_count(repo->stage) == 0 ||
	    (repo->is_remote && !(xhp->flags & FLAPPY_FLAG_USE_STAGE))) {
		repo->idx = repo->index;
		flappy_object_retain(repo->idx);
		return repo;
	}

	r = repo_merge_stage(repo);
	if (r < 0) {
		flappy_error_printf(
		    "failed to open repository: %s: could not merge stage: %s\n",
		    url, strerror(-r));
		flappy_repo_release(repo);
		errno = -r;
		return NULL;
	}

	return repo;
}


void
flappy_repo_release(struct flappy_repo *repo)
{
	if (!repo)
		return;

	if (repo->idx) {
		flappy_object_release(repo->idx);
		repo->idx = NULL;
	}
	if (repo->index) {
		flappy_object_release(repo->index);
		repo->idx = NULL;
	}
	if (repo->stage) {
		flappy_object_release(repo->stage);
		repo->idx = NULL;
	}
	if (repo->idxmeta) {
		flappy_object_release(repo->idxmeta);
		repo->idxmeta = NULL;
	}
	free(repo);
}

flappy_dictionary_t
flappy_repo_get_virtualpkg(struct flappy_repo *repo, const char *pkg)
{
	flappy_dictionary_t pkgd;
	const char *pkgver;
	char pkgname[FLAPPY_NAME_SIZE] = {0};

	if (!repo || !repo->idx || !pkg) {
		return NULL;
	}
	pkgd = flappy_find_virtualpkg_in_dict(repo->xhp, repo->idx, pkg);
	if (!pkgd) {
		return NULL;
	}
	if (flappy_dictionary_get(pkgd, "repository") && flappy_dictionary_get(pkgd, "pkgname")) {
		return pkgd;
	}
	if (!flappy_dictionary_set_cstring_nocopy(pkgd, "repository", repo->uri)) {
		return NULL;
	}
	if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		return NULL;
	}
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		return NULL;
	}
	if (!flappy_dictionary_set_cstring(pkgd, "pkgname", pkgname)) {
		return NULL;
	}
	flappy_dbg_printf("%s: found %s\n", __func__, pkgver);

	return pkgd;
}

flappy_dictionary_t
flappy_repo_get_pkg(struct flappy_repo *repo, const char *pkg)
{
	flappy_dictionary_t pkgd = NULL;
	const char *pkgver;
	char pkgname[FLAPPY_NAME_SIZE] = {0};

	if (!repo || !repo->idx || !pkg) {
		return NULL;
	}
	/* Try matching vpkg from configuration files */
	if ((pkgd = flappy_find_virtualpkg_in_conf(repo->xhp, repo->idx, pkg))) {
		goto add;
	}
	/* ... otherwise match a real pkg */
	if ((pkgd = flappy_find_pkg_in_dict(repo->idx, pkg))) {
		goto add;
	}
	return NULL;
add:
	if (flappy_dictionary_get(pkgd, "repository") && flappy_dictionary_get(pkgd, "pkgname")) {
		return pkgd;
	}
	if (!flappy_dictionary_set_cstring_nocopy(pkgd, "repository", repo->uri)) {
		return NULL;
	}
	if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		return NULL;
	}
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		return NULL;
	}
	if (!flappy_dictionary_set_cstring(pkgd, "pkgname", pkgname)) {
		return NULL;
	}
	flappy_dbg_printf("%s: found %s\n", __func__, pkgver);
	return pkgd;
}

static flappy_array_t
revdeps_match(struct flappy_repo *repo, flappy_dictionary_t tpkgd, const char *str)
{
	flappy_dictionary_t pkgd;
	flappy_array_t revdeps = NULL, pkgdeps, provides;
	flappy_object_iterator_t iter;
	flappy_object_t obj;
	const char *pkgver = NULL, *tpkgver = NULL, *arch = NULL, *vpkg = NULL;

	iter = flappy_dictionary_iterator(repo->idx);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		pkgd = flappy_dictionary_get_keysym(repo->idx, obj);
		if (flappy_dictionary_equals(pkgd, tpkgd))
			continue;

		pkgdeps = flappy_dictionary_get(pkgd, "run_depends");
		if (!flappy_array_count(pkgdeps))
			continue;
		/*
		 * Try to match passed in string.
		 */
		if (str) {
			if (!flappy_match_pkgdep_in_array(pkgdeps, str))
				continue;
			flappy_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!flappy_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			flappy_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = flappy_array_create();

			if (!flappy_match_string_in_array(revdeps, tpkgver))
				flappy_array_add_cstring_nocopy(revdeps, tpkgver);

			continue;
		}
		/*
		 * Try to match any virtual package.
		 */
		provides = flappy_dictionary_get(tpkgd, "provides");
		for (unsigned int i = 0; i < flappy_array_count(provides); i++) {
			flappy_array_get_cstring_nocopy(provides, i, &vpkg);
			if (!flappy_match_pkgdep_in_array(pkgdeps, vpkg))
				continue;

			flappy_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!flappy_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver",
			    &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = flappy_array_create();

			if (!flappy_match_string_in_array(revdeps, tpkgver))
				flappy_array_add_cstring_nocopy(revdeps, tpkgver);
		}
		/*
		 * Try to match by pkgver.
		 */
		flappy_dictionary_get_cstring_nocopy(tpkgd, "pkgver", &pkgver);
		if (!flappy_match_pkgdep_in_array(pkgdeps, pkgver))
			continue;

		flappy_dictionary_get_cstring_nocopy(pkgd,
		    "architecture", &arch);
		if (!flappy_pkg_arch_match(repo->xhp, arch, NULL))
			continue;

		flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &tpkgver);
		/* match */
		if (revdeps == NULL)
			revdeps = flappy_array_create();

		if (!flappy_match_string_in_array(revdeps, tpkgver))
			flappy_array_add_cstring_nocopy(revdeps, tpkgver);
	}
	flappy_object_iterator_release(iter);
	return revdeps;
}

flappy_array_t
flappy_repo_get_pkg_revdeps(struct flappy_repo *repo, const char *pkg)
{
	flappy_array_t revdeps = NULL, vdeps = NULL;
	flappy_dictionary_t pkgd;
	const char *vpkg;
	bool match = false;

	if (repo->idx == NULL)
		return NULL;

	if (((pkgd = flappy_repo_get_pkg(repo, pkg)) == NULL) &&
	    ((pkgd = flappy_repo_get_virtualpkg(repo, pkg)) == NULL)) {
		errno = ENOENT;
		return NULL;
	}
	/*
	 * If pkg is a virtual pkg let's match it instead of the real pkgver.
	 */
	if ((vdeps = flappy_dictionary_get(pkgd, "provides"))) {
		for (unsigned int i = 0; i < flappy_array_count(vdeps); i++) {
			char vpkgn[FLAPPY_NAME_SIZE];

			flappy_array_get_cstring_nocopy(vdeps, i, &vpkg);
			if (!flappy_pkg_name(vpkgn, FLAPPY_NAME_SIZE, vpkg)) {
				abort();
			}
			if (strcmp(vpkgn, pkg) == 0) {
				match = true;
				break;
			}
			vpkg = NULL;
		}
		if (match)
			revdeps = revdeps_match(repo, pkgd, vpkg);
	}
	if (!match)
		revdeps = revdeps_match(repo, pkgd, NULL);

	return revdeps;
}

int
flappy_repo_key_import(struct flappy_repo *repo)
{
	flappy_dictionary_t repokeyd = NULL;
	flappy_data_t pubkey = NULL;
	uint16_t pubkey_size = 0;
	const char *signedby = NULL;
	char *hexfp = NULL;
	char *p, *dbkeyd, *rkeyfile = NULL;
	int import, rv = 0;

	assert(repo);
	/*
	 * If repository does not have required metadata plist, ignore it.
	 */
	if (!flappy_dictionary_count(repo->idxmeta)) {
		flappy_dbg_printf("[repo] `%s' unsigned repository!\n", repo->uri);
		return 0;
	}
	/*
	 * Check for required objects in index-meta:
	 * 	- signature-by (string)
	 * 	- public-key (data)
	 * 	- public-key-size (number)
	 */
	flappy_dictionary_get_cstring_nocopy(repo->idxmeta, "signature-by", &signedby);
	flappy_dictionary_get_uint16(repo->idxmeta, "public-key-size", &pubkey_size);
	pubkey = flappy_dictionary_get(repo->idxmeta, "public-key");

	if (signedby == NULL || pubkey_size == 0 ||
	    flappy_object_type(pubkey) != FLAPPY_TYPE_DATA) {
		flappy_dbg_printf("[repo] `%s': incomplete signed repository "
		    "(missing objs)\n", repo->uri);
		rv = EINVAL;
		goto out;
	}
	hexfp = flappy_pubkey2fp(pubkey);
	if (hexfp == NULL) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Check if the public key is alredy stored.
	 */
	rkeyfile = flappy_xasprintf("%s/keys/%s.plist", repo->xhp->metadir, hexfp);
	repokeyd = flappy_plist_dictionary_from_file(rkeyfile);
	if (flappy_object_type(repokeyd) == FLAPPY_TYPE_DICTIONARY) {
		flappy_dbg_printf("[repo] `%s' public key already stored.\n", repo->uri);
		goto out;
	}
	/*
	 * Notify the client and take appropiate action to import
	 * the repository public key. Pass back the public key openssh fingerprint
	 * to the client.
	 */
	import = flappy_set_cb_state(repo->xhp, FLAPPY_STATE_REPO_KEY_IMPORT, 0,
			hexfp, "`%s' repository has been RSA signed by \"%s\"",
			repo->uri, signedby);
	if (import <= 0) {
		rv = EAGAIN;
		goto out;
	}

	p = strdup(rkeyfile);
	dbkeyd = dirname(p);
	assert(dbkeyd);
	if (access(dbkeyd, R_OK|W_OK) == -1) {
		rv = errno;
		if (rv == ENOENT)
			rv = flappy_mkpath(dbkeyd, 0755);
		if (rv != 0) {
			rv = errno;
			flappy_dbg_printf("[repo] `%s' cannot create %s: %s\n",
			    repo->uri, dbkeyd, strerror(errno));
			free(p);
			goto out;
		}
	}
	free(p);

	repokeyd = flappy_dictionary_create();
	flappy_dictionary_set(repokeyd, "public-key", pubkey);
	flappy_dictionary_set_uint16(repokeyd, "public-key-size", pubkey_size);
	flappy_dictionary_set_cstring_nocopy(repokeyd, "signature-by", signedby);

	if (!flappy_dictionary_externalize_to_file(repokeyd, rkeyfile)) {
		rv = errno;
		flappy_dbg_printf("[repo] `%s' failed to externalize %s: %s\n",
		    repo->uri, rkeyfile, strerror(rv));
	}

out:
	if (hexfp)
		free(hexfp);
	if (repokeyd)
		flappy_object_release(repokeyd);
	if (rkeyfile)
		free(rkeyfile);
	return rv;
}

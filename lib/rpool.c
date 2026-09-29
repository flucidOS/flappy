/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
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

#include <sys/utsname.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>

#include "flappy_api_impl.h"
#include "fetch.h"

struct rpool_fpkg {
	flappy_array_t revdeps;
	flappy_dictionary_t pkgd;
	const char *pattern;
	const char *bestpkgver;
	bool best;
};

typedef enum {
	BEST_PKG = 1,
	VIRTUAL_PKG,
	REAL_PKG,
	REVDEPS_PKG
} pkg_repo_type_t;

static SIMPLEQ_HEAD(rpool_head, flappy_repo) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

/**
 * @file lib/rpool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

int
flappy_rpool_sync(struct flappy_handle *xhp, const char *uri)
{
	const char *repouri = NULL;

	for (unsigned int i = 0; i < flappy_array_count(xhp->repositories); i++) {
		flappy_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		/* If argument was set just process that repository */
		if (uri && strcmp(repouri, uri))
			continue;

		if (flappy_repo_sync(xhp, repouri) == -1) {
			flappy_dbg_printf(
			    "[rpool] `%s' failed to fetch repository data: %s\n",
			    repouri, fetchLastErrCode == 0 ? strerror(errno) :
			    flappy_fetch_error_string());
			continue;
		}
	}
	return 0;
}

struct flappy_repo HIDDEN *
flappy_regget_repo(struct flappy_handle *xhp, const char *url)
{
	struct flappy_repo *repo;
	const char *repouri = NULL;

	if (SIMPLEQ_EMPTY(&rpool_queue)) {
		/* iterate until we have a match */
		for (unsigned int i = 0; i < flappy_array_count(xhp->repositories); i++) {
			flappy_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
			if (strcmp(repouri, url))
				continue;

			repo = flappy_repo_open(xhp, repouri);
			if (!repo)
				return NULL;

			SIMPLEQ_INSERT_TAIL(&rpool_queue, repo, entries);
			flappy_dbg_printf("[rpool] `%s' registered.\n", repouri);
		}
	}
	SIMPLEQ_FOREACH(repo, &rpool_queue, entries)
		if (strcmp(url, repo->uri) == 0)
			return repo;

	return NULL;
}

struct flappy_repo *
flappy_rpool_get_repo(const char *url)
{
	struct flappy_repo *repo;

	SIMPLEQ_FOREACH(repo, &rpool_queue, entries)
		if (strcmp(url, repo->uri) == 0)
			return repo;

	return NULL;
}

void
flappy_rpool_release(struct flappy_handle *xhp)
{
	struct flappy_repo *repo;

	while ((repo = SIMPLEQ_FIRST(&rpool_queue))) {
	       SIMPLEQ_REMOVE(&rpool_queue, repo, flappy_repo, entries);
	       flappy_repo_release(repo);
	}
	if (xhp && xhp->repositories) {
		flappy_object_release(xhp->repositories);
		xhp->repositories = NULL;
	}
}

int
flappy_rpool_foreach(struct flappy_handle *xhp,
	int (*fn)(struct flappy_repo *, void *, bool *),
	void *arg)
{
	struct flappy_repo *repo = NULL;
	const char *repouri = NULL;
	int rv = 0;
	bool foundrepo = false, done = false;
	unsigned int n = 0;

	assert(fn != NULL);

again:
	for (unsigned int i = n; i < flappy_array_count(xhp->repositories); i++, n++) {
		flappy_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		flappy_dbg_printf("[rpool] checking `%s' at index %u\n", repouri, n);
		if ((repo = flappy_rpool_get_repo(repouri)) == NULL) {
			repo = flappy_repo_open(xhp, repouri);
			if (!repo) {
				flappy_repo_remove(xhp, repouri);
				goto again;
			}
			SIMPLEQ_INSERT_TAIL(&rpool_queue, repo, entries);
			flappy_dbg_printf("[rpool] `%s' registered.\n", repouri);
		}
		foundrepo = true;
		rv = (*fn)(repo, arg, &done);
		if (rv != 0 || done)
			break;
	}
	if (!foundrepo)
		rv = ENOTSUP;

	return rv;
}

static int
find_virtualpkg_cb(struct flappy_repo *repo, void *arg, bool *done)
{
	struct rpool_fpkg *rpf = arg;

	rpf->pkgd = flappy_repo_get_virtualpkg(repo, rpf->pattern);
	if (rpf->pkgd) {
		/* found */
		*done = true;
		return 0;
	}
	/* not found */
	return 0;
}

static int
find_pkg_cb(struct flappy_repo *repo, void *arg, bool *done)
{
	struct rpool_fpkg *rpf = arg;

	rpf->pkgd = flappy_repo_get_pkg(repo, rpf->pattern);
	if (rpf->pkgd) {
		/* found */
		*done = true;
		return 0;
	}
	/* Not found */
	return 0;
}

static int
find_pkg_revdeps_cb(struct flappy_repo *repo, void *arg, bool *done UNUSED)
{
	struct rpool_fpkg *rpf = arg;
	flappy_array_t revdeps = NULL;
	const char *pkgver = NULL;

	revdeps = flappy_repo_get_pkg_revdeps(repo, rpf->pattern);
	if (flappy_array_count(revdeps)) {
		/* found */
		if (rpf->revdeps == NULL)
			rpf->revdeps = flappy_array_create();
		for (unsigned int i = 0; i < flappy_array_count(revdeps); i++) {
			flappy_array_get_cstring_nocopy(revdeps, i, &pkgver);
			flappy_array_add_cstring_nocopy(rpf->revdeps, pkgver);
		}
		flappy_object_release(revdeps);
	}
	return 0;
}

static int
find_best_pkg_cb(struct flappy_repo *repo, void *arg, bool *done UNUSED)
{
	struct rpool_fpkg *rpf = arg;
	flappy_dictionary_t pkgd;
	const char *repopkgver = NULL;

	pkgd = flappy_repo_get_pkg(repo, rpf->pattern);
	if (pkgd == NULL) {
		if (errno && errno != ENOENT)
			return errno;

		flappy_dbg_printf("[rpool] Package '%s' not found in repository"
		    " '%s'.\n", rpf->pattern, repo->uri);
		return 0;
	}
	flappy_dictionary_get_cstring_nocopy(pkgd,
	    "pkgver", &repopkgver);
	if (rpf->bestpkgver == NULL) {
		flappy_dbg_printf("[rpool] Found match '%s' (%s).\n",
		    repopkgver, repo->uri);
		rpf->pkgd = pkgd;
		rpf->bestpkgver = repopkgver;
		return 0;
	}
	/*
	 * Compare current stored version against new
	 * version from current package in repository.
	 */
	if (flappy_cmpver(repopkgver, rpf->bestpkgver) == 1) {
		flappy_dbg_printf("[rpool] Found best match '%s' (%s).\n",
		    repopkgver, repo->uri);
		rpf->pkgd = pkgd;
		rpf->bestpkgver = repopkgver;
	}
	return 0;
}

static flappy_object_t
repo_find_pkg(struct flappy_handle *xhp,
	      const char *pkg,
	      pkg_repo_type_t type)
{
	struct rpool_fpkg rpf;
	int rv = 0;

	assert(xhp);
	assert(pkg);

	rpf.pattern = pkg;
	rpf.pkgd = NULL;
	rpf.revdeps = NULL;
	rpf.bestpkgver = NULL;

	switch (type) {
	case BEST_PKG:
		/*
		 * Find best pkg version.
		 */
		rv = flappy_rpool_foreach(xhp, find_best_pkg_cb, &rpf);
		break;
	case VIRTUAL_PKG:
		/*
		 * Find virtual pkg.
		 */
		rv = flappy_rpool_foreach(xhp, find_virtualpkg_cb, &rpf);
		break;
	case REAL_PKG:
		/*
		 * Find real pkg.
		 */
		rv = flappy_rpool_foreach(xhp, find_pkg_cb, &rpf);
		break;
	case REVDEPS_PKG:
		/*
		 * Find revdeps for pkg.
		 */
		rv = flappy_rpool_foreach(xhp, find_pkg_revdeps_cb, &rpf);
		break;
	}
	if (rv != 0) {
		errno = rv;
		return NULL;
	}
	if (type == REVDEPS_PKG) {
		if (rpf.revdeps == NULL)
			errno = ENOENT;

		return rpf.revdeps;
	} else {
		if (rpf.pkgd == NULL)
			errno = ENOENT;
	}
	return rpf.pkgd;
}

flappy_dictionary_t
flappy_rpool_get_virtualpkg(struct flappy_handle *xhp, const char *pkg)
{
	return repo_find_pkg(xhp, pkg, VIRTUAL_PKG);
}

flappy_dictionary_t
flappy_rpool_get_pkg(struct flappy_handle *xhp, const char *pkg)
{
	if (xhp->flags & FLAPPY_FLAG_BESTMATCH)
		return repo_find_pkg(xhp, pkg, BEST_PKG);

	return repo_find_pkg(xhp, pkg, REAL_PKG);
}

flappy_array_t
flappy_rpool_get_pkg_revdeps(struct flappy_handle *xhp, const char *pkg)
{
	return repo_find_pkg(xhp, pkg, REVDEPS_PKG);
}

flappy_array_t
flappy_rpool_get_pkg_fulldeptree(struct flappy_handle *xhp, const char *pkg)
{
	return flappy_get_pkg_fulldeptree(xhp, pkg, true);
}

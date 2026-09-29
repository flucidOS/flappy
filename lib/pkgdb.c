/*-
 * Copyright (c) 2012-2020 Juan Romero Pardines.
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
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "flappy.h"
#include "flappy_api_impl.h"

/**
 * @file lib/pkgdb.c
 * @brief Package database handling routines
 * @defgroup pkgdb Package database handling functions
 *
 * Functions to manipulate the main package database plist file (pkgdb).
 *
 * The following image shown below shows the proplib structure used
 * by the main package database plist:
 *
 * @image html images/flappy_pkgdb_dictionary.png
 *
 * Legend:
 *  - <b>Salmon filled box</b>: \a pkgdb plist internalized.
 *  - <b>White filled box</b>: mandatory objects.
 *  - <b>Grey filled box</b>: optional objects.
 *  - <b>Green filled box</b>: possible value set in the object, only one
 *    of them is set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e array, bool, integer, string,
 * dictionary.
 */

int
flappy_pkgdb_lock(struct flappy_handle *xhp)
{
	char path[PATH_MAX];
	mode_t prev_umask;

	if (flappy_path_join(path, sizeof(path), xhp->metadir, "lock", (char *)NULL) == -1) {
		return flappy_error_errno(errno,
		    "failed to create lockfile path: %s\n", strerror(errno));
	}

	prev_umask = umask(022);
	if (flappy_mkpath(xhp->metadir, 0755) == -1 && errno != EEXIST) {
		umask(prev_umask);
		return flappy_error_errno(errno,
		    "failed to create metadir: %s: %s\n",
		    xhp->metadir, strerror(errno));
	}

	xhp->lock_fd = open(path, O_CREAT|O_WRONLY|O_CLOEXEC, 0664);
	if (xhp->lock_fd == -1) {
		return flappy_error_errno(errno,
		    "failed to lock package database: %s\n", strerror(errno));
	}
	umask(prev_umask);

	if (flock(xhp->lock_fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno != EWOULDBLOCK)
			goto err;
		flappy_warn_printf("package database locked, waiting...\n");
	}

	if (flock(xhp->lock_fd, LOCK_EX) == -1) {
err:
		close(xhp->lock_fd);
		xhp->lock_fd = -1;
		return flappy_error_errno(errno, "failed to lock file: %s: %s\n",
		    path, strerror(errno));
	}

	return 0;
}

void
flappy_pkgdb_unlock(struct flappy_handle *xhp)
{
	if (xhp->lock_fd == -1)
		return;
	close(xhp->lock_fd);
	xhp->lock_fd = -1;
}

static int
pkgdb_map_vpkgs(struct flappy_handle *xhp)
{
	flappy_object_iterator_t iter;
	flappy_object_t obj;
	int r = 0;

	if (!flappy_dictionary_count(xhp->pkgdb))
		return 0;

	if (xhp->vpkgd == NULL) {
		xhp->vpkgd = flappy_dictionary_create();
		if (!xhp->vpkgd) {
			r = -errno;
			flappy_error_printf("failed to create dictionary\n");
			return r;
		}
	}

	/*
	 * This maps all pkgs that have virtualpkgs in pkgdb.
	 */
	iter = flappy_dictionary_iterator(xhp->pkgdb);
	if (!iter) {
		r = -errno;
		flappy_error_printf("failed to create iterator");
		return r;
	}

	while ((obj = flappy_object_iterator_next(iter))) {
		flappy_array_t provides;
		flappy_dictionary_t pkgd;
		const char *pkgver = NULL;
		const char *pkgname = NULL;
		unsigned int cnt;

		pkgd = flappy_dictionary_get_keysym(xhp->pkgdb, obj);
		provides = flappy_dictionary_get(pkgd, "provides");
		cnt = flappy_array_count(provides);
		if (!cnt)
			continue;

		flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		flappy_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);
		assert(pkgname);

		for (unsigned int i = 0; i < cnt; i++) {
			char vpkgname[FLAPPY_NAME_SIZE];
			const char *vpkg = NULL;
			flappy_dictionary_t providers;
			bool alloc = false;

			flappy_array_get_cstring_nocopy(provides, i, &vpkg);
			if (!flappy_pkg_name(vpkgname, sizeof(vpkgname), vpkg)) {
				flappy_warn_printf("%s: invalid provides: %s\n", pkgver, vpkg);
				continue;
			}

			providers = flappy_dictionary_get(xhp->vpkgd, vpkgname);
			if (!providers) {
				providers = flappy_dictionary_create();
				if (!providers) {
					r = -errno;
					flappy_error_printf("failed to create dictionary\n");
					goto out;
				}
				if (!flappy_dictionary_set(xhp->vpkgd, vpkgname, providers)) {
					r = -errno;
					flappy_error_printf("failed to set dictionary entry\n");
					flappy_object_release(providers);
					goto out;
				}
				alloc = true;
			}

			if (!flappy_dictionary_set_cstring(providers, vpkg, pkgname)) {
				r = -errno;
				flappy_error_printf("failed to set dictionary entry\n");
				if (alloc)
					flappy_object_release(providers);
				goto out;
			}
			if (alloc)
				flappy_object_release(providers);
			flappy_dbg_printf("[pkgdb] added vpkg %s for %s\n", vpkg, pkgname);
		}
	}
out:
	flappy_object_iterator_release(iter);
	return r;
}

static int
pkgdb_map_names(struct flappy_handle *xhp)
{
	flappy_object_iterator_t iter;
	flappy_object_t obj;
	int rv = 0;

	if (!flappy_dictionary_count(xhp->pkgdb))
		return 0;

	/*
	 * This maps all pkgs in pkgdb to have the "pkgname" string property.
	 * This way we do it once and not multiple times.
	 */
	iter = flappy_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		flappy_dictionary_t pkgd;
		const char *pkgver;
		char pkgname[FLAPPY_NAME_SIZE] = {0};

		pkgd = flappy_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
			continue;
		}
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		if (!flappy_dictionary_set_cstring(pkgd, "pkgname", pkgname)) {
			rv = EINVAL;
			break;
		}
	}
	flappy_object_iterator_release(iter);
	return rv;
}

int HIDDEN
flappy_pkgdb_init(struct flappy_handle *xhp)
{
	int rv;

	assert(xhp);

	if (xhp->pkgdb)
		return 0;

	if (!xhp->pkgdb_plist)
		xhp->pkgdb_plist = flappy_xasprintf("%s/%s", xhp->metadir, FLAPPY_PKGDB);

#if 0
	if ((rv = flappy_pkgdb_conversion(xhp)) != 0)
		return rv;
#endif


	if ((rv = flappy_pkgdb_update(xhp, false, true)) != 0) {
		if (rv != ENOENT)
			flappy_error_printf("failed to initialize pkgdb: %s\n", strerror(rv));
		return rv;
	}
	if ((rv = pkgdb_map_names(xhp)) != 0) {
		flappy_dbg_printf("[pkgdb] pkgdb_map_names %s\n", strerror(rv));
		return rv;
	}
	if ((rv = pkgdb_map_vpkgs(xhp)) != 0) {
		flappy_dbg_printf("[pkgdb] pkgdb_map_vpkgs %s\n", strerror(rv));
		return rv;
	}
	assert(xhp->pkgdb);
	flappy_dbg_printf("[pkgdb] initialized ok.\n");

	return 0;
}

int
flappy_pkgdb_update(struct flappy_handle *xhp, bool flush, bool update)
{
	flappy_dictionary_t pkgdb_storage;
	mode_t prev_umask;
	static int cached_rv;
	int rv = 0;

	if (cached_rv && !flush)
		return cached_rv;

	if (xhp->pkgdb && flush) {
		pkgdb_storage = flappy_dictionary_internalize_from_file(xhp->pkgdb_plist);
		if (pkgdb_storage == NULL ||
		    !flappy_dictionary_equals(xhp->pkgdb, pkgdb_storage)) {
			/* flush dictionary to storage */
			prev_umask = umask(022);
			if (!flappy_dictionary_externalize_to_file(xhp->pkgdb, xhp->pkgdb_plist)) {
				umask(prev_umask);
				return errno;
			}
			umask(prev_umask);
		}
		if (pkgdb_storage)
			flappy_object_release(pkgdb_storage);

		flappy_object_release(xhp->pkgdb);
		xhp->pkgdb = NULL;
		cached_rv = 0;
	}
	if (!update)
		return rv;

	/* update copy in memory */
	if ((xhp->pkgdb = flappy_dictionary_internalize_from_file(xhp->pkgdb_plist)) == NULL) {
		rv = errno;
		if (!rv)
			rv = EINVAL;

		if (rv == ENOENT)
			xhp->pkgdb = flappy_dictionary_create();
		else
			flappy_error_printf("cannot access to pkgdb: %s\n", strerror(rv));

		cached_rv = rv = errno;
	}

	return rv;
}

void HIDDEN
flappy_pkgdb_release(struct flappy_handle *xhp)
{
	assert(xhp);

	flappy_pkgdb_unlock(xhp);
	if (xhp->pkgdb)
		flappy_object_release(xhp->pkgdb);
	flappy_dbg_printf("[pkgdb] released ok.\n");
}

int
flappy_pkgdb_foreach_cb(struct flappy_handle *xhp,
		int (*fn)(struct flappy_handle *, flappy_object_t, const char *, void *, bool *),
		void *arg)
{
	flappy_array_t allkeys;
	int r;

	// XXX: this should be done before calling the function...
	if ((r = flappy_pkgdb_init(xhp)) != 0)
		return r > 0 ? -r : r;

	allkeys = flappy_dictionary_all_keys(xhp->pkgdb);
	assert(allkeys);
	r = flappy_array_foreach_cb(xhp, allkeys, xhp->pkgdb, fn, arg);
	flappy_object_release(allkeys);
	return r;
}

int
flappy_pkgdb_foreach_cb_multi(struct flappy_handle *xhp,
		int (*fn)(struct flappy_handle *, flappy_object_t, const char *, void *, bool *),
		void *arg)
{
	flappy_array_t allkeys;
	int r;

	// XXX: this should be done before calling the function...
	if ((r = flappy_pkgdb_init(xhp)) != 0)
		return r > 0 ? -r : r;

	allkeys = flappy_dictionary_all_keys(xhp->pkgdb);
	if (!allkeys)
		return flappy_error_oom();

	r = flappy_array_foreach_cb_multi(xhp, allkeys, xhp->pkgdb, fn, arg);
	flappy_object_release(allkeys);
	return r;
}

flappy_dictionary_t
flappy_pkgdb_get_pkg(struct flappy_handle *xhp, const char *pkg)
{
	flappy_dictionary_t pkgd;

	if (flappy_pkgdb_init(xhp) != 0)
		return NULL;

	pkgd = flappy_find_pkg_in_dict(xhp->pkgdb, pkg);
	if (!pkgd)
		errno = ENOENT;
	return pkgd;
}

flappy_dictionary_t
flappy_pkgdb_get_virtualpkg(struct flappy_handle *xhp, const char *vpkg)
{
	if (flappy_pkgdb_init(xhp) != 0)
		return NULL;

	return flappy_find_virtualpkg_in_dict(xhp, xhp->pkgdb, vpkg);
}

static void
generate_full_revdeps_tree(struct flappy_handle *xhp)
{
	flappy_object_t obj;
	flappy_object_iterator_t iter;
	flappy_dictionary_t vpkg_cache;

	if (xhp->pkgdb_revdeps)
		return;

	xhp->pkgdb_revdeps = flappy_dictionary_create();
	assert(xhp->pkgdb_revdeps);

	vpkg_cache = flappy_dictionary_create();
	assert(vpkg_cache);

	iter = flappy_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		flappy_array_t rundeps;
		flappy_dictionary_t pkgd;
		const char *pkgver = NULL;

		pkgd = flappy_dictionary_get_keysym(xhp->pkgdb, obj);
		rundeps = flappy_dictionary_get(pkgd, "run_depends");
		if (!flappy_array_count(rundeps))
			continue;

		flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		for (unsigned int i = 0; i < flappy_array_count(rundeps); i++) {
			flappy_array_t pkg;
			const char *pkgdep = NULL, *v;
			char curpkgname[FLAPPY_NAME_SIZE];
			bool alloc = false;

			flappy_array_get_cstring_nocopy(rundeps, i, &pkgdep);
			if ((!flappy_pkgpattern_name(curpkgname, sizeof(curpkgname), pkgdep)) &&
			    (!flappy_pkg_name(curpkgname, sizeof(curpkgname), pkgdep))) {
					abort();
			}

			/* TODO: this is kind of a workaround, to avoid calling vpkg_user_conf
			 * over and over again for the same packages which is slow. A better
			 * solution for itself vpkg_user_conf being slow should probably be
			 * implemented at some point.
			 */
			if (!flappy_dictionary_get_cstring_nocopy(vpkg_cache, curpkgname, &v)) {
				const char *vpkgname = vpkg_user_conf(xhp, curpkgname);
				if (vpkgname) {
					v = vpkgname;
				} else {
					v = curpkgname;
				}
				errno = 0;
				if (!flappy_dictionary_set_cstring_nocopy(vpkg_cache, curpkgname, v)) {
					flappy_error_printf("%s\n", strerror(errno ? errno : ENOMEM));
					abort();
				}
			}

			pkg = flappy_dictionary_get(xhp->pkgdb_revdeps, v);
			if (pkg == NULL) {
				alloc = true;
				pkg = flappy_array_create();
			}
			if (!flappy_match_string_in_array(pkg, pkgver)) {
				flappy_array_add_cstring_nocopy(pkg, pkgver);
				flappy_dictionary_set(xhp->pkgdb_revdeps, v, pkg);
			}
			if (alloc)
				flappy_object_release(pkg);
		}
	}
	flappy_object_iterator_release(iter);
	flappy_object_release(vpkg_cache);
}

flappy_array_t
flappy_pkgdb_get_pkg_revdeps(struct flappy_handle *xhp, const char *pkg)
{
	flappy_dictionary_t pkgd;
	const char *pkgver = NULL;
	char pkgname[FLAPPY_NAME_SIZE];

	if ((pkgd = flappy_pkgdb_get_pkg(xhp, pkg)) == NULL)
		return NULL;

	generate_full_revdeps_tree(xhp);
	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) 
		return NULL;

	return flappy_dictionary_get(xhp->pkgdb_revdeps, pkgname);
}

flappy_array_t
flappy_pkgdb_get_pkg_fulldeptree(struct flappy_handle *xhp, const char *pkg)
{
	return flappy_get_pkg_fulldeptree(xhp, pkg, false);
}

flappy_dictionary_t
flappy_pkgdb_get_pkg_files(struct flappy_handle *xhp, const char *pkg)
{
	flappy_dictionary_t pkgd;
	const char *pkgver = NULL;
	char pkgname[FLAPPY_NAME_SIZE], plist[PATH_MAX];

	if (pkg == NULL)
		return NULL;

	pkgd = flappy_pkgdb_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return NULL;

	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver))
		return NULL;

	snprintf(plist, sizeof(plist)-1, "%s/.%s-files.plist", xhp->metadir, pkgname);
	return flappy_plist_dictionary_from_file(plist);
}

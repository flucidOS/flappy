/*-
 * Copyright (c) 2009-2020 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "flappy_api_impl.h"

/**
 * @file lib/transaction_prepare.c
 * @brief Transaction handling routines
 * @defgroup transaction Transaction handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by flappy_transaction_prepare().
 *
 * @image html images/flappy_transaction_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: The transaction dictionary.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static int
compute_transaction_stats(struct flappy_handle *xhp)
{
	flappy_dictionary_t pkg_metad;
	flappy_object_iterator_t iter;
	flappy_object_t obj;
	struct statvfs svfs;
	uint64_t rootdir_free_size, tsize, dlsize, instsize, rmsize;
	uint32_t inst_pkgcnt, up_pkgcnt, cf_pkgcnt, rm_pkgcnt, dl_pkgcnt;
	uint32_t hold_pkgcnt;

	inst_pkgcnt = up_pkgcnt = cf_pkgcnt = rm_pkgcnt = 0;
	hold_pkgcnt = dl_pkgcnt = 0;
	tsize = dlsize = instsize = rmsize = 0;

	iter = flappy_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = flappy_object_iterator_next(iter)) != NULL) {
		const char *pkgver = NULL, *repo = NULL, *pkgname = NULL;
		bool preserve = false;
		flappy_trans_type_t ttype;
		/*
		 * Count number of pkgs to be removed, configured,
		 * installed and updated.
		 */
		flappy_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		flappy_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgname);
		flappy_dictionary_get_cstring_nocopy(obj, "repository", &repo);
		flappy_dictionary_get_bool(obj, "preserve", &preserve);
		ttype = flappy_transaction_pkg_type(obj);

		if (ttype == FLAPPY_TRANS_REMOVE) {
			rm_pkgcnt++;
		} else if (ttype == FLAPPY_TRANS_CONFIGURE) {
			cf_pkgcnt++;
		} else if (ttype == FLAPPY_TRANS_INSTALL || ttype == FLAPPY_TRANS_REINSTALL) {
			inst_pkgcnt++;
		} else if (ttype == FLAPPY_TRANS_UPDATE) {
			up_pkgcnt++;
		} else if (ttype == FLAPPY_TRANS_HOLD) {
			hold_pkgcnt++;
		}

		if ((ttype != FLAPPY_TRANS_CONFIGURE) && (ttype != FLAPPY_TRANS_REMOVE) &&
		    (ttype != FLAPPY_TRANS_HOLD) &&
		    flappy_repository_is_remote(repo) && !flappy_binpkg_exists(xhp, obj)) {
			flappy_dictionary_get_uint64(obj, "filename-size", &tsize);
			tsize += 512;
			dlsize += tsize;
			dl_pkgcnt++;
			flappy_dictionary_set_bool(obj, "download", true);
		}
		if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
			continue;
		}
		/* installed_size from repo */
		if (ttype != FLAPPY_TRANS_REMOVE && ttype != FLAPPY_TRANS_HOLD &&
		    ttype != FLAPPY_TRANS_CONFIGURE) {
			flappy_dictionary_get_uint64(obj, "installed_size", &tsize);
			instsize += tsize;
		}
		/*
		 * If removing or updating a package without preserve,
		 * get installed_size from pkgdb instead.
		 */
		if (ttype == FLAPPY_TRANS_REMOVE ||
		   ((ttype == FLAPPY_TRANS_UPDATE) && !preserve)) {
			pkg_metad = flappy_pkgdb_get_pkg(xhp, pkgname);
			if (pkg_metad == NULL)
				continue;
			flappy_dictionary_get_uint64(pkg_metad,
			    "installed_size", &tsize);
			rmsize += tsize;
		}
	}
	flappy_object_iterator_release(iter);

	if (instsize > rmsize) {
		instsize -= rmsize;
		rmsize = 0;
	} else if (rmsize > instsize) {
		rmsize -= instsize;
		instsize = 0;
	} else {
		instsize = rmsize = 0;
	}

	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-install-pkgs", inst_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-update-pkgs", up_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-configure-pkgs", cf_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-remove-pkgs", rm_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-download-pkgs", dl_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint32(xhp->transd,
				"total-hold-pkgs", hold_pkgcnt))
		return EINVAL;
	if (!flappy_dictionary_set_uint64(xhp->transd,
				"total-installed-size", instsize))
		return EINVAL;
	if (!flappy_dictionary_set_uint64(xhp->transd,
				"total-download-size", dlsize))
		return EINVAL;
	if (!flappy_dictionary_set_uint64(xhp->transd,
				"total-removed-size", rmsize))
		return EINVAL;

	/* Get free space from target rootdir: return ENOSPC if there's not enough space */
	if (statvfs(xhp->rootdir, &svfs) == -1) {
		flappy_dbg_printf("%s: statvfs failed: %s\n", __func__, strerror(errno));
		return 0;
	}
	/* compute free space on disk */
	rootdir_free_size = svfs.f_bfree * svfs.f_bsize;

	if (!flappy_dictionary_set_uint64(xhp->transd,
				"disk-free-size", rootdir_free_size))
		return EINVAL;

	if (instsize > rootdir_free_size)
		return ENOSPC;

	return 0;
}

int HIDDEN
flappy_transaction_init(struct flappy_handle *xhp)
{
	flappy_array_t array;
	flappy_dictionary_t dict;

	if (xhp->transd != NULL)
		return 0;

	if ((xhp->transd = flappy_dictionary_create()) == NULL)
		return flappy_error_oom();

	if ((array = flappy_array_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "packages", array)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	flappy_object_release(array);

	if ((array = flappy_array_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "missing_deps", array)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	flappy_object_release(array);

	if ((array = flappy_array_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "missing_shlibs", array)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	flappy_object_release(array);

	if ((array = flappy_array_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "conflicts", array)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	flappy_object_release(array);

	if ((dict = flappy_dictionary_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "obsolete_files", dict)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	flappy_object_release(dict);

	if ((dict = flappy_dictionary_create()) == NULL) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	if (!flappy_dictionary_set(xhp->transd, "remove_files", dict)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return flappy_error_oom();
	}
	flappy_object_release(dict);

	return 0;
}

int
flappy_transaction_prepare(struct flappy_handle *xhp)
{
	flappy_array_t pkgs, edges;
	flappy_dictionary_t tpkgd;
	flappy_trans_type_t ttype;
	unsigned int i, cnt;
	int rv = 0;
	int r;
	bool all_on_hold = true;

	if ((rv = flappy_transaction_init(xhp)) != 0)
		return rv;

	if (xhp->transd == NULL)
		return ENXIO;

	/*
	 * Collect dependencies for pkgs in transaction.
	 */
	if ((edges = flappy_array_create()) == NULL)
		return ENOMEM;

	flappy_dbg_printf("%s: processing deps\n", __func__);
	/*
	 * The edges are also appended after its dependencies have been
	 * collected; the edges at the original array are removed later.
	 */
	pkgs = flappy_dictionary_get(xhp->transd, "packages");
	assert(flappy_object_type(pkgs) == FLAPPY_TYPE_ARRAY);
	cnt = flappy_array_count(pkgs);
	for (i = 0; i < cnt; i++) {
		flappy_dictionary_t pkgd;
		flappy_string_t str;

		pkgd = flappy_array_get(pkgs, i);
		str = flappy_dictionary_get(pkgd, "pkgver");
		ttype = flappy_transaction_pkg_type(pkgd);

		if (ttype == FLAPPY_TRANS_REMOVE || ttype == FLAPPY_TRANS_HOLD)
			continue;

		assert(flappy_object_type(str) == FLAPPY_TYPE_STRING);

		if (!flappy_array_add(edges, str)) {
			flappy_object_release(edges);
			return ENOMEM;
		}
		if ((rv = flappy_transaction_pkg_deps(xhp, pkgs, pkgd)) != 0) {
			flappy_object_release(edges);
			return rv;
		}
		if (!flappy_array_add(pkgs, pkgd)) {
			flappy_object_release(edges);
			return ENOMEM;
		}
	}
	/* ... remove dup edges at head */
	for (i = 0; i < flappy_array_count(edges); i++) {
		const char *pkgver = NULL;
		flappy_array_get_cstring_nocopy(edges, i, &pkgver);
		flappy_remove_pkg_from_array_by_pkgver(pkgs, pkgver);
	}
	flappy_object_release(edges);

	/*
	 * Do not perform any checks if FLAPPY_FLAG_DOWNLOAD_ONLY
	 * is set. We just need to download the archives (dependencies).
	 */
	if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY)
		goto out;

	/*
	 * If all pkgs in transaction are on hold, no need to check
	 * for anything else.
	 */
	flappy_dbg_printf("%s: checking on hold pkgs\n", __func__);
	for (i = 0; i < cnt; i++) {
		tpkgd = flappy_array_get(pkgs, i);
		if (flappy_transaction_pkg_type(tpkgd) != FLAPPY_TRANS_HOLD) {
			all_on_hold = false;
			break;
		}
	}
	if (all_on_hold)
		goto out;

	/*
	 * Check for packages to be replaced.
	 */
	flappy_dbg_printf("%s: checking replaces\n", __func__);
	if (!flappy_transaction_check_replaces(xhp, pkgs)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	/*
	 * Check if there are missing revdeps.
	 */
	flappy_dbg_printf("%s: checking revdeps\n", __func__);
	if (!flappy_transaction_check_revdeps(xhp, pkgs)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if (flappy_dictionary_get(xhp->transd, "missing_deps")) {
		if (xhp->flags & FLAPPY_FLAG_FORCE_REMOVE_REVDEPS) {
			flappy_dbg_printf("[trans] continuing with broken reverse dependencies!");
		} else {
			return ENODEV;
		}
	}
	/*
	 * Check for package conflicts.
	 */
	flappy_dbg_printf("%s: checking conflicts\n", __func__);
	r = flappy_transaction_check_conflicts(xhp, pkgs);
	if (r < 0) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return -r;
	}
	if (flappy_dictionary_get(xhp->transd, "conflicts")) {
		return EAGAIN;
	}
	/*
	 * Check for unresolved shared libraries.
	 */
	flappy_dbg_printf("%s: checking shlibs\n", __func__);
	if (!flappy_transaction_check_shlibs(xhp, pkgs)) {
		flappy_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if (flappy_dictionary_get(xhp->transd, "missing_shlibs")) {
		if (xhp->flags & FLAPPY_FLAG_FORCE_REMOVE_REVDEPS) {
			flappy_dbg_printf("[trans] continuing with unresolved shared libraries!");
		} else {
			return ENOEXEC;
		}
	}
out:
	/*
	 * Add transaction stats for total download/installed size,
	 * number of packages to be installed, updated, configured
	 * and removed to the transaction dictionary.
	 */
	flappy_dbg_printf("%s: computing stats\n", __func__);
	if ((rv = compute_transaction_stats(xhp)) != 0) {
		return rv;
	}
	/*
	 * Make transaction dictionary immutable.
	 */
	flappy_dictionary_make_immutable(xhp->transd);

	return 0;
}

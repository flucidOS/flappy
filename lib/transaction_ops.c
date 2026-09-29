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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>

#include "flappy_api_impl.h"

/**
 * @file lib/transaction_ops.c
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
trans_find_pkg(struct flappy_handle *xhp, const char *pkg, bool force)
{
	char buf[FLAPPY_NAME_SIZE];
	flappy_dictionary_t pkg_pkgdb = NULL, pkg_repod = NULL, vpkg_pkgdb = NULL;
	flappy_object_t obj;
	flappy_array_t pkgs;
	pkg_state_t state = 0;
	flappy_trans_type_t ttype;
	const char *repoloc, *repopkgver, *instpkgver, *pkgname;
	bool autoinst = false;
	int rv = 0;

	assert(pkg != NULL);

	/*
	 * Find out if pkg is installed first.
	 */
	if (flappy_pkg_name(buf, sizeof(buf), pkg)) {
		pkg_pkgdb = flappy_pkgdb_get_pkg(xhp, buf);
		if (!pkg_pkgdb)
			vpkg_pkgdb = flappy_pkgdb_get_virtualpkg(xhp, buf);
	} else {
		pkg_pkgdb = flappy_pkgdb_get_pkg(xhp, pkg);
		if (!pkg_pkgdb)
			vpkg_pkgdb = flappy_pkgdb_get_virtualpkg(xhp, pkg);
	}

	if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
		pkg_pkgdb = NULL;
		ttype = FLAPPY_TRANS_DOWNLOAD;
	}

	if (vpkg_pkgdb) {
		// virtual package installed, if there is no real package in
		// the rpool, we are keeping the virtual package.
		pkg_repod = flappy_rpool_get_pkg(xhp, pkg);
		if (!pkg_repod) {
			pkg_pkgdb = vpkg_pkgdb;
			// if we are using the installed virtual package,
			// use the provider to query the repository pool.
			if (!flappy_dictionary_get_cstring_nocopy(
			        pkg_pkgdb, "pkgname", &pkg)) {
				flappy_error_printf("missing `pkgname` property\n");
				return EINVAL;
			}
		}
	}
	if (pkg_pkgdb) {
		// package already installed
		if (force) {
			ttype = FLAPPY_TRANS_REINSTALL;
		} else {
			ttype = FLAPPY_TRANS_UPDATE;
		}
		if (flappy_dictionary_get(pkg_pkgdb, "repolock")) {
			struct flappy_repo *repo;
			/* find update from repo */
			flappy_dictionary_get_cstring_nocopy(pkg_pkgdb, "repository", &repoloc);
			assert(repoloc);
			if ((repo = flappy_regget_repo(xhp, repoloc)) == NULL) {
				/* not found */
				return ENOENT;
			}
			pkg_repod = flappy_repo_get_pkg(repo, pkg);
		} else {
			/* find update from rpool */
			pkg_repod = flappy_rpool_get_pkg(xhp, pkg);
		}
	} else {
		ttype = FLAPPY_TRANS_INSTALL;
		pkg_repod = flappy_rpool_get_pkg(xhp, pkg);
		if (!pkg_repod)
			pkg_repod = flappy_rpool_get_virtualpkg(xhp, pkg);
	}

	if (!pkg_repod) {
		/* not found */
		return ENOENT;
	}

	flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver);

	if (ttype == FLAPPY_TRANS_UPDATE) {
		/*
		 * Compare installed version vs best pkg available in repos
		 * for pkg updates.
		 */
		flappy_dictionary_get_cstring_nocopy(pkg_pkgdb,
		    "pkgver", &instpkgver);
		if (flappy_cmpver(repopkgver, instpkgver) <= 0 &&
		    !flappy_pkg_reverts(pkg_repod, instpkgver)) {
			flappy_dictionary_get_cstring_nocopy(pkg_repod,
			    "repository", &repoloc);
			flappy_dbg_printf("[rpool] Skipping `%s' "
			    "(installed: %s) from repository `%s'\n",
			    repopkgver, instpkgver, repoloc);
			return EEXIST;
		}
	} else if (ttype == FLAPPY_TRANS_REINSTALL) {
		/*
		 * For reinstallation check if installed version is less than
		 * or equal to the pkg in repos, if true, continue with reinstallation;
		 * otherwise perform an update.
		 */
		flappy_dictionary_get_cstring_nocopy(pkg_pkgdb, "pkgver", &instpkgver);
		if (flappy_cmpver(repopkgver, instpkgver) == 1) {
			ttype = FLAPPY_TRANS_UPDATE;
		}
	}

	if (pkg_pkgdb) {
		/*
		 * If pkg is already installed, respect some properties.
		 */
		if ((obj = flappy_dictionary_get(pkg_pkgdb, "automatic-install")))
			flappy_dictionary_set(pkg_repod, "automatic-install", obj);
		if ((obj = flappy_dictionary_get(pkg_pkgdb, "hold")))
			flappy_dictionary_set(pkg_repod, "hold", obj);
		if ((obj = flappy_dictionary_get(pkg_pkgdb, "repolock")))
			flappy_dictionary_set(pkg_repod, "repolock", obj);
	}
	/*
	 * Prepare transaction dictionary.
	 */
	if ((rv = flappy_transaction_init(xhp)) != 0)
		return rv;

	pkgs = flappy_dictionary_get(xhp->transd, "packages");
	/*
	 * Find out if package being updated matches the one already
	 * in transaction, in that case ignore it.
	 */
	if (ttype == FLAPPY_TRANS_UPDATE) {
		if (flappy_find_pkg_in_array(pkgs, repopkgver, 0)) {
			flappy_dbg_printf("[update] `%s' already queued in "
			    "transaction.\n", repopkgver);
			return EEXIST;
		}
	}

	if (!flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname)) {
		flappy_error_printf("missing `pkgname` property\n");
		return EINVAL;
	}
	/*
	 * Set package state in dictionary with same state than the
	 * package currently uses, otherwise not-installed.
	 */
	if ((rv = flappy_pkg_state_installed(xhp, pkgname, &state)) != 0) {
		if (rv != ENOENT) {
			return rv;
		}
		/* Package not installed, don't error out */
		state = FLAPPY_PKG_STATE_NOT_INSTALLED;
	}
	if ((rv = flappy_set_pkg_state_dictionary(pkg_repod, state)) != 0) {
		return rv;
	}

	if (state == FLAPPY_PKG_STATE_NOT_INSTALLED)
		ttype = FLAPPY_TRANS_INSTALL;

	if (!force && flappy_dictionary_get(pkg_repod, "hold"))
		ttype = FLAPPY_TRANS_HOLD;

	/*
	 * Store pkgd from repo into the transaction.
	 */
	if (!flappy_transaction_pkg_type_set(pkg_repod, ttype)) {
		return EINVAL;
	}

	/*
	 * Set automatic-install to true if it was requested and this is a new install.
	 */
	if (ttype == FLAPPY_TRANS_INSTALL)
		autoinst = xhp->flags & FLAPPY_FLAG_INSTALL_AUTO;

	if (!flappy_transaction_store(xhp, pkgs, pkg_repod, autoinst)) {
		return EINVAL;
	}

	return 0;
}

/*
 * Returns 1 if there's an update, 0 if none or -1 on error.
 */
static int
flappy_autoupdate(struct flappy_handle *xhp)
{
	flappy_array_t rdeps;
	flappy_dictionary_t pkgd;
	const char *pkgver = NULL, *pkgname = NULL;
	int rv;

	/*
	 * Check if there's a new update for FLAPPY before starting
	 * another transaction.
	 */
	if (((pkgd = flappy_pkgdb_get_pkg(xhp, "flappy")) == NULL) &&
	    ((pkgd = flappy_pkgdb_get_virtualpkg(xhp, "flappy")) == NULL))
		return 0;

	if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		return EINVAL;
	}
	if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname)) {
		return EINVAL;
	}

	rv = trans_find_pkg(xhp, pkgname, false);

	flappy_dbg_printf("%s: trans_find_pkg flappy: %d\n", __func__, rv);

	if (rv == 0) {
		if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
			return 0;
		}
		/* a new flappy version is available, check its revdeps */
		rdeps = flappy_pkgdb_get_pkg_revdeps(xhp, "flappy");
		for (unsigned int i = 0; i < flappy_array_count(rdeps); i++)  {
			const char *curpkgver = NULL;
			char curpkgn[FLAPPY_NAME_SIZE] = {0};

			flappy_array_get_cstring_nocopy(rdeps, i, &curpkgver);
			flappy_dbg_printf("%s: processing revdep %s\n", __func__, curpkgver);

			if (!flappy_pkg_name(curpkgn, sizeof(curpkgn), curpkgver))
				flappy_unreachable();
			rv = trans_find_pkg(xhp, curpkgn, false);
			flappy_dbg_printf("%s: trans_find_pkg revdep %s: %d\n", __func__, curpkgver, rv);
			if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV)
				return -1;
		}
		/*
		 * Set FLAPPY_FLAG_FORCE_REMOVE_REVDEPS to ignore broken
		 * reverse dependencies in flappy_transaction_prepare().
		 *
		 * This won't skip revdeps of the flappy pkg, rather other
		 * packages in rootdir that could be broken indirectly.
		 *
		 * A sysup transaction after updating flappy should fix them
		 * again.
		 */
		xhp->flags |= FLAPPY_FLAG_FORCE_REMOVE_REVDEPS;
		return 1;
	} else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
		/* no update */
		return 0;
	} else {
		/* error */
		return -1;
	}

	return 0;
}

int
flappy_transaction_update_packages(struct flappy_handle *xhp)
{
	flappy_object_t obj;
	flappy_object_iterator_t iter;
	flappy_dictionary_t pkgd;
	bool newpkg_found = false;
	int rv = 0;

	rv = flappy_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* flappy needs to be updated, don't allow any other update */
		return EBUSY;
	case -1:
		/* error */
		return EINVAL;
	default:
		break;
	}

	iter = flappy_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		const char *pkgver = NULL;
		char pkgname[FLAPPY_NAME_SIZE] = {0};

		pkgd = flappy_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
			continue;
		}
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		rv = trans_find_pkg(xhp, pkgname, false);
		flappy_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv == 0) {
			newpkg_found = true;
		} else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
			/*
			 * missing pkg or installed version is greater than or
			 * equal than pkg in repositories.
			 */
			rv = 0;
		}
	}
	flappy_object_iterator_release(iter);

	return newpkg_found ? rv : EEXIST;
}

int
flappy_transaction_update_pkg(struct flappy_handle *xhp, const char *pkg, bool force)
{
	flappy_array_t rdeps;
	int rv;

	rv = flappy_autoupdate(xhp);
	flappy_dbg_printf("%s: flappy_autoupdate %d\n", __func__, rv);
	switch (rv) {
	case 1:
		/* flappy needs to be updated, only allow flappy to be updated */
		if (strcmp(pkg, "flappy"))
			return EBUSY;
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	/* update its reverse dependencies */
	rdeps = flappy_pkgdb_get_pkg_revdeps(xhp, pkg);
	if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
		rdeps = NULL;
	}
	for (unsigned int i = 0; i < flappy_array_count(rdeps); i++)  {
		char pkgname[FLAPPY_NAME_SIZE];
		const char *pkgver = NULL;

		if (!flappy_array_get_cstring_nocopy(rdeps, i, &pkgver))
			flappy_unreachable();
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver))
			flappy_unreachable();

		rv = trans_find_pkg(xhp, pkgname, false);
		flappy_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV) {
			return rv;
		}
	}
	/* add pkg repod */
	rv = trans_find_pkg(xhp, pkg, force);
	flappy_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
	return rv;
}

int
flappy_transaction_install_pkg(struct flappy_handle *xhp, const char *pkg, bool force)
{
	flappy_array_t rdeps;
	int rv;

	rv = flappy_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* flappy needs to be updated, only allow flappy to be updated */
		if (strcmp(pkg, "flappy"))
			return EBUSY;
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	/* update its reverse dependencies */
	rdeps = flappy_pkgdb_get_pkg_revdeps(xhp, pkg);
	if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
		rdeps = NULL;
	}
	for (unsigned int i = 0; i < flappy_array_count(rdeps); i++)  {
		char pkgname[FLAPPY_NAME_SIZE];
		const char *pkgver = NULL;

		if (!flappy_array_get_cstring_nocopy(rdeps, i, &pkgver))
			flappy_unreachable();
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver))
			flappy_unreachable();

		rv = trans_find_pkg(xhp, pkgname, false);
		flappy_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV) {
			return rv;
		}
	}
	rv = trans_find_pkg(xhp, pkg, force);
	flappy_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
	return rv;
}

int
flappy_transaction_remove_pkg(struct flappy_handle *xhp,
			    const char *pkgname,
			    bool recursive)
{
	flappy_dictionary_t pkgd;
	flappy_array_t pkgs, orphans, orphans_pkg;
	flappy_object_t obj;
	int rv = 0;

	assert(xhp);
	assert(pkgname);

	if ((pkgd = flappy_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
		/* pkg not installed */
		return ENOENT;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((rv = flappy_transaction_init(xhp)) != 0)
		return rv;

	pkgs = flappy_dictionary_get(xhp->transd, "packages");

	if (!recursive)
		goto rmpkg;
	/*
	 * If recursive is set, find out which packages would be orphans
	 * if the supplied package were already removed.
	 */
	if ((orphans_pkg = flappy_array_create()) == NULL)
		return ENOMEM;

	flappy_array_set_cstring_nocopy(orphans_pkg, 0, pkgname);
	orphans = flappy_find_pkg_orphans(xhp, orphans_pkg);
	flappy_object_release(orphans_pkg);
	if (flappy_object_type(orphans) != FLAPPY_TYPE_ARRAY)
		return EINVAL;

	for (unsigned int i = 0; i < flappy_array_count(orphans); i++) {
		obj = flappy_array_get(orphans, i);
		flappy_transaction_pkg_type_set(obj, FLAPPY_TRANS_REMOVE);
		if (!flappy_transaction_store(xhp, pkgs, obj, false)) {
			return EINVAL;
		}
	}
	flappy_object_release(orphans);
	return rv;

rmpkg:
	/*
	 * Add pkg dictionary into the transaction pkgs queue.
	 */
	flappy_transaction_pkg_type_set(pkgd, FLAPPY_TRANS_REMOVE);
	if (!flappy_transaction_store(xhp, pkgs, pkgd, false)) {
		return EINVAL;
	}
	return rv;
}

int
flappy_transaction_autoremove_pkgs(struct flappy_handle *xhp)
{
	flappy_array_t orphans, pkgs;
	flappy_object_t obj;
	int rv = 0;

	orphans = flappy_find_pkg_orphans(xhp, NULL);
	if (flappy_array_count(orphans) == 0) {
		/* no orphans? we are done */
		goto out;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((rv = flappy_transaction_init(xhp)) != 0)
		goto out;

	pkgs = flappy_dictionary_get(xhp->transd, "packages");
	/*
	 * Add pkg orphan dictionary into the transaction pkgs queue.
	 */
	for (unsigned int i = 0; i < flappy_array_count(orphans); i++) {
		obj = flappy_array_get(orphans, i);
		flappy_transaction_pkg_type_set(obj, FLAPPY_TRANS_REMOVE);
		if (!flappy_transaction_store(xhp, pkgs, obj, false)) {
			rv = EINVAL;
			goto out;
		}
	}
out:
	if (orphans)
		flappy_object_release(orphans);

	return rv;
}

flappy_trans_type_t
flappy_transaction_pkg_type(flappy_dictionary_t pkg_repod)
{
	uint8_t r;

	if (flappy_object_type(pkg_repod) != FLAPPY_TYPE_DICTIONARY)
		return 0;

	if (!flappy_dictionary_get_uint8(pkg_repod, "transaction", &r))
		return 0;

	return r;
}

bool
flappy_transaction_pkg_type_set(flappy_dictionary_t pkg_repod, flappy_trans_type_t ttype)
{
	uint8_t r;

	if (flappy_object_type(pkg_repod) != FLAPPY_TYPE_DICTIONARY)
		return false;

	switch (ttype) {
	case FLAPPY_TRANS_INSTALL:
	case FLAPPY_TRANS_UPDATE:
	case FLAPPY_TRANS_CONFIGURE:
	case FLAPPY_TRANS_REMOVE:
	case FLAPPY_TRANS_REINSTALL:
	case FLAPPY_TRANS_HOLD:
	case FLAPPY_TRANS_DOWNLOAD:
		break;
	default:
		return false;
	}
	r = ttype;
	if (!flappy_dictionary_set_uint8(pkg_repod, "transaction", r))
		return false;

	return true;
}

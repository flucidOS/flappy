/*-
 * Copyright (c) 2008-2020 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "flappy_api_impl.h"

static int
add_missing_reqdep(struct flappy_handle *xhp, const char *reqpkg)
{
	flappy_array_t mdeps;
	flappy_object_iterator_t iter = NULL;
	flappy_object_t obj;
	unsigned int idx = 0;
	bool add_pkgdep, pkgfound, update_pkgdep;
	int rv = 0;

	assert(reqpkg != NULL);

	add_pkgdep = update_pkgdep = pkgfound = false;
	mdeps = flappy_dictionary_get(xhp->transd, "missing_deps");

	iter = flappy_array_iterator(mdeps);
	if (iter == NULL)
		goto out;

	while ((obj = flappy_object_iterator_next(iter)) != NULL) {
		const char *curdep, *curver, *pkgver;
		char curpkgnamedep[FLAPPY_NAME_SIZE];
		char pkgnamedep[FLAPPY_NAME_SIZE];

		assert(flappy_object_type(obj) == FLAPPY_TYPE_STRING);
		curdep = flappy_string_cstring_nocopy(obj);
		curver = flappy_pkgpattern_version(curdep);
		pkgver = flappy_pkgpattern_version(reqpkg);
		if (curver == NULL || pkgver == NULL)
			goto out;
		if (!flappy_pkgpattern_name(curpkgnamedep, FLAPPY_NAME_SIZE, curdep)) {
			goto out;
		}
		if (!flappy_pkgpattern_name(pkgnamedep, FLAPPY_NAME_SIZE, reqpkg)) {
			goto out;
		}
		if (strcmp(pkgnamedep, curpkgnamedep) == 0) {
			pkgfound = true;
			if (strcmp(curver, pkgver) == 0) {
				rv = EEXIST;
				goto out;
			}
			/*
			 * if new dependency version is greater than current
			 * one, store it.
			 */
			flappy_dbg_printf("Missing pkgdep name matched, curver: %s newver: %s\n", curver, pkgver);
			if (flappy_cmpver(curver, pkgver) <= 0) {
				add_pkgdep = false;
				rv = EEXIST;
				goto out;
			}
			update_pkgdep = true;
		}
		if (pkgfound)
			break;

		idx++;
	}
	add_pkgdep = true;
out:
	if (iter)
		flappy_object_iterator_release(iter);
	if (update_pkgdep)
		flappy_array_remove(mdeps, idx);
	if (add_pkgdep) {
		char *str;

		str = flappy_xasprintf("MISSING: %s", reqpkg);
		flappy_array_add_cstring(mdeps, str);
		free(str);
	}

	return rv;
}

#define MAX_DEPTH	512

static int
repo_deps(struct flappy_handle *xhp,
	  flappy_array_t pkgs,		/* array of pkgs */
	  flappy_array_t queued,		/* queued packages */
	  flappy_dictionary_t pkg_repod,	/* pkg repo dictionary */
	  unsigned short *depth)	/* max recursion depth */
{
	flappy_array_t pkg_rdeps = NULL, pkg_provides = NULL;
	flappy_dictionary_t curpkgd = NULL, repopkgd = NULL;
	flappy_trans_type_t ttype;
	pkg_state_t state;
	flappy_object_t obj;
	flappy_object_iterator_t iter;
	const char *curpkg = NULL, *reqpkg = NULL, *pkgver_q = NULL;
	char pkgname[FLAPPY_NAME_SIZE], reqpkgname[FLAPPY_NAME_SIZE];
	int rv = 0;

	assert(xhp);
	assert(pkgs);
	assert(pkg_repod);

	if (*depth >= MAX_DEPTH)
		return ELOOP;

	flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &curpkg);
	pkg_provides = flappy_dictionary_get(pkg_repod, "provides");
	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	pkg_rdeps = flappy_dictionary_get(pkg_repod, "run_depends");
	if (flappy_array_count(pkg_rdeps) == 0)
		goto out;

	iter = flappy_array_iterator(pkg_rdeps);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		bool error = false, foundvpkg = false;
		bool autoinst = true;

		ttype = FLAPPY_TRANS_UNKNOWN;
		reqpkg = flappy_string_cstring_nocopy(obj);

		if (xhp->flags & FLAPPY_FLAG_DEBUG) {
			flappy_dbg_printf("%s", "");
			for (unsigned short x = 0; x < *depth; x++) {
				flappy_dbg_printf_append(" ");
			}
			flappy_dbg_printf_append("%s: requires dependency '%s': ", curpkg ? curpkg : " ", reqpkg);
		}
		if ((!flappy_pkgpattern_name(pkgname, sizeof(pkgname), reqpkg)) &&
		    (!flappy_pkg_name(pkgname, sizeof(pkgname), reqpkg))) {
			flappy_dbg_printf("%s: can't guess pkgname for dependency: %s\n", curpkg, reqpkg);
			flappy_set_cb_state(xhp, FLAPPY_STATE_INVALID_DEP, ENXIO, NULL,
			    "%s: can't guess pkgname for dependency '%s'", curpkg, reqpkg);
			rv = ENXIO;
			break;
		}
		/*
		 * Pass 0: check if required dependency is ignored.
		 */
		if (flappy_pkg_is_ignored(xhp, pkgname)) {
			flappy_dbg_printf_append("%s ignored.\n", pkgname);
			continue;
		}
		/*
		 * Pass 1: check if required dependency is provided as virtual
		 * package via "provides", if true ignore dependency.
		 */
		if (pkg_provides && flappy_match_virtual_pkg_in_array(pkg_provides, reqpkg)) {
			flappy_dbg_printf_append("%s is a vpkg provided by %s, ignored.\n", pkgname, curpkg);
			continue;
		}
		/*
		 * Pass 2: check if required dependency is currently queued or
		 * has been already added in the transaction dictionary.
		 */
		if ((curpkgd = flappy_find_pkg_in_array(queued, reqpkg, 0)) ||
		    (curpkgd = flappy_find_virtualpkg_in_array(xhp, queued, reqpkg, 0))) {
			flappy_trans_type_t ttype_q = flappy_transaction_pkg_type(curpkgd);
			flappy_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &pkgver_q);
			flappy_dbg_printf_append(" (%s queued %d)\n", pkgver_q, ttype_q);
			continue;
		}
		/*
		 * Pass 3: check if required dependency has been already added
		 * in the transaction dictionary.
		 */
		if ((curpkgd = flappy_find_pkg_in_array(pkgs, reqpkg, 0)) ||
		    (curpkgd = flappy_find_virtualpkg_in_array(xhp, pkgs, reqpkg, 0))) {
			flappy_trans_type_t ttype_q = flappy_transaction_pkg_type(curpkgd);
			flappy_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &pkgver_q);
			if (ttype_q != FLAPPY_TRANS_REMOVE && ttype_q != FLAPPY_TRANS_HOLD) {
				flappy_dbg_printf_append(" (%s queued %d)\n", pkgver_q, ttype_q);
				continue;
			}
		}
		/*
		 * Pass 4: check if required dependency is already installed
		 * and its version is fully matched.
		 */
		if ((curpkgd = flappy_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
			if ((curpkgd = flappy_pkgdb_get_virtualpkg(xhp, pkgname))) {
				foundvpkg = true;
			}
		}
		if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
			/*
			 * if FLAPPY_FLAG_DOWNLOAD_ONLY always assume
			 * all deps are not installed. This way one can download
			 * the whole set of binary packages to perform an
			 * off-line installation later on.
			 */
			curpkgd = NULL;
		}

		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				flappy_dbg_printf("failed to find installed pkg for `%s': %s\n", reqpkg, strerror(rv));
				break;
			}
			/* Required dependency not installed */
			flappy_dbg_printf_append("not installed.\n");
			ttype = FLAPPY_TRANS_INSTALL;
			state = FLAPPY_PKG_STATE_NOT_INSTALLED;
		} else {
			/*
			 * Required dependency is installed, check if its version can
			 * satisfy the requirements.
			 */
			flappy_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &pkgver_q);

			/* Check its state */
			if ((rv = flappy_pkg_state_dictionary(curpkgd, &state)) != 0) {
				break;
			}

			if (foundvpkg && flappy_match_virtual_pkg_in_dict(curpkgd, reqpkg)) {
				/*
				 * Check if required dependency is a virtual package and is satisfied
				 * by an installed package.
				 */
				flappy_dbg_printf_append("[virtual] satisfied by `%s'.\n", pkgver_q);
				continue;
			}
			rv = flappy_pkgpattern_match(pkgver_q, reqpkg);
			if (rv == 0) {
				char curpkgname[FLAPPY_NAME_SIZE];
				/*
				 * The version requirement is not satisfied.
				 */
				if (!flappy_pkg_name(curpkgname, sizeof(curpkgname), pkgver_q))
					flappy_unreachable();

				if (strcmp(pkgname, curpkgname)) {
					flappy_dbg_printf_append("not installed `%s (vpkg)'", pkgver_q);
					if (flappy_dictionary_get(curpkgd, "hold")) {
						ttype = FLAPPY_TRANS_HOLD;
						flappy_dbg_printf_append(" on hold state! ignoring package.\n");
						rv = ENODEV;
					} else {
						flappy_dbg_printf_append("\n");
						ttype = FLAPPY_TRANS_INSTALL;
					}
				} else {
					flappy_dbg_printf_append("installed `%s', must be updated", pkgver_q);
					if (flappy_dictionary_get(curpkgd, "hold")) {
						flappy_dbg_printf_append(" on hold state! ignoring package.\n");
						ttype = FLAPPY_TRANS_HOLD;
						rv = ENODEV;
					} else {
						flappy_dbg_printf_append("\n");
						ttype = FLAPPY_TRANS_UPDATE;
					}
				}
				/*
				 * Not satisfied and package on hold.
				 */
				if (rv == ENODEV) {
					rv = add_missing_reqdep(xhp, reqpkg);
					if (rv != 0 && rv != EEXIST) {
						flappy_dbg_printf("`%s': add_missing_reqdep failed\n", reqpkg);
						break;
					} else if (rv == EEXIST) {
						flappy_dbg_printf("`%s' missing dep already added.\n", reqpkg);
						rv = 0;
						continue;
					} else {
						flappy_dbg_printf("`%s' added into the missing deps array.\n", reqpkg);
						continue;
					}
				}
			} else if (rv == 1) {
				/*
				 * The version requirement is satisfied.
				 */
				rv = 0;
				if (state == FLAPPY_PKG_STATE_UNPACKED) {
					/*
					 * Package matches the dependency pattern but was only unpacked,
					 * configure pkg.
					 */
					flappy_dbg_printf_append("installed `%s', must be configured.\n", pkgver_q);
					ttype = FLAPPY_TRANS_CONFIGURE;
				} else if (state == FLAPPY_PKG_STATE_INSTALLED) {
					/*
					 * Package matches the dependency pattern and is fully installed,
					 * skip to next one.
					 */
					flappy_dbg_printf_append("installed `%s'.\n", pkgver_q);
					continue;
				}
			} else {
				/* error matching pkgpattern */
				flappy_dbg_printf("failed to match pattern %s with %s\n", reqpkg, pkgver_q);
				break;
			}
		}
		if (ttype == FLAPPY_TRANS_UPDATE || ttype == FLAPPY_TRANS_CONFIGURE) {
			/*
			 * If the package is already installed preserve the installation mode,
			 * which is not automatic if automatic-install is not set.
			 */
			bool pkgd_auto = false;
			flappy_dictionary_get_bool(curpkgd, "automatic-install", &pkgd_auto);
			autoinst = pkgd_auto;
		}
		if (ttype == FLAPPY_TRANS_CONFIGURE) {
			if (!flappy_transaction_pkg_type_set(curpkgd, ttype)) {
				rv = EINVAL;
				flappy_dbg_printf("flappy_transaction_pkg_type_set failed for `%s': %s\n", reqpkg, strerror(rv));
				break;
			}
			if (!flappy_transaction_store(xhp, pkgs, curpkgd, autoinst)) {
				rv = EINVAL;
				flappy_dbg_printf("flappy_transaction_store failed for `%s': %s\n", reqpkg, strerror(rv));
				break;
			}
			continue;
		}
		/*
		 * Pass 5: find required dependency in repository pool.
		 * If dependency does not match add pkg into the missing
		 * deps array and pass to next one.
		 */
		if (flappy_dictionary_get(curpkgd, "repolock")) {
			const char *repourl = NULL;
			struct flappy_repo *repo = NULL;
			flappy_dbg_printf("`%s' is repolocked, looking at single repository.\n", reqpkg);
			flappy_dictionary_get_cstring_nocopy(curpkgd, "repository", &repourl);
			if (repourl && (repo = flappy_regget_repo(xhp, repourl))) {
				repopkgd = flappy_repo_get_pkg(repo, reqpkg);
			} else {
				repopkgd = NULL;
			}
		} else {
			repopkgd = flappy_rpool_get_pkg(xhp, reqpkg);
			if (!repopkgd) {
				repopkgd = flappy_rpool_get_virtualpkg(xhp, reqpkg);
			}
		}
		if (repopkgd == NULL) {
			/* pkg not found, there was some error */
			if (errno && errno != ENOENT) {
				flappy_dbg_printf("failed to find pkg for `%s' in rpool: %s\n", reqpkg, strerror(errno));
				rv = errno;
				break;
			}
			rv = add_missing_reqdep(xhp, reqpkg);
			if (rv != 0 && rv != EEXIST) {
				flappy_dbg_printf("`%s': add_missing_reqdep failed\n", reqpkg);
				break;
			} else if (rv == EEXIST) {
				flappy_dbg_printf("`%s' missing dep already added.\n", reqpkg);
				rv = 0;
				continue;
			} else {
				flappy_dbg_printf("`%s' added into the missing deps array.\n", reqpkg);
				continue;
			}
		}


		flappy_dictionary_get_cstring_nocopy(repopkgd, "pkgver", &pkgver_q);
		if (!flappy_pkg_name(reqpkgname, sizeof(reqpkgname), pkgver_q)) {
			rv = EINVAL;
			break;
		}
		/*
		 * Check dependency validity.
		 */
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), curpkg)) {
			rv = EINVAL;
			break;
		}
		if (strcmp(pkgname, reqpkgname) == 0) {
			flappy_dbg_printf_append("[ignoring wrong dependency %s (depends on itself)]\n", reqpkg);
			flappy_remove_string_from_array(pkg_rdeps, reqpkg);
			continue;
		}
		/*
		 * Installed package must be updated, check if dependency is
		 * satisfied.
		 */
		if (ttype == FLAPPY_TRANS_UPDATE) {
			switch (flappy_pkgpattern_match(pkgver_q, reqpkg)) {
				case 0: /* nomatch */
					break;
				case 1: /* match */
					if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver_q))
						flappy_unreachable();
					/*
					 * If there's an update in transaction,
					 * it's assumed version is greater.
					 * So dependency pattern matching didn't
					 * succeed... return ENODEV.
					 */
					if (flappy_find_pkg_in_array(pkgs, pkgname, FLAPPY_TRANS_UPDATE)) {
						error = true;
						rv = ENODEV;
					}
					break;
				default:
					error = true;
					rv = EINVAL;
					break;
			}
			if (error)
				break;
		}

		if (!flappy_array_add(queued, repopkgd))
			return -flappy_error_oom();

		pkg_rdeps = flappy_dictionary_get(repopkgd, "run_depends");
		if (flappy_array_count(pkg_rdeps)) {
			/*
			 * Process rundeps for current pkg found in rpool.
			 */
			if (xhp->flags & FLAPPY_FLAG_DEBUG) {
				flappy_dbg_printf("%s", "");
				for (unsigned short x = 0; x < *depth; x++) {
					flappy_dbg_printf_append(" ");
				}
				flappy_dbg_printf_append("%s: finding dependencies:\n", pkgver_q);
			}
			(*depth)++;
			rv = repo_deps(xhp, pkgs, queued, repopkgd, depth);
			if (rv != 0) {
				flappy_dbg_printf("Error checking %s for rundeps: %s\n", reqpkg, strerror(rv));
				break;
			}
		}
		if (xhp->flags & FLAPPY_FLAG_DOWNLOAD_ONLY) {
			ttype = FLAPPY_TRANS_DOWNLOAD;
		} else if (flappy_dictionary_get(curpkgd, "hold")) {
			ttype = FLAPPY_TRANS_HOLD;
		}

		flappy_array_remove(queued, flappy_array_count(queued) - 1);

		/*
		 * All deps were processed, store pkg in transaction.
		 */
		if (!flappy_transaction_pkg_type_set(repopkgd, ttype)) {
			rv = EINVAL;
			flappy_dbg_printf("flappy_transaction_pkg_type_set failed for `%s': %s\n", reqpkg, strerror(rv));
			break;
		}
		if (!flappy_transaction_store(xhp, pkgs, repopkgd, autoinst)) {
			rv = EINVAL;
			flappy_dbg_printf("flappy_transaction_store failed for `%s': %s\n", reqpkg, strerror(rv));
			break;
		}
	}
	flappy_object_iterator_release(iter);
out:
	(*depth)--;

	return rv;
}

int HIDDEN
flappy_transaction_pkg_deps(struct flappy_handle *xhp,
			  flappy_array_t pkgs,
			  flappy_dictionary_t pkg_repod)
{
	const char *pkgver;
	unsigned short depth = 0;
	flappy_array_t queued;
	int rv;

	assert(xhp);
	assert(pkgs);
	assert(pkg_repod);

	queued = flappy_array_create();
	if (!queued)
		return -flappy_error_oom();

	if (!flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver))
		return EINVAL;

	flappy_dbg_printf("Finding required dependencies for '%s':\n", pkgver);

	/*
	 * This will find direct and indirect deps, if any of them is not
	 * there it will be added into the missing_deps array.
	 */
	rv = repo_deps(xhp, pkgs, queued, pkg_repod, &depth);
	flappy_object_release(queued);
	return rv;
}

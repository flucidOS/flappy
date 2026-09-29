/*-
 * Copyright (c) 2013-2020 Juan Romero Pardines.
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
#include <stdbool.h>
#include <errno.h>

#include "flappy.h"
#include "flappy_api_impl.h"

/*
 * Verify reverse dependencies for packages in transaction.
 * This will catch cases where a package update would break its reverse dependencies:
 *
 * 	- foo-1.0 is being updated to 2.0.
 * 	- baz-1.1 depends on foo<2.0.
 * 	- foo is updated to 2.0, hence baz-1.1 is currently broken.
 *
 * Abort transaction if such case is found.
 */
static bool
check_virtual_pkgs(flappy_array_t mdeps,
		   flappy_dictionary_t trans_pkgd,
		   flappy_dictionary_t rev_pkgd)
{
	flappy_array_t rundeps;
	flappy_array_t provides;
	const char *pkgver, *vpkgver, *revpkgver, *pkgpattern;
	char pkgname[FLAPPY_NAME_SIZE], vpkgname[FLAPPY_NAME_SIZE];
	char *str = NULL;
	bool matched = false;

	pkgver = vpkgver = revpkgver = pkgpattern = NULL;
	provides = flappy_dictionary_get(trans_pkgd, "provides");

	for (unsigned int i = 0; i < flappy_array_count(provides); i++) {
		flappy_dictionary_get_cstring_nocopy(trans_pkgd, "pkgver", &pkgver);
		flappy_dictionary_get_cstring_nocopy(rev_pkgd, "pkgver", &revpkgver);
		flappy_array_get_cstring_nocopy(provides, i, &vpkgver);

		if (!flappy_pkg_name(vpkgname, sizeof(vpkgname), vpkgver)) {
			break;
		}

		rundeps = flappy_dictionary_get(rev_pkgd, "run_depends");
		for (unsigned int x = 0; x < flappy_array_count(rundeps); x++) {
			flappy_array_get_cstring_nocopy(rundeps, x, &pkgpattern);

			if ((!flappy_pkgpattern_name(pkgname, sizeof(pkgname), pkgpattern)) &&
			    (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgpattern)))
				continue;

			if (strcmp(vpkgname, pkgname)) {
				continue;
			}
			if (!strcmp(vpkgver, pkgpattern) ||
			    flappy_pkgpattern_match(vpkgver, pkgpattern)) {
				continue;
			}

			str = flappy_xasprintf("%s broken, needs '%s' virtual pkg (got `%s')",
			    revpkgver, pkgpattern, vpkgver);
			flappy_array_add_cstring(mdeps, str);
			free(str);
			matched = true;
		}
	}
	return matched;
}

static void
broken_pkg(flappy_array_t mdeps, const char *dep, const char *pkg)
{
	char *str;

	str = flappy_xasprintf("%s in transaction breaks installed pkg `%s'", pkg, dep);
	flappy_array_add_cstring(mdeps, str);
	free(str);
}

bool HIDDEN
flappy_transaction_check_revdeps(struct flappy_handle *xhp, flappy_array_t pkgs)
{
	flappy_array_t mdeps;
	bool error = false;

	mdeps = flappy_dictionary_get(xhp->transd, "missing_deps");

	for (unsigned int i = 0; i < flappy_array_count(pkgs); i++) {
		flappy_array_t pkgrdeps, rundeps;
		flappy_dictionary_t revpkgd;
		flappy_object_t obj;
		flappy_trans_type_t ttype;
		const char *pkgver = NULL, *revpkgver = NULL;
		char pkgname[FLAPPY_NAME_SIZE] = {0};

		obj = flappy_array_get(pkgs, i);
		/*
		 * If pkg is on hold, pass to the next one.
		 */
		ttype = flappy_transaction_pkg_type(obj);
		if (ttype == FLAPPY_TRANS_HOLD) {
			continue;
		}
		if (!flappy_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver)) {
			error = true;
			goto out;
		}
		if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			error = true;
			goto out;
		}
		/*
		 * if pkg in transaction is not installed,
		 * pass to next one.
		 */
		if (ttype == FLAPPY_TRANS_INSTALL)
			continue;
		/*
		 * If pkg is installed but does not have revdeps,
		 * pass to next one.
		 */
		pkgrdeps = flappy_pkgdb_get_pkg_revdeps(xhp, pkgname);
		if (!flappy_array_count(pkgrdeps)) {
			continue;
		}
		/*
		 * If pkg is ignored, pass to the next one.
		 */
		if (flappy_pkg_is_ignored(xhp, pkgver)) {
			continue;
		}

		/*
		 * Time to validate revdeps for current pkg.
		 */
		for (unsigned int x = 0; x < flappy_array_count(pkgrdeps); x++) {
			const char *curpkgver = NULL;
			char curdepname[FLAPPY_NAME_SIZE] = {0};
			char curpkgname[FLAPPY_NAME_SIZE] = {0};
			bool found = false;

			if (!flappy_array_get_cstring_nocopy(pkgrdeps, x, &curpkgver)) {
				error = true;
				goto out;
			}

			if (!flappy_pkg_name(pkgname, sizeof(pkgname), curpkgver)) {
				error = true;
				goto out;
			}

			if ((revpkgd = flappy_find_pkg_in_array(pkgs, pkgname, 0))) {
				if (flappy_transaction_pkg_type(revpkgd) == FLAPPY_TRANS_REMOVE)
					continue;
			}
			if (revpkgd == NULL)
				revpkgd = flappy_pkgdb_get_pkg(xhp, curpkgver);


			flappy_dictionary_get_cstring_nocopy(revpkgd, "pkgver", &revpkgver);
			/*
			 * If target pkg is being removed, all its revdeps
			 * will be broken unless those revdeps are also in
			 * the transaction.
			 */
			if (ttype == FLAPPY_TRANS_REMOVE) {
				if (flappy_dictionary_get(obj, "replaced")) {
					continue;
				}
				if (flappy_find_pkg_in_array(pkgs, pkgname, FLAPPY_TRANS_REMOVE)) {
					continue;
				}
				broken_pkg(mdeps, curpkgver, pkgver);
				continue;
			}
			/*
			 * First try to match any supported virtual package.
			 */
			if (check_virtual_pkgs(mdeps, obj, revpkgd)) {
				continue;
			}
			/*
			 * Try to match real dependencies.
			 */
			rundeps = flappy_dictionary_get(revpkgd, "run_depends");
			/*
			 * Find out what dependency is it.
			 */
			if (!flappy_pkg_name(curpkgname, sizeof(curpkgname), pkgver)) {
				return false;
			}

			for (unsigned int j = 0; j < flappy_array_count(rundeps); j++) {
				const char *curdep = NULL;

				flappy_array_get_cstring_nocopy(rundeps, j, &curdep);
				if ((!flappy_pkgpattern_name(curdepname, sizeof(curdepname), curdep)) &&
				    (!flappy_pkg_name(curdepname, sizeof(curdepname), curdep))) {
					return false;
				}
				if (strcmp(curdepname, curpkgname) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				continue;
			}
			if (flappy_match_pkgdep_in_array(rundeps, pkgver)) {
				continue;
			}
			/*
			 * Installed package conflicts with package
			 * in transaction being updated, check
			 * if a new version of this conflicting package
			 * is in the transaction.
			 */
			if (flappy_find_pkg_in_array(pkgs, pkgname, FLAPPY_TRANS_UPDATE)) {
				continue;
			}
			broken_pkg(mdeps, curpkgver, pkgver);
		}
	}
out:
	if (!error) {
		mdeps = flappy_dictionary_get(xhp->transd, "missing_deps");
		if (flappy_array_count(mdeps) == 0)
			flappy_dictionary_remove(xhp->transd, "missing_deps");
	}
	return error ? false : true;
}

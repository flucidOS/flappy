/*-
 * Copyright (c) 2011-2020 Juan Romero Pardines.
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
#include <unistd.h>
#include <libgen.h>

#include "flappy_api_impl.h"

/*
 * Processes the array of pkg dictionaries in "pkgs" to
 * find matching package replacements via "replaces" pkg obj.
 *
 * This array contains the unordered list of packages in
 * the transaction dictionary.
 */
bool HIDDEN
flappy_transaction_check_replaces(struct flappy_handle *xhp, flappy_array_t pkgs)
{
	assert(xhp);
	assert(pkgs);

	for (unsigned int i = 0; i < flappy_array_count(pkgs); i++) {
		flappy_array_t replaces;
		flappy_object_t obj;
		flappy_object_iterator_t iter;
		flappy_dictionary_t instd, reppkgd;
		const char *pkgver = NULL;
		char pkgname[FLAPPY_NAME_SIZE] = {0};

		obj = flappy_array_get(pkgs, i);
		replaces = flappy_dictionary_get(obj, "replaces");
		if (replaces == NULL || flappy_array_count(replaces) == 0)
			continue;

		if (!flappy_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver)) {
			return false;
		}
		if (!flappy_pkg_name(pkgname, FLAPPY_NAME_SIZE, pkgver)) {
			return false;
		}

		iter = flappy_array_iterator(replaces);
		assert(iter);

		for (unsigned int j = 0; j < flappy_array_count(replaces); j++) {
			const char *curpkgver = NULL, *pattern = NULL;
			char curpkgname[FLAPPY_NAME_SIZE] = {0};
			bool instd_auto = false, hold = false;
			flappy_trans_type_t ttype;

			if(!flappy_array_get_cstring_nocopy(replaces, j, &pattern))
				abort();

			/*
			 * Find the installed package that matches the pattern
			 * to be replaced. Also check if the package would be
			 * installed in the transaction.
			 */
			if (((instd = flappy_pkgdb_get_pkg(xhp, pattern)) == NULL) &&
			    ((instd = flappy_pkgdb_get_virtualpkg(xhp, pattern)) == NULL) &&
			    ((instd = flappy_find_pkg_in_array(pkgs, pattern, FLAPPY_TRANS_INSTALL)) == NULL))
				continue;

			if (!flappy_dictionary_get_cstring_nocopy(instd, "pkgver", &curpkgver)) {
				flappy_object_iterator_release(iter);
				return false;
			}
			/* ignore pkgs on hold mode */
			if (flappy_dictionary_get_bool(instd, "hold", &hold) && hold)
				continue;

			if (!flappy_pkg_name(curpkgname, FLAPPY_NAME_SIZE, curpkgver)) {
				flappy_object_iterator_release(iter);
				return false;
			}
			/*
			 * Check that we are not replacing the same package,
			 * due to virtual packages.
			 */
			if (strcmp(pkgname, curpkgname) == 0) {
				continue;
			}
			/*
			 * Make sure to not add duplicates.
			 */
			flappy_dictionary_get_bool(instd, "automatic-install", &instd_auto);
			reppkgd = flappy_find_pkg_in_array(pkgs, curpkgname, 0);
			if (reppkgd) {
				ttype = flappy_transaction_pkg_type(reppkgd);
				if (ttype == FLAPPY_TRANS_REMOVE || ttype == FLAPPY_TRANS_HOLD)
					continue;
				if (!flappy_dictionary_get_cstring_nocopy(reppkgd,
				    "pkgver", &curpkgver)) {
					flappy_object_iterator_release(iter);
					return false;
				}
				if (!flappy_match_virtual_pkg_in_dict(reppkgd, pattern) &&
				    !flappy_pkgpattern_match(curpkgver, pattern))
					continue;
				/*
				 * Package contains replaces="pkgpattern", but the
				 * package that should be replaced is also in the
				 * transaction and it's going to be updated.
				 */
				if (!instd_auto) {
					flappy_dictionary_remove(obj, "automatic-install");
				}
				if (!flappy_dictionary_set_bool(reppkgd, "replaced", true)) {
					flappy_object_iterator_release(iter);
					return false;
				}
				if (!flappy_transaction_pkg_type_set(reppkgd, FLAPPY_TRANS_REMOVE)) {
					flappy_object_iterator_release(iter);
					return false;
				}
				if (flappy_array_replace_dict_by_name(pkgs, reppkgd, curpkgname) != 0) {
					flappy_object_iterator_release(iter);
					return false;
				}
				flappy_verbose_printf("Package `%s' will be replaced by `%s'\n", curpkgver, pkgver);
				flappy_dbg_printf(
				    "Package `%s' in transaction will be "
				    "replaced by `%s', matched with `%s'\n",
				    curpkgver, pkgver, pattern);
				continue;
			}
			/*
			 * If new package is providing a virtual package to the
			 * package that we want to replace we should respect
			 * the automatic-install object.
			 */
			if (flappy_match_virtual_pkg_in_dict(obj, pattern)) {
				if (!instd_auto) {
					flappy_dictionary_remove(obj, "automatic-install");
				}
			}
			/*
			 * Add package dictionary into the transaction and mark
			 * it as to be "removed".
			 */
			if (!flappy_transaction_pkg_type_set(instd, FLAPPY_TRANS_REMOVE)) {
				flappy_object_iterator_release(iter);
				return false;
			}
			if (!flappy_dictionary_set_bool(instd, "replaced", true)) {
				flappy_object_iterator_release(iter);
				return false;
			}
			if (!flappy_array_add_first(pkgs, instd)) {
				flappy_object_iterator_release(iter);
				return false;
			}
			flappy_verbose_printf("Package `%s' will be replaced by `%s'\n", curpkgver, pkgver);
			flappy_dbg_printf(
			    "Package `%s' will be replaced by `%s', "
			    "matched with `%s'\n", curpkgver, pkgver, pattern);
		}
		flappy_object_iterator_release(iter);
	}

	return true;
}

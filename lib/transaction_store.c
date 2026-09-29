/*-
 * Copyright (c) 2014-2020 Juan Romero Pardines.
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

bool HIDDEN
flappy_transaction_store(struct flappy_handle *xhp, flappy_array_t pkgs,
		flappy_dictionary_t pkgrd, bool autoinst)
{
	flappy_dictionary_t d, pkgd;
	flappy_array_t replaces;
	const char *pkgver, *pkgname, *curpkgver, *repo;
	char *self_replaced;
	int rv;

	assert(xhp);
	assert(pkgs);
	assert(pkgrd);

	if (!flappy_dictionary_get_cstring_nocopy(pkgrd, "pkgver", &pkgver)) {
		return false;
	}
	if (!flappy_dictionary_get_cstring_nocopy(pkgrd, "pkgname", &pkgname)) {
		return false;
	}
	d = flappy_find_pkg_in_array(pkgs, pkgname, 0);
	if (flappy_object_type(d) == FLAPPY_TYPE_DICTIONARY) {
		/* compare version stored in transaction vs current */
		if (!flappy_dictionary_get_cstring_nocopy(d, "pkgver", &curpkgver)) {
			return false;
		}
		rv = flappy_cmpver(pkgver, curpkgver);
		if (rv == 0 || rv == -1) {
			/* same version or stored version greater than current */
			return true;
		} else {
			/*
			 * Current version is greater than stored,
			 * replace stored with current.
			 */
			if (!flappy_remove_pkg_from_array_by_pkgver(pkgs, curpkgver)) {
				return false;
			}
			flappy_dbg_printf("[trans] replaced %s with %s\n", curpkgver, pkgver);
		}
	}

	if ((pkgd = flappy_dictionary_copy_mutable(pkgrd)) == NULL)
		return false;

	/*
	 * Add required objects into package dep's dictionary.
	 */
	if (autoinst && !flappy_dictionary_set_bool(pkgd, "automatic-install", true))
		goto err;

	/*
	 * Set a replaces to itself, so that virtual packages are always replaced.
	*/
	if ((replaces = flappy_dictionary_get(pkgd, "replaces")) == NULL)
		replaces = flappy_array_create();

	self_replaced = flappy_xasprintf("%s>=0", pkgname);
	flappy_array_add_cstring(replaces, self_replaced);
	free(self_replaced);

	if (!flappy_dictionary_set(pkgd, "replaces", replaces))
		goto err;

	/*
	 * Add the dictionary into the unsorted queue.
	 */
	if (!flappy_array_add(pkgs, pkgd))
		goto err;

	flappy_dictionary_get_cstring_nocopy(pkgd, "repository", &repo);

	flappy_set_cb_state(xhp, FLAPPY_STATE_TRANS_ADDPKG, 0, pkgver,
	    "Found %s in repository %s", pkgver, repo);

	flappy_dbg_printf("[trans] `%s' stored%s (%s)\n", pkgver,
	    autoinst ? " as automatic install" : "", repo);
	flappy_object_release(pkgd);

	return true;
err:
	flappy_object_release(pkgd);
	return false;
}

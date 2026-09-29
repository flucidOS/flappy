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
#include <time.h>

#include "flappy_api_impl.h"

int HIDDEN
flappy_register_pkg(struct flappy_handle *xhp, flappy_dictionary_t pkgrd)
{
	flappy_array_t replaces;
	flappy_dictionary_t pkgd;
	time_t t;
	struct tm tm, *tmp;
	const char *pkgver, *pkgname;
	char sha256[FLAPPY_SHA256_SIZE], outstr[64], *buf;
	int rv = 0;

	assert(flappy_object_type(pkgrd) == FLAPPY_TYPE_DICTIONARY);

	flappy_dictionary_make_immutable(pkgrd);
	if ((pkgd = flappy_dictionary_copy_mutable(pkgrd)) == NULL) {
		goto out;
	}

	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	if (xhp->flags & FLAPPY_FLAG_INSTALL_REPRO) {
		/*
		 * Reproducible mode. Some objects must not be recorded:
		 * 	- install-date
		 * 	- repository
		 */
		flappy_dictionary_remove(pkgd, "repository");
	} else {
		/*
		 * Set the "install-date" object to know the pkg installation date.
		 */
		t = time(NULL);
		if ((tmp = localtime_r(&t, &tm)) == NULL) {
			flappy_dbg_printf("%s: localtime_r failed: %s\n",
					pkgver, strerror(errno));
			rv = EINVAL;
			goto out;
		}
		if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", tmp) == 0) {
			flappy_dbg_printf("%s: strftime failed: %s\n",
					pkgver, strerror(errno));
			rv = EINVAL;
			goto out;
		}
		if (!flappy_dictionary_set_cstring(pkgd, "install-date", outstr)) {
			flappy_dbg_printf("%s: install-date set failed!\n", pkgver);
			rv = EINVAL;
			goto out;
		}
	}
	/*
	 * Create a hash for the pkg's metafile if it exists.
	 */
	buf = flappy_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
	if (flappy_file_sha256(sha256, sizeof sha256, buf)) {
		flappy_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
	}
	free(buf);
	/*
	 * Remove self replacement when applicable.
	 */
	if ((replaces = flappy_dictionary_get(pkgd, "replaces"))) {
		buf = flappy_xasprintf("%s>=0", pkgname);
		flappy_remove_string_from_array(replaces, buf);
		free(buf);
		if (!flappy_array_count(replaces))
			flappy_dictionary_remove(pkgd, "replaces");
	}
	/*
	 * Remove unneeded objs from pkg dictionary.
	 */
	flappy_dictionary_remove(pkgd, "download");
	flappy_dictionary_remove(pkgd, "remove-and-update");
	flappy_dictionary_remove(pkgd, "transaction");
	flappy_dictionary_remove(pkgd, "skip-obsoletes");
	flappy_dictionary_remove(pkgd, "pkgname");
	flappy_dictionary_remove(pkgd, "version");

	if (!flappy_dictionary_set(xhp->pkgdb, pkgname, pkgd)) {
		flappy_dbg_printf("%s: failed to set pkgd for %s\n", __func__, pkgver);
	}
out:
	flappy_object_release(pkgd);

	return rv;
}

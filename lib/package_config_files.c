/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive_entry.h>

#include "flappy_api_impl.h"

/*
 * Returns true if entry is a configuration file, false otherwise.
 */
int HIDDEN
flappy_entry_is_a_conf_file(flappy_dictionary_t filesd,
			  const char *entry_pname)
{
	flappy_array_t array;
	flappy_dictionary_t d;
	const char *cffile;

	array = flappy_dictionary_get(filesd, "conf_files");
	if (flappy_array_count(array) == 0)
		return false;

	for (unsigned int i = 0; i < flappy_array_count(array); i++) {
		d = flappy_array_get(array, i);
		flappy_dictionary_get_cstring_nocopy(d, "file", &cffile);
		if (strcmp(cffile, entry_pname) == 0)
			return true;
	}
	return false;
}

/*
 * Returns 1 if entry should be installed, 0 if don't or -1 on error.
 */
int HIDDEN
flappy_entry_install_conf_file(struct flappy_handle *xhp,
			     flappy_dictionary_t binpkg_filesd,
			     flappy_dictionary_t pkg_filesd,
			     struct archive_entry *entry,
			     const char *entry_pname,
			     const char *pkgver,
			     bool mysymlink)
{
	flappy_object_t obj, obj2;
	flappy_object_iterator_t iter, iter2;
	const char *version = NULL, *cffile, *sha256_new = NULL;
	char buf[PATH_MAX], sha256_cur[FLAPPY_SHA256_SIZE];
	const char *sha256_orig = NULL;
	int rv = 0;

	assert(flappy_object_type(binpkg_filesd) == FLAPPY_TYPE_DICTIONARY);
	assert(entry);
	assert(entry_pname);
	assert(pkgver);

	iter = flappy_array_iter_from_dict(binpkg_filesd, "conf_files");
	if (iter == NULL)
		return -1;

	/*
	 * Get original hash for the file from current
	 * installed package.
	 */
	flappy_dbg_printf("%s: processing conf_file %s\n",
	    pkgver, entry_pname);

	if (pkg_filesd == NULL || mysymlink) {
		/*
		 * 1. File exists on disk but it's not managed by the same package.
		 * 2. File exists on disk as symlink.
		 * Install it as file.new-<version>.
		 */
		version = flappy_pkg_version(pkgver);
		if (!version)
			flappy_unreachable();
		flappy_dbg_printf("%s: conf_file %s not currently "
		    "installed, renaming to %s.new-%s\n", pkgver,
		    entry_pname, entry_pname, version);
		snprintf(buf, sizeof(buf), "%s.new-%s", entry_pname, version);
		flappy_set_cb_state(xhp, FLAPPY_STATE_CONFIG_FILE,
		    0, pkgver, "File `%s' exists, installing configuration file to `%s'.", entry_pname, buf);
		archive_entry_copy_pathname(entry, buf);
		rv = 1;
		goto out;
	}

	iter2 = flappy_array_iter_from_dict(pkg_filesd, "conf_files");
	if (iter2 != NULL) {
		while ((obj2 = flappy_object_iterator_next(iter2))) {
			flappy_dictionary_get_cstring_nocopy(obj2,
			    "file", &cffile);
			snprintf(buf, sizeof(buf), ".%s", cffile);
			if (strcmp(entry_pname, buf) == 0) {
				flappy_dictionary_get_cstring_nocopy(obj2, "sha256", &sha256_orig);
				break;
			}
		}
		flappy_object_iterator_release(iter2);
	}
	/*
	 * First case: original hash not found, install new file.
	 */
	if (sha256_orig == NULL) {
		flappy_dbg_printf("%s: conf_file %s not installed\n",
		    pkgver, entry_pname);
		rv = 1;
		goto out;
	}

	/*
	 * Compare original, installed and new hash for current file.
	 */
	while ((obj = flappy_object_iterator_next(iter))) {
		flappy_dictionary_get_cstring_nocopy(obj, "file", &cffile);
		snprintf(buf, sizeof(buf), ".%s", cffile);
		if (strcmp(entry_pname, buf)) {
			continue;
		}
		if (!flappy_file_sha256(sha256_cur, sizeof sha256_cur, buf)) {
			if (errno == ENOENT) {
				/*
				 * File not installed, install new one.
				 */
				flappy_dbg_printf("%s: conf_file %s not "
				    "installed\n", pkgver, entry_pname);
				rv = 1;
				break;
			} else {
				rv = -1;
				break;
			}
		}
		flappy_dictionary_get_cstring_nocopy(obj, "sha256", &sha256_new);
		/*
		 * Orig = X, Curr = X, New = X
		 *
		 * Keep file as is (no changes).
		 */
		if ((strcmp(sha256_orig, sha256_cur) == 0) &&
		    (strcmp(sha256_orig, sha256_new) == 0) &&
		    (strcmp(sha256_cur, sha256_new) == 0)) {
			flappy_dbg_printf("%s: conf_file %s orig = X, "
			    "cur = X, new = X\n", pkgver, entry_pname);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = X, New = Y
		 *
		 * Install new file (installed file hasn't been modified) if
		 * configuration option keepconfig is NOT set.
		 */
		} else if ((strcmp(sha256_orig, sha256_cur) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_cur, sha256_new)) &&
			   (!(xhp->flags & FLAPPY_FLAG_KEEP_CONFIG))) {
			flappy_set_cb_state(xhp, FLAPPY_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Updating configuration file `%s' provided "
			    "by `%s'.", cffile, pkgver);
			rv = 1;
			break;
		/*
		 * Orig = X, Curr = Y, New = X
		 *
		 * Keep installed file as is because it has been modified,
		 * but new package doesn't contain new changes compared
		 * to the original version.
		 */
		} else if ((strcmp(sha256_orig, sha256_new) == 0) &&
			   (strcmp(sha256_cur, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			flappy_set_cb_state(xhp, FLAPPY_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Keeping modified configuration file `%s'.",
			    cffile);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = Y, New = Y
		 *
		 * Keep file as is because changes made are compatible
		 * with new version.
		 */
		} else if ((strcmp(sha256_cur, sha256_new) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			flappy_dbg_printf("%s: conf_file %s orig = X, "
			    "cur = Y, new = Y\n", pkgver, entry_pname);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = Y, New = Z
		 * or
		 * Orig = X, Curr = X, New = Y if keepconf is set
		 *
		 * Install new file as <file>.new-<version>
		 */
		} else if (((strcmp(sha256_orig, sha256_cur)) &&
			    (strcmp(sha256_cur, sha256_new)) &&
			    (strcmp(sha256_orig, sha256_new))) ||
			    (xhp->flags & FLAPPY_FLAG_KEEP_CONFIG)) {
			version = flappy_pkg_version(pkgver);
			if (!version)
				flappy_unreachable();
			snprintf(buf, sizeof(buf), ".%s.new-%s", cffile, version);
			flappy_set_cb_state(xhp, FLAPPY_STATE_CONFIG_FILE,
			    0, pkgver, "File `%s' exists, installing configuration file to `%s'.", cffile, buf);
			archive_entry_copy_pathname(entry, buf);
			rv = 1;
			break;
		}
	}

out:

	flappy_object_iterator_release(iter);

	flappy_dbg_printf("%s: conf_file %s returned %d\n",
	    pkgver, entry_pname, rv);

	return rv;
}

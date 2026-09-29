/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <flappy.h>
#include "defs.h"

#define _BOLD	"\033[1m"
#define _RESET	"\033[m"

static void
print_value_obj(const char *keyname, flappy_object_t obj,
		const char *indent, const char *bold,
		const char *reset, bool raw)
{
	flappy_array_t allkeys;
	flappy_object_t obj2, keysym;
	const char *ksymname, *value;
	char size[8];

	if (indent == NULL)
		indent = "";

	switch (flappy_object_type(obj)) {
	case FLAPPY_TYPE_STRING:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		printf("%s\n", flappy_string_cstring_nocopy(obj));
		break;
	case FLAPPY_TYPE_NUMBER:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		if (flappy_humanize_number(size,
		    (int64_t)flappy_number_unsigned_integer_value(obj)) == -1)
			printf("%ju\n",
			    flappy_number_unsigned_integer_value(obj));
		else
			printf("%s\n", size);
		break;
	case FLAPPY_TYPE_BOOL:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		printf("%s\n", flappy_bool_true(obj) ? "yes" : "no");
		break;
	case FLAPPY_TYPE_ARRAY:
		if (!raw)
			printf("%s%s%s%s:\n", indent, bold, keyname, reset);
		for (unsigned int i = 0; i < flappy_array_count(obj); i++) {
			obj2 = flappy_array_get(obj, i);
			if (flappy_object_type(obj2) == FLAPPY_TYPE_STRING) {
				value = flappy_string_cstring_nocopy(obj2);
				printf("%s%s%s\n", indent, !raw ? "\t" : "",
				    value);
			} else {
				print_value_obj(keyname, obj2, "  ", bold, reset, raw);
			}
		}
		break;
	case FLAPPY_TYPE_DICTIONARY:
		if (!raw)
			printf("%s%s%s%s:\n", indent, bold, keyname, reset);
		allkeys = flappy_dictionary_all_keys(obj);
		for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
			keysym = flappy_array_get(allkeys, i);
			ksymname = flappy_dictionary_keysym_cstring_nocopy(keysym);
			obj2 = flappy_dictionary_get_keysym(obj, keysym);
			print_value_obj(ksymname, obj2, "  ", bold, reset, raw);
		}
		flappy_object_release(allkeys);
		if (raw)
			printf("\n");
		break;
	case FLAPPY_TYPE_DATA:
		if (!raw) {
			flappy_humanize_number(size, (int64_t)flappy_data_size(obj));
			printf("%s%s%s%s: %s\n", indent, bold, keyname, reset, size);
		} else {
			fwrite(flappy_data_data_nocopy(obj), 1, flappy_data_size(obj), stdout);
		}
		break;
	default:
		flappy_warn_printf("unknown obj type (key %s)\n",
		    keyname);
		break;
	}
}

void
show_pkg_info_one(flappy_dictionary_t d, const char *keys)
{
	flappy_object_t obj;
	const char *bold, *reset;
	char *key, *p, *saveptr;
	int v_tty = isatty(STDOUT_FILENO);
	bool raw;

	if (v_tty && !getenv("NO_COLOR")) {
		bold = _BOLD;
		reset = _RESET;
	} else {
		bold = "";
		reset = "";
	}

	if (strchr(keys, ',') == NULL) {
		obj = flappy_dictionary_get(d, keys);
		if (obj == NULL)
			return;
		raw = true;
		if (flappy_object_type(obj) == FLAPPY_TYPE_DICTIONARY)
			raw = false;
		print_value_obj(keys, obj, NULL, bold, reset, raw);
		return;
	}
	key = strdup(keys);
	if (key == NULL)
		abort();
	for ((p = strtok_r(key, ",", &saveptr)); p;
	    (p = strtok_r(NULL, ",", &saveptr))) {
		obj = flappy_dictionary_get(d, p);
		if (obj == NULL)
			continue;
		raw = true;
		if (flappy_object_type(obj) == FLAPPY_TYPE_DICTIONARY)
			raw = false;
		print_value_obj(p, obj, NULL, bold, reset, raw);
	}
	free(key);
}

void
show_pkg_info(flappy_dictionary_t dict)
{
	flappy_array_t all_keys;
	flappy_object_t obj, keysym;
	const char *keyname, *bold, *reset;
	int v_tty = isatty(STDOUT_FILENO);

	if (v_tty && !getenv("NO_COLOR")) {
		bold = _BOLD;
		reset = _RESET;
	} else {
		bold = "";
		reset = "";
	}

	all_keys = flappy_dictionary_all_keys(dict);
	for (unsigned int i = 0; i < flappy_array_count(all_keys); i++) {
		keysym = flappy_array_get(all_keys, i);
		keyname = flappy_dictionary_keysym_cstring_nocopy(keysym);
		obj = flappy_dictionary_get_keysym(dict, keysym);
		/* anything else */
		print_value_obj(keyname, obj, NULL, bold, reset, false);
	}
	flappy_object_release(all_keys);
}

int
show_pkg_files(flappy_dictionary_t filesd)
{
	flappy_array_t array, allkeys;
	flappy_object_t obj;
	flappy_dictionary_keysym_t ksym;
	const char *keyname = NULL, *file = NULL;

	if (flappy_object_type(filesd) != FLAPPY_TYPE_DICTIONARY)
		return EINVAL;

	allkeys = flappy_dictionary_all_keys(filesd);
	for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
		ksym = flappy_array_get(allkeys, i);
		keyname = flappy_dictionary_keysym_cstring_nocopy(ksym);
		if ((strcmp(keyname, "files") &&
		    (strcmp(keyname, "conf_files") &&
		    (strcmp(keyname, "links")))))
			continue;

		array = flappy_dictionary_get(filesd, keyname);
		if (array == NULL || flappy_array_count(array) == 0)
			continue;

		for (unsigned int x = 0; x < flappy_array_count(array); x++) {
			obj = flappy_array_get(array, x);
			if (flappy_object_type(obj) != FLAPPY_TYPE_DICTIONARY)
				continue;
			flappy_dictionary_get_cstring_nocopy(obj, "file", &file);
			printf("%s", file);
			if (flappy_dictionary_get_cstring_nocopy(obj,
			    "target", &file))
				printf(" -> %s", file);

			printf("\n");
		}
	}
	flappy_object_release(allkeys);

	return 0;
}

int
show_pkg_info_from_metadir(struct flappy_handle *xhp,
			   const char *pkg,
			   const char *option)
{
	flappy_dictionary_t d;

	d = flappy_pkgdb_get_pkg(xhp, pkg);
	if (d == NULL)
		return ENOENT;

	if (option == NULL)
		show_pkg_info(d);
	else
		show_pkg_info_one(d, option);

	return 0;
}

int
show_pkg_files_from_metadir(struct flappy_handle *xhp, const char *pkg)
{
	flappy_dictionary_t d;
	int rv = 0;

	d = flappy_pkgdb_get_pkg_files(xhp, pkg);
	if (d == NULL)
		return ENOENT;

	rv = show_pkg_files(d);

	return rv;
}

int
repo_show_pkg_info(struct flappy_handle *xhp,
		   const char *pattern,
		   const char *option)
{
	flappy_dictionary_t pkgd;

	if (((pkgd = flappy_rpool_get_pkg(xhp, pattern)) == NULL) &&
	    ((pkgd = flappy_rpool_get_virtualpkg(xhp, pattern)) == NULL))
		return errno;

	if (option)
		show_pkg_info_one(pkgd, option);
	else
		show_pkg_info(pkgd);

	return 0;
}

int
cat_file(struct flappy_handle *xhp, const char *pkg, const char *file)
{
	char bfile[PATH_MAX];
	flappy_dictionary_t pkgd;
	int rv;

	pkgd = flappy_pkgdb_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = flappy_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		flappy_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	return flappy_archive_fetch_file_into_fd(bfile, file, STDOUT_FILENO);
}

int
repo_cat_file(struct flappy_handle *xhp, const char *pkg, const char *file)
{
	char bfile[PATH_MAX];
	flappy_dictionary_t pkgd;
	int rv;

	pkgd = flappy_rpool_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = flappy_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		flappy_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	return flappy_archive_fetch_file_into_fd(bfile, file, STDOUT_FILENO);
}

int
repo_show_pkg_files(struct flappy_handle *xhp, const char *pkg)
{
	char bfile[PATH_MAX];
	flappy_dictionary_t pkgd, filesd;
	int rv;

	pkgd = flappy_rpool_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = flappy_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		flappy_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	filesd = flappy_archive_fetch_plist(bfile, "/files.plist");
	if (filesd == NULL) {
                if (errno != ENOTSUP && errno != ENOENT) {
			flappy_error_printf("Unexpected error: %s\n", strerror(errno));
		}
		return errno;
	}

	rv = show_pkg_files(filesd);
	flappy_object_release(filesd);
	return rv;
}

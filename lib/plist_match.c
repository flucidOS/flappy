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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "flappy_api_impl.h"

/**
 * @file lib/plist_find.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
bool
flappy_match_virtual_pkg_in_array(flappy_array_t a, const char *str)
{
	if (flappy_pkgpattern_version(str)) {
		if (flappy_match_pkgdep_in_array(a, str) ||
		    flappy_match_pkgpattern_in_array(a, str))
		return true;
	} else if (flappy_pkg_version(str)) {
		return flappy_match_string_in_array(a, str);
	} else {
		return flappy_match_pkgname_in_array(a, str);
	}
	return false;
}

bool
flappy_match_virtual_pkg_in_dict(flappy_dictionary_t d, const char *str)
{
	flappy_array_t provides;

	assert(flappy_object_type(d) == FLAPPY_TYPE_DICTIONARY);

	if ((provides = flappy_dictionary_get(d, "provides")))
		return flappy_match_virtual_pkg_in_array(provides, str);

	return false;
}

bool
flappy_match_any_virtualpkg_in_rundeps(flappy_array_t rundeps,
				     flappy_array_t provides)
{
	flappy_object_t obj, obj2;
	flappy_object_iterator_t iter, iter2;
	const char *vpkgver, *pkgpattern;

	iter = flappy_array_iterator(provides);
	assert(iter);

	while ((obj = flappy_object_iterator_next(iter))) {
		vpkgver = flappy_string_cstring_nocopy(obj);
		iter2 = flappy_array_iterator(rundeps);
		assert(iter2);
		while ((obj2 = flappy_object_iterator_next(iter2))) {
			pkgpattern = flappy_string_cstring_nocopy(obj2);
			if (flappy_pkgpattern_match(vpkgver, pkgpattern)) {
				flappy_object_iterator_release(iter2);
				flappy_object_iterator_release(iter);
				return true;
			}
		}
		flappy_object_iterator_release(iter2);
	}
	flappy_object_iterator_release(iter);

	return false;
}

static bool
match_string_in_array(flappy_array_t array, const char *str, int mode)
{
	char pkgname[FLAPPY_NAME_SIZE];
	bool found = false;

	assert(flappy_object_type(array) == FLAPPY_TYPE_ARRAY);
	assert(str != NULL);

	for (unsigned int i = 0; i < flappy_array_count(array); i++) {
		flappy_object_t obj = flappy_array_get(array, i);
		if (mode == 0) {
			/* match by string */
			if (flappy_string_equals_cstring(obj, str)) {
				found = true;
				break;
			}
		} else if (mode == 1) {
			/* match by pkgname against pkgver */
			const char *pkgdep = flappy_string_cstring_nocopy(obj);
			if (!flappy_pkg_name(pkgname, FLAPPY_NAME_SIZE, pkgdep))
				break;
			if (strcmp(pkgname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 2) {
			/* match by pkgver against pkgname */
			const char *pkgdep = flappy_string_cstring_nocopy(obj);
			if (!flappy_pkg_name(pkgname, FLAPPY_NAME_SIZE, str))
				break;
			if (strcmp(pkgname, pkgdep) == 0) {
				found = true;
				break;
			}
		} else if (mode == 3) {
			/* match pkgpattern against pkgdep */
			const char *pkgdep = flappy_string_cstring_nocopy(obj);
			if (flappy_pkgpattern_match(pkgdep, str)) {
				found = true;
				break;
			}
		} else if (mode == 4) {
			/* match pkgdep against pkgpattern */
			const char *pkgdep = flappy_string_cstring_nocopy(obj);
			if (flappy_pkgpattern_match(str, pkgdep)) {
				found = true;
				break;
			}
		}
	}

	return found;
}

bool
flappy_match_string_in_array(flappy_array_t array, const char *str)
{
	return match_string_in_array(array, str, 0);
}

bool
flappy_match_pkgname_in_array(flappy_array_t array, const char *pkgname)
{
	return match_string_in_array(array, pkgname, 1);
}

bool
flappy_match_pkgver_in_array(flappy_array_t array, const char *pkgver)
{
	return match_string_in_array(array, pkgver, 2);
}

bool
flappy_match_pkgpattern_in_array(flappy_array_t array, const char *pattern)
{
	return match_string_in_array(array, pattern, 3);
}

bool
flappy_match_pkgdep_in_array(flappy_array_t array, const char *pkgver)
{
	return match_string_in_array(array, pkgver, 4);
}

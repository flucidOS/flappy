/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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

#include <sys/param.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <flappy.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing run time dependencies.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_rundeps(struct flappy_handle *xhp, const char *pkgname, flappy_dictionary_t pkg_propsd)
{
	flappy_array_t array;
	const char *reqpkg = NULL;
	int rv = 0;

	array = flappy_dictionary_get(pkg_propsd, "run_depends");
	for (unsigned int i = 0; i < flappy_array_count(array); i++) {
		flappy_array_get_cstring_nocopy(array, i, &reqpkg);
		if (flappy_pkg_is_ignored(xhp, reqpkg))
			continue;
		if (flappy_pkg_is_installed(xhp, reqpkg) <= 0) {
			flappy_error_printf("%s: dependency not satisfied: %s\n",
			    pkgname, reqpkg);
			rv = -1;
		}
	}
	return rv;
}

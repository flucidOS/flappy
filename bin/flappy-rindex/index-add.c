/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <flappy.h>
#include "defs.h"

static int
repodata_commit(const char *repodir, const char *repoarch,
	flappy_dictionary_t index, flappy_dictionary_t stage, flappy_dictionary_t meta,
	const char *compression)
{
	flappy_object_iterator_t iter;
	flappy_object_t keysym;
	int r;
	flappy_dictionary_t oldshlibs, usedshlibs;

	if (flappy_dictionary_count(stage) == 0)
		return 0;

	/*
	 * Find old shlibs-provides
	 */
	oldshlibs = flappy_dictionary_create();
	usedshlibs = flappy_dictionary_create();

	iter = flappy_dictionary_iterator(stage);
	while ((keysym = flappy_object_iterator_next(iter))) {
		const char *pkgname = flappy_dictionary_keysym_cstring_nocopy(keysym);
		flappy_dictionary_t pkg = flappy_dictionary_get(index, pkgname);
		flappy_array_t pkgshlibs;

		pkgshlibs = flappy_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < flappy_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			flappy_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			flappy_dictionary_set_cstring(oldshlibs, shlib, pkgname);
		}
	}
	flappy_object_iterator_release(iter);

	/*
	 * throw away all unused shlibs
	 */
	iter = flappy_dictionary_iterator(index);
	while ((keysym = flappy_object_iterator_next(iter))) {
		const char *pkgname = flappy_dictionary_keysym_cstring_nocopy(keysym);
		flappy_dictionary_t pkg = flappy_dictionary_get(stage, pkgname);
		flappy_array_t pkgshlibs;
		if (!pkg)
			pkg = flappy_dictionary_get_keysym(index, keysym);
		pkgshlibs = flappy_dictionary_get(pkg, "shlib-requires");

		for (unsigned int i = 0; i < flappy_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			bool alloc = false;
			flappy_array_t users;
			flappy_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			if (!flappy_dictionary_get(oldshlibs, shlib))
				continue;
			users = flappy_dictionary_get(usedshlibs, shlib);
			if (!users) {
				users = flappy_array_create();
				flappy_dictionary_set(usedshlibs, shlib, users);
				alloc = true;
			}
			flappy_array_add_cstring(users, pkgname);
			if (alloc)
				flappy_object_release(users);
		}
	}
	flappy_object_iterator_release(iter);

	/*
	 * purge all packages that are fullfilled by the index and
	 * not in the stage.
	 */
	iter = flappy_dictionary_iterator(index);
	while ((keysym = flappy_object_iterator_next(iter))) {
		flappy_dictionary_t pkg = flappy_dictionary_get_keysym(index, keysym);
		flappy_array_t pkgshlibs;


		if (flappy_dictionary_get(stage,
					flappy_dictionary_keysym_cstring_nocopy(keysym))) {
			continue;
		}

		pkgshlibs = flappy_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < flappy_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			flappy_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			flappy_dictionary_remove(usedshlibs, shlib);
		}
	}
	flappy_object_iterator_release(iter);

	/*
	 * purge all packages that are fullfilled by the stage
	 */
	iter = flappy_dictionary_iterator(stage);
	while ((keysym = flappy_object_iterator_next(iter))) {
		flappy_dictionary_t pkg = flappy_dictionary_get_keysym(stage, keysym);
		flappy_array_t pkgshlibs;

		pkgshlibs = flappy_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < flappy_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			flappy_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			flappy_dictionary_remove(usedshlibs, shlib);
		}
	}
	flappy_object_iterator_release(iter);

	if (flappy_dictionary_count(usedshlibs) != 0) {
		printf("Inconsistent shlibs:\n");
		iter = flappy_dictionary_iterator(usedshlibs);
		while ((keysym = flappy_object_iterator_next(iter))) {
			const char *shlib = flappy_dictionary_keysym_cstring_nocopy(keysym),
					*provider = NULL, *pre;
			flappy_array_t users = flappy_dictionary_get(usedshlibs, shlib);
			flappy_dictionary_get_cstring_nocopy(oldshlibs, shlib, &provider);

			printf("  %s (provided by: %s; used by: ", shlib, provider);
			pre = "";
			for (unsigned int i = 0; i < flappy_array_count(users); i++) {
				const char *user = NULL;
				flappy_array_get_cstring_nocopy(users, i, &user);
				printf("%s%s", pre, user);
				pre = ", ";
			}
			printf(")\n");
		}
		flappy_object_iterator_release(iter);
		iter = flappy_dictionary_iterator(stage);
		while ((keysym = flappy_object_iterator_next(iter))) {
			flappy_dictionary_t pkg = flappy_dictionary_get_keysym(stage, keysym);
			const char *pkgver = NULL, *arch = NULL;
			flappy_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver);
			flappy_dictionary_get_cstring_nocopy(pkg, "architecture", &arch);
			printf("stage: added `%s' (%s)\n", pkgver, arch);
		}
		flappy_object_iterator_release(iter);
	} else {
		iter = flappy_dictionary_iterator(stage);
		while ((keysym = flappy_object_iterator_next(iter))) {
			const char *pkgname = flappy_dictionary_keysym_cstring_nocopy(keysym);
			flappy_dictionary_t pkg = flappy_dictionary_get_keysym(stage, keysym);
			const char *pkgver = NULL, *arch = NULL;
			flappy_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver);
			flappy_dictionary_get_cstring_nocopy(pkg, "architecture", &arch);
			printf("index: added `%s' (%s).\n", pkgver, arch);
			flappy_dictionary_set(index, pkgname, pkg);
		}
		flappy_object_iterator_release(iter);
		stage = NULL;
	}

	r = repodata_flush(repodir, repoarch, index, stage, meta, compression);
	flappy_object_release(usedshlibs);
	flappy_object_release(oldshlibs);
	return r;
}

static int
index_add_pkg(struct flappy_handle *xhp, flappy_dictionary_t index, flappy_dictionary_t stage,
		const char *file, bool force)
{
	char sha256[FLAPPY_SHA256_SIZE];
	char pkgname[FLAPPY_NAME_SIZE];
	struct stat st;
	const char *arch = NULL;
	const char *pkgver = NULL;
	flappy_dictionary_t binpkgd, curpkgd;
	int r;

	/*
	 * Read metadata props plist dictionary from binary package.
	 */
	binpkgd = flappy_archive_fetch_plist(file, "/props.plist");
	if (!binpkgd) {
		flappy_error_printf("index: failed to read %s metadata for "
		    "`%s', skipping!\n", FLAPPY_PKGPROPS, file);
		return 0;
	}
	flappy_dictionary_get_cstring_nocopy(binpkgd, "architecture", &arch);
	flappy_dictionary_get_cstring_nocopy(binpkgd, "pkgver", &pkgver);
	if (!flappy_pkg_arch_match(xhp, arch, NULL)) {
		fprintf(stderr, "index: ignoring %s, unmatched arch (%s)\n", pkgver, arch);
		goto out;
	}
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		r = -EINVAL;
		goto err;
	}

	/*
	 * Check if this package exists already in the index, but first
	 * checking the version. If current package version is greater
	 * than current registered package, update the index; otherwise
	 * pass to the next one.
	 */
	curpkgd = flappy_dictionary_get(stage, pkgname);
	if (!curpkgd)
		curpkgd = flappy_dictionary_get(index, pkgname);

	if (curpkgd && !force) {
		const char *opkgver = NULL, *oarch = NULL;
		int cmp;

		flappy_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &opkgver);
		flappy_dictionary_get_cstring_nocopy(curpkgd, "architecture", &oarch);

		cmp = flappy_cmpver(pkgver, opkgver);
		if (cmp < 0 && flappy_pkg_reverts(binpkgd, opkgver)) {
			/*
			 * If the considered package reverts the package in the index,
			 * consider the current package as the newer one.
			 */
			cmp = 1;
		} else if (cmp > 0 && flappy_pkg_reverts(curpkgd, pkgver)) {
			/*
			 * If package in the index reverts considered package, consider the
			 * package in the index as the newer one.
			 */
			cmp = -1;
		}
		if (cmp <= 0) {
			fprintf(stderr, "index: skipping `%s' (%s), already registered.\n", pkgver, arch);
			goto out;
		}
	}

	if (!flappy_file_sha256(sha256, sizeof(sha256), file))
		goto err_errno;
	if (!flappy_dictionary_set_cstring(binpkgd, "filename-sha256", sha256))
		goto err_errno;
	if (stat(file, &st) == -1)
		goto err_errno;
	if (!flappy_dictionary_set_uint64(binpkgd, "filename-size", (uint64_t)st.st_size))
		goto err_errno;

	flappy_dictionary_remove(binpkgd, "pkgname");
	flappy_dictionary_remove(binpkgd, "version");
	flappy_dictionary_remove(binpkgd, "packaged-with");

	/*
	 * Add new pkg dictionary into the stage index
	 */
	if (!flappy_dictionary_set(stage, pkgname, binpkgd))
		goto err_errno;

out:
	flappy_object_release(binpkgd);
	return 0;
err_errno:
	r = -errno;
err:
	flappy_object_release(binpkgd);
	return r;
}

int
index_add(struct flappy_handle *xhp, int args, int argc, char **argv, bool force, const char *compression)
{
	flappy_dictionary_t index, stage, meta;
	struct flappy_repo *repo;
	char *tmprepodir = NULL, *repodir = NULL;
	int lockfd;
	int r;
	const char *repoarch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;

	if ((tmprepodir = strdup(argv[args])) == NULL)
		return EXIT_FAILURE;
	repodir = dirname(tmprepodir);

	lockfd = flappy_repo_lock(repodir, repoarch);
	if (lockfd < 0) {
		flappy_error_printf("flappy-rindex: cannot lock repository "
		    "%s: %s\n", repodir, strerror(-lockfd));
		free(tmprepodir);
		return EXIT_FAILURE;
	}

	repo = flappy_repo_open(xhp, repodir);
	if (!repo && errno != ENOENT) {
		free(tmprepodir);
		return EXIT_FAILURE;
	}

	if (repo) {
		index = flappy_dictionary_copy_mutable(repo->index);
		stage = flappy_dictionary_copy_mutable(repo->stage);
		meta = flappy_dictionary_copy_mutable(repo->idxmeta);
	} else {
		index = flappy_dictionary_create();
		stage = flappy_dictionary_create();
		meta = NULL;
	}

	for (int i = args; i < argc; i++) {
		r = index_add_pkg(xhp, index, stage, argv[i], force);
		if (r < 0)
			goto err2;
	}

	r = repodata_commit(repodir, repoarch, index, stage, meta, compression);
	if (r < 0) {
		flappy_error_printf("failed to write repodata: %s\n", strerror(-r));
		goto err2;
	}
	printf("index: %u packages registered.\n", flappy_dictionary_count(index));

	flappy_object_release(index);
	flappy_object_release(stage);
	if (meta)
		flappy_object_release(meta);
	flappy_repo_release(repo);
	flappy_repo_unlock(repodir, repoarch, lockfd);
	free(tmprepodir);
	return EXIT_SUCCESS;

err2:
	flappy_object_release(index);
	flappy_object_release(stage);
	if (meta)
		flappy_object_release(meta);
	flappy_repo_release(repo);
	flappy_repo_unlock(repodir, repoarch, lockfd);
	free(tmprepodir);
	return EXIT_FAILURE;
}

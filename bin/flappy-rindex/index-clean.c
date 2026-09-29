/*-
 * Copyright (c) 2012-2019 Juan Romero Pardines.
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
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <flappy.h>

#include "defs.h"

struct cleaner_ctx {
	const char *repourl;
	bool hashcheck;
	flappy_dictionary_t dict;
};

static int
idx_cleaner_cb(struct flappy_handle *xhp UNUSED,
		flappy_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	char path[PATH_MAX];
	char pkgname[FLAPPY_NAME_SIZE];
	struct cleaner_ctx *ctx = arg;
	const char *arch = NULL, *pkgver = NULL;
	int r;

	flappy_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	flappy_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

	flappy_dbg_printf("%s: checking %s [%s] ...\n", ctx->repourl, pkgver, arch);

	r = snprintf(path, sizeof(path), "%s/%s.%s.flappy", ctx->repourl, pkgver, arch);
	if (r < 0 || (size_t)r >= sizeof(path)) {
		r = -ENAMETOOLONG;
		flappy_error_printf("package path too long: %s: %s\n", path, strerror(-r));
		return r;
	}
	if (access(path, R_OK) == -1) {
		/*
		 * File cannot be read, might be permissions,
		 * broken or simply unexistent; either way, remove it.
		 */
		goto remove;
	}
	if (ctx->hashcheck) {
		const char *sha256 = NULL;
		/*
		 * File can be read; check its hash.
		 */
		flappy_dictionary_get_cstring_nocopy(obj, "filename-sha256", &sha256);
		if (flappy_file_sha256_check(path, sha256) != 0)
			goto remove;
	}

	return 0;
remove:
	if (!flappy_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		flappy_error_printf("invalid pkgver: %s\n", pkgver);
		return -EINVAL;
	}
	flappy_dictionary_remove(ctx->dict, pkgname);
	printf("index: removed pkg %s\n", pkgver);
	return 0;
}

static int
cleanup_repo(struct flappy_handle *xhp, const char *repodir, struct flappy_repo *repo,
	bool hashcheck, const char *compression)
{
	struct cleaner_ctx ctx = {
		.hashcheck = hashcheck,
		.repourl = repodir
	};
	flappy_dictionary_t index, stage;
	flappy_array_t allkeys;
	int r = 0;
	const char *repoarch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;
	/*
	 * First pass: find out obsolete entries on index and index-files.
	 */
	index = flappy_dictionary_copy_mutable(repo->index);
	stage = flappy_dictionary_copy_mutable(repo->stage);

	allkeys = flappy_dictionary_all_keys(index);
	ctx.dict = index;
	(void)flappy_array_foreach_cb_multi(xhp, allkeys, repo->idx, idx_cleaner_cb, &ctx);
	flappy_object_release(allkeys);

	allkeys = flappy_dictionary_all_keys(stage);
	ctx.dict = stage;
	(void)flappy_array_foreach_cb_multi(xhp, allkeys, repo->idx, idx_cleaner_cb, &ctx);
	flappy_object_release(allkeys);

	if (flappy_dictionary_equals(index, repo->index) &&
	    flappy_dictionary_equals(stage, repo->stage)) {
		flappy_object_release(index);
		flappy_object_release(stage);
		return 0;
	}

	r = repodata_flush(repodir, repoarch, index, stage, repo->idxmeta, compression);
	if (r < 0) {
		flappy_error_printf("failed to write repodata: %s\n", strerror(-r));
		flappy_object_release(index);
		flappy_object_release(stage);
		return r;
	}
	printf("stage: %u packages registered.\n", flappy_dictionary_count(stage));
	printf("index: %u packages registered.\n", flappy_dictionary_count(index));
	flappy_object_release(index);
	flappy_object_release(stage);
	return 0;
}

/*
 * Removes stalled pkg entries in repository's FLAPPY_REPOIDX file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
int
index_clean(struct flappy_handle *xhp, const char *repodir, const bool hashcheck, const char *compression)
{
	struct flappy_repo *repo;
	const char *arch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;
	int lockfd;
	int r;

	lockfd = flappy_repo_lock(repodir, arch);
	if (lockfd < 0) {
		flappy_error_printf("cannot lock repository: %s\n", strerror(-lockfd));
		return EXIT_FAILURE;
	}

	repo = flappy_repo_open(xhp, repodir);
	if (!repo) {
		r = -errno;
		if (r == -ENOENT) {
			flappy_repo_unlock(repodir, arch, lockfd);
			return 0;
		}
		flappy_error_printf("cannot read repository data: %s\n",
		    strerror(-r));
		flappy_repo_unlock(repodir, arch, lockfd);
		return EXIT_FAILURE;
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

	r = cleanup_repo(xhp, repodir, repo, hashcheck, compression);
	flappy_repo_release(repo);
	flappy_repo_unlock(repodir, arch, lockfd);
	return r ? EXIT_FAILURE : EXIT_SUCCESS;
}

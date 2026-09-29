/*-
 * Copyright (c) 2015-2019 Juan Romero Pardines.
 * Copyright (c) 2019 Duncan Overbruck.
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

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "flappy_api_impl.h"

/**
 * @file lib/package_alternatives.c
 * @brief Alternatives generic routines
 * @defgroup alternatives Alternatives generic functions
 *
 * These functions implement the alternatives framework.
 */

static char *
left(const char *str)
{
	const char *e;

	e = strchr(str, ':');
	if (!e)
		return NULL;

	return strndup(str, e - str);
}

static const char *
right(const char *str)
{
	const char *p = strchr(str, ':');
	if (!p)
		return NULL;
	return p + 1;
}

static const char *
normpath(char *path)
{
	char *seg, *p;

	for (p = path, seg = NULL; *p; p++) {
		if (strncmp(p, "/../", 4) == 0 || strncmp(p, "/..", 4) == 0) {
			memmove(seg ? seg : p, p+3, strlen(p+3) + 1);
			return normpath(path);
		} else if (strncmp(p, "/./", 3) == 0 || strncmp(p, "/.", 3) == 0) {
			memmove(p, p+2, strlen(p+2) + 1);
		} else if (strncmp(p, "//", 2) == 0 || strncmp(p, "/", 2) == 0) {
			memmove(p, p+1, strlen(p+1) + 1);
		}
		if (*p == '/')
			seg = p;
	}
	return path;
}

static char *
relpath(char *from, char *to)
{
	int up;
	char *p = to, *rel;

	assert(from[0] == '/');
	assert(to[0] == '/');
	normpath(from);
	normpath(to);

	for (; *from == *to && *to; from++, to++) {
		if (*to == '/')
			p = to;
	}

	for (up = -1, from--; from && *from; from = strchr(from + 1, '/'), up++);

	rel = calloc(1, 3 * up + strlen(p) + 1);
	if (!rel)
		return NULL;

	while (up--)
		strcat(rel, "../");
	if (*p)
		strcat(rel, p+1);
	return rel;
}

static int
remove_symlinks(struct flappy_handle *xhp, flappy_array_t a, const char *grname)
{
	unsigned int i, cnt;
	struct stat st;

	cnt = flappy_array_count(a);
	for (i = 0; i < cnt; i++) {
		flappy_string_t str;
		char *l, *lnk;

		str = flappy_array_get(a, i);
		l = left(flappy_string_cstring_nocopy(str));
		assert(l);
		if (l[0] != '/') {
			const char *tgt;
			char *tgt_dup, *tgt_dir;
			tgt = right(flappy_string_cstring_nocopy(str));
			assert(tgt);
			tgt_dup = strdup(tgt);
			assert(tgt_dup);
			tgt_dir = dirname(tgt_dup);
			lnk = flappy_xasprintf("%s%s/%s", xhp->rootdir, tgt_dir, l);
			free(tgt_dup);
		} else {
			lnk = flappy_xasprintf("%s%s", xhp->rootdir, l);
		}
		if (lstat(lnk, &st) == -1 || !S_ISLNK(st.st_mode)) {
			free(lnk);
			free(l);
			continue;
		}
		flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_LINK_REMOVED, 0, NULL,
		    "Removing '%s' alternatives group symlink: %s", grname, l);
		unlink(lnk);
		free(lnk);
		free(l);
	}

	return 0;
}

static int
create_symlinks(struct flappy_handle *xhp, flappy_array_t a, const char *grname)
{
	int rv;
	unsigned int i, n;
	char *alternative, *tok1, *tok2, *linkpath, *target, *dir, *p;

	n = flappy_array_count(a);

	for (i = 0; i < n; i++) {
		alternative = flappy_string_cstring(flappy_array_get(a, i));

		if (!(tok1 = strtok(alternative, ":")) ||
		    !(tok2 = strtok(NULL, ":"))) {
			free(alternative);
			return EINVAL;
		}

		target = strdup(tok2);
		dir = dirname(tok2);

		/* add target dir to relative links */
		if (tok1[0] != '/')
			linkpath = flappy_xasprintf("%s/%s/%s", xhp->rootdir, dir, tok1);
		else
			linkpath = flappy_xasprintf("%s/%s", xhp->rootdir, tok1);

		/* create target directory, necessary for dangling symlinks */
		dir = flappy_xasprintf("%s/%s", xhp->rootdir, dir);
		if (strcmp(dir, ".") && flappy_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			flappy_dbg_printf(
			    "failed to create target dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			free(dir);
			goto err;
		}
		free(dir);

		/* create link directory, necessary for dangling symlinks */
		p = strdup(linkpath);
		dir = dirname(p);
		if (strcmp(dir, ".") && flappy_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			flappy_dbg_printf(
			    "failed to create symlink dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			free(p);
			goto err;
		}
		free(p);

		flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_LINK_ADDED, 0, NULL,
		    "Creating '%s' alternatives group symlink: %s -> %s",
		    grname, tok1, target);

		if (target[0] == '/') {
			p = relpath(linkpath + strlen(xhp->rootdir), target);
			free(target);
			target = p;
		}

		unlink(linkpath);
		if ((rv = symlink(target, linkpath)) != 0) {
			flappy_dbg_printf(
			    "failed to create alt symlink '%s' for group '%s': %s\n",
			    linkpath, grname,  strerror(errno));
			goto err;
		}

		free(alternative);
		free(target);
		free(linkpath);
	}

	return 0;

err:
	free(alternative);
	free(target);
	free(linkpath);
	return rv;
}

int
flappy_alternatives_set(struct flappy_handle *xhp, const char *pkgname,
		const char *group)
{
	flappy_array_t allkeys;
	flappy_dictionary_t alternatives, pkg_alternatives, pkgd, prevpkgd, prevpkg_alts;
	const char *pkgver = NULL, *prevpkgname = NULL;
	int rv = 0;

	assert(xhp);
	assert(pkgname);

	alternatives = flappy_dictionary_get(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_");
	if (alternatives == NULL)
		return ENOENT;

	pkgd = flappy_pkgdb_get_pkg(xhp, pkgname);
	if (pkgd == NULL)
		return ENOENT;

	pkg_alternatives = flappy_dictionary_get(pkgd, "alternatives");
	if (!flappy_dictionary_count(pkg_alternatives))
		return ENOENT;

	if (group && !flappy_dictionary_get(pkg_alternatives, group))
		return ENOENT;

	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	allkeys = flappy_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
		flappy_array_t array;
		flappy_object_t keysym;
		flappy_string_t kstr;
		const char *keyname;

		keysym = flappy_array_get(allkeys, i);
		keyname = flappy_dictionary_keysym_cstring_nocopy(keysym);

		if (group && strcmp(keyname, group))
			continue;

		array = flappy_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		/* remove symlinks from previous alternative */
		flappy_array_get_cstring_nocopy(array, 0, &prevpkgname);
		if (prevpkgname && strcmp(pkgname, prevpkgname) != 0) {
			if ((prevpkgd = flappy_pkgdb_get_pkg(xhp, prevpkgname)) &&
			    (prevpkg_alts = flappy_dictionary_get(prevpkgd, "alternatives")) &&
			    flappy_dictionary_count(prevpkg_alts)) {
				rv = remove_symlinks(xhp,
				    flappy_dictionary_get(prevpkg_alts, keyname),
				    keyname);
				if (rv != 0)
					break;
			}
		}

		/* put this alternative group at the head */
		flappy_remove_string_from_array(array, pkgname);
		kstr = flappy_string_create_cstring(pkgname);
		flappy_array_add_first(array, kstr);
		flappy_object_release(kstr);

		/* apply the alternatives group */
		flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: applying '%s' alternatives group", pkgver, keyname);
		rv = create_symlinks(xhp, flappy_dictionary_get(pkg_alternatives, keyname), keyname);
		if (rv != 0 || group)
			break;
	}
	flappy_object_release(allkeys);
	return rv;
}

static int
switch_alt_group(struct flappy_handle *xhp, const char *grpn, const char *pkgn,
		flappy_dictionary_t *pkg_alternatives)
{
	flappy_dictionary_t curpkgd, pkgalts;

	curpkgd = flappy_pkgdb_get_pkg(xhp, pkgn);
	assert(curpkgd);

	flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_SWITCHED, 0, NULL,
		"Switched '%s' alternatives group to '%s'", grpn, pkgn);
	pkgalts = flappy_dictionary_get(curpkgd, "alternatives");
	if (pkg_alternatives) *pkg_alternatives = pkgalts;
	return create_symlinks(xhp, flappy_dictionary_get(pkgalts, grpn), grpn);
}

int
flappy_alternatives_unregister(struct flappy_handle *xhp, flappy_dictionary_t pkgd)
{
	flappy_array_t allkeys;
	flappy_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver, *pkgname;
	bool update = false;
	int rv = 0;

	assert(xhp);

	alternatives = flappy_dictionary_get(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_");
	if (alternatives == NULL)
		return 0;

	pkg_alternatives = flappy_dictionary_get(pkgd, "alternatives");
	if (!flappy_dictionary_count(pkg_alternatives))
		return 0;

	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	flappy_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	flappy_dictionary_get_bool(pkgd, "alternatives-update", &update);

	allkeys = flappy_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
		flappy_array_t array;
		flappy_object_t keysym;
		bool current = false;
		const char *first = NULL, *keyname;

		keysym = flappy_array_get(allkeys, i);
		keyname = flappy_dictionary_keysym_cstring_nocopy(keysym);

		array = flappy_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		flappy_array_get_cstring_nocopy(array, 0, &first);
		if (strcmp(pkgname, first) == 0) {
			/* this pkg is the current alternative for this group */
			current = true;
			rv = remove_symlinks(xhp,
				flappy_dictionary_get(pkg_alternatives, keyname),
				keyname);
			if (rv != 0)
				break;
		}

		if (!update) {
			flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_REMOVED, 0, NULL,
			    "%s: unregistered '%s' alternatives group", pkgver, keyname);
			flappy_remove_string_from_array(array, pkgname);
			flappy_array_get_cstring_nocopy(array, 0, &first);
		}

		if (flappy_array_count(array) == 0) {
			flappy_dictionary_remove(alternatives, keyname);
			continue;
		}

		if (update || !current)
			continue;

		/* get the new alternative group package */
		if (switch_alt_group(xhp, keyname, first, &pkg_alternatives) != 0)
			break;
	}
	flappy_object_release(allkeys);

	return rv;
}

/*
 * Prune the alternatives group from the db. This will first unregister
 * it for the package and if there's no other package left providing the
 * same, also ditch the whole group. When this is called, it is guaranteed
 * that what is happening is an upgrade, because it's only invoked when
 * the repo and installed alternatives sets differ for a specific package.
 */
static void
prune_altgroup(struct flappy_handle *xhp, flappy_dictionary_t repod,
		const char *pkgname, const char *pkgver, const char *keyname)
{
	const char *newpkg = NULL, *curpkg = NULL;
	flappy_array_t array;
	flappy_dictionary_t alternatives;
	flappy_string_t kstr;
	unsigned int grp_count;
	bool current = false;

	flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_REMOVED, 0, NULL,
		"%s: unregistered '%s' alternatives group", pkgver, keyname);

	alternatives = flappy_dictionary_get(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_");
	assert(alternatives);
	array = flappy_dictionary_get(alternatives, keyname);

	/* if using alt group from another package, we won't switch anything */
	flappy_array_get_cstring_nocopy(array, 0, &curpkg);
	current = (strcmp(pkgname, curpkg) == 0);

	/* actually prune the alt group for the current package */
	flappy_remove_string_from_array(array, pkgname);
	grp_count = flappy_array_count(array);
	if (grp_count == 0) {
		/* it was the last one, ditch the whole thing */
		flappy_dictionary_remove(alternatives, keyname);
		return;
	}
	if (!current) {
		/* not the last one, and ours wasn't the one being used */
		return;
	}

	if (flappy_array_count(flappy_dictionary_get(repod, "run_depends")) == 0 &&
	    flappy_array_count(flappy_dictionary_get(repod, "shlib-requires")) == 0) {
		/*
		 * Empty dependencies indicate a removed package (pure meta),
		 * use the first available group after ours has been pruned
		 */
		flappy_array_get_cstring_nocopy(array, 0, &newpkg);
		switch_alt_group(xhp, keyname, newpkg, NULL);
		return;
	}

	/*
	 * Use the last group, as this indicates that a transitional metapackage
	 * is replacing the original and therefore a new package has registered
	 * a replacement group, which should be last in the array (most recent).
	 */
	flappy_array_get_cstring_nocopy(array, grp_count - 1, &newpkg);

	/* put the new package as head */
	kstr = flappy_string_create_cstring(newpkg);
	flappy_remove_string_from_array(array, newpkg);
	flappy_array_add_first(array, kstr);
	flappy_array_get_cstring_nocopy(array, 0, &newpkg);
	flappy_object_release(kstr);

	switch_alt_group(xhp, keyname, newpkg, NULL);
}


static void
remove_obsoletes(struct flappy_handle *xhp, const char *pkgname, const char *pkgver,
		flappy_dictionary_t pkgdb_alts, flappy_dictionary_t repod)
{
	flappy_array_t allkeys;
	flappy_dictionary_t pkgd, pkgd_alts, repod_alts;

	pkgd = flappy_pkgdb_get_pkg(xhp, pkgname);
	if (flappy_object_type(pkgd) != FLAPPY_TYPE_DICTIONARY) {
		return;
	}

	pkgd_alts = flappy_dictionary_get(pkgd, "alternatives");
	repod_alts = flappy_dictionary_get(repod, "alternatives");

	if (flappy_object_type(pkgd_alts) != FLAPPY_TYPE_DICTIONARY) {
		return;
	}

	allkeys = flappy_dictionary_all_keys(pkgd_alts);
	for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
		flappy_array_t array, array2, array_repo;
		flappy_object_t keysym;
		const char *keyname, *first = NULL;

		keysym = flappy_array_get(allkeys, i);
		array = flappy_dictionary_get_keysym(pkgd_alts, keysym);
		keyname = flappy_dictionary_keysym_cstring_nocopy(keysym);

		array_repo = flappy_dictionary_get(repod_alts, keyname);
		if (!flappy_array_equals(array, array_repo)) {
			/*
			 * Check if current provider in pkgdb is this pkg.
			 */
			array2 = flappy_dictionary_get(pkgdb_alts, keyname);
			if (array2) {
				flappy_array_get_cstring_nocopy(array2, 0, &first);
				if (strcmp(pkgname, first) == 0) {
					remove_symlinks(xhp, array_repo, keyname);
				}
			}
		}
		/*
		 * There is nothing left in the alternatives group, which means
		 * the package is being upgraded and is removing it; if we don't
		 * prune it, the system will keep it set after removal of its
		 * parent package, but it will be empty and invalid...
		 */
		if (flappy_array_count(array_repo) == 0) {
			prune_altgroup(xhp, repod, pkgname, pkgver, keyname);
		}
	}
	flappy_object_release(allkeys);
}

int
flappy_alternatives_register(struct flappy_handle *xhp, flappy_dictionary_t pkg_repod)
{
	flappy_array_t allkeys;
	flappy_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver, *pkgname;
	int rv = 0;

	assert(xhp);

	if (xhp->pkgdb == NULL)
		return EINVAL;

	alternatives = flappy_dictionary_get(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_");
	if (alternatives == NULL) {
		alternatives = flappy_dictionary_create();
		flappy_dictionary_set(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_", alternatives);
		flappy_object_release(alternatives);
	}
	alternatives = flappy_dictionary_get(xhp->pkgdb, "_FLAPPY_ALTERNATIVES_");
	assert(alternatives);

	flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	flappy_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);

	/*
	 * Compare alternatives from pkgdb and repo and then remove obsolete
	 * symlinks, also remove obsolete (empty) alternatives groups.
	 */
	remove_obsoletes(xhp, pkgname, pkgver, alternatives, pkg_repod);

	pkg_alternatives = flappy_dictionary_get(pkg_repod, "alternatives");
	if (!flappy_dictionary_count(pkg_alternatives))
		return 0;

	allkeys = flappy_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < flappy_array_count(allkeys); i++) {
		flappy_array_t array;
		flappy_object_t keysym;
		const char *keyname, *first = NULL;

		keysym = flappy_array_get(allkeys, i);
		keyname = flappy_dictionary_keysym_cstring_nocopy(keysym);

		array = flappy_dictionary_get(alternatives, keyname);
		if (array == NULL) {
			array = flappy_array_create();
		} else {
			if (flappy_match_string_in_array(array, pkgname)) {
				flappy_array_get_cstring_nocopy(array, 0, &first);
				if (strcmp(pkgname, first)) {
					/* current alternative does not match */
					continue;
				}
				/* already registered, update symlinks */
				rv = create_symlinks(xhp,
					flappy_dictionary_get(pkg_alternatives, keyname),
					keyname);
				if (rv != 0)
					break;
			} else {
				/* not registered, add provider */
				flappy_array_add_cstring(array, pkgname);
				flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_ADDED, 0, NULL,
				    "%s: registered '%s' alternatives group", pkgver, keyname);
			}
			continue;
		}

		flappy_array_add_cstring(array, pkgname);
		flappy_dictionary_set(alternatives, keyname, array);
		flappy_set_cb_state(xhp, FLAPPY_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: registered '%s' alternatives group", pkgver, keyname);
		/* apply alternatives for this group */
		rv = create_symlinks(xhp,
			flappy_dictionary_get(pkg_alternatives, keyname),
			keyname);
		flappy_object_release(array);
		if (rv != 0)
			break;
	}
	flappy_object_release(allkeys);

	return rv;
}

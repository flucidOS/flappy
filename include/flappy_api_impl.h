/*-
 * Copyright (c) 2010-2015 Juan Romero Pardines.
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
 *-
 */

#ifndef _FLAPPY_API_IMPL_H_
#define _FLAPPY_API_IMPL_H_

#include <assert.h>
#include "flappy.h"

/*
 * By default all public functions have default visibility, unless
 * visibility has been detected by configure and the HIDDEN definition
 * is used.
 */
#if HAVE_VISIBILITY
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif

#include "queue.h"
#include "compat.h"

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(uintptr_t)(const void *)(a))
#endif

#ifndef __arraycount
#define __arraycount(x) (sizeof(x) / sizeof(*x))
#endif

struct archive;
struct archive_entry;

/**
 * @private
 */
int HIDDEN dewey_match(const char *, const char *);
int HIDDEN flappy_pkgdb_init(struct flappy_handle *);
void HIDDEN flappy_pkgdb_release(struct flappy_handle *);
int HIDDEN flappy_pkgdb_conversion(struct flappy_handle *);
int HIDDEN flappy_array_replace_dict_by_name(flappy_array_t, flappy_dictionary_t,
		const char *);
int HIDDEN flappy_array_replace_dict_by_pattern(flappy_array_t, flappy_dictionary_t,
		const char *);
bool HIDDEN flappy_remove_pkg_from_array_by_name(flappy_array_t, const char *);
bool HIDDEN flappy_remove_pkg_from_array_by_pattern(flappy_array_t, const char *);
bool HIDDEN flappy_remove_pkg_from_array_by_pkgver(flappy_array_t, const char *);
void HIDDEN flappy_fetch_set_cache_connection(int, int);
void HIDDEN flappy_fetch_unset_cache_connection(void);
int HIDDEN flappy_entry_is_a_conf_file(flappy_dictionary_t, const char *);
int HIDDEN flappy_entry_install_conf_file(struct flappy_handle *, flappy_dictionary_t,
		flappy_dictionary_t, struct archive_entry *, const char *,
		const char *, bool);
flappy_dictionary_t HIDDEN flappy_find_virtualpkg_in_conf(struct flappy_handle *,
		flappy_dictionary_t, const char *);
flappy_dictionary_t HIDDEN flappy_find_pkg_in_dict(flappy_dictionary_t, const char *);
flappy_dictionary_t HIDDEN flappy_find_virtualpkg_in_dict(struct flappy_handle *,
		flappy_dictionary_t, const char *);
flappy_dictionary_t HIDDEN flappy_find_pkg_in_array(flappy_array_t, const char *,
		flappy_trans_type_t);
flappy_dictionary_t HIDDEN flappy_find_virtualpkg_in_array(struct flappy_handle *,
		flappy_array_t, const char *, flappy_trans_type_t);

/* transaction */
bool HIDDEN flappy_transaction_check_revdeps(struct flappy_handle *, flappy_array_t);
bool HIDDEN flappy_transaction_check_shlibs(struct flappy_handle *, flappy_array_t);
bool HIDDEN flappy_transaction_check_replaces(struct flappy_handle *, flappy_array_t);
int HIDDEN flappy_transaction_check_conflicts(struct flappy_handle *, flappy_array_t);
bool HIDDEN flappy_transaction_store(struct flappy_handle *, flappy_array_t, flappy_dictionary_t, bool);
int HIDDEN flappy_transaction_init(struct flappy_handle *);
int HIDDEN flappy_transaction_files(struct flappy_handle *,
		flappy_object_iterator_t);
int HIDDEN flappy_transaction_fetch(struct flappy_handle *,
		flappy_object_iterator_t);
int HIDDEN flappy_transaction_pkg_deps(struct flappy_handle *, flappy_array_t, flappy_dictionary_t);
int HIDDEN flappy_transaction_internalize(struct flappy_handle *, flappy_object_iterator_t);

char HIDDEN *flappy_get_remote_repo_string(const char *);
int HIDDEN flappy_repo_sync(struct flappy_handle *, const char *);
int HIDDEN flappy_file_hash_check_dictionary(struct flappy_handle *,
		flappy_dictionary_t, const char *, const char *);
int HIDDEN flappy_file_exec(struct flappy_handle *, const char *, ...);
void HIDDEN flappy_set_cb_fetch(struct flappy_handle *, off_t, off_t, off_t,
		const char *, bool, bool, bool);
int HIDDEN flappy_set_cb_state(struct flappy_handle *, flappy_state_t, int,
		const char *, const char *, ...);
int HIDDEN flappy_unpack_binary_pkg(struct flappy_handle *, flappy_dictionary_t);
int HIDDEN flappy_remove_pkg(struct flappy_handle *, const char *, bool);
int HIDDEN flappy_register_pkg(struct flappy_handle *, flappy_dictionary_t);

char HIDDEN *flappy_archive_get_file(struct archive *, struct archive_entry *);
flappy_dictionary_t HIDDEN flappy_archive_get_dictionary(struct archive *,
		struct archive_entry *);
const char HIDDEN *vpkg_user_conf(struct flappy_handle *, const char *);

struct archive HIDDEN *flappy_archive_read_new(void);
int HIDDEN flappy_archive_read_open(struct archive *ar, const char *path);
int HIDDEN flappy_archive_read_open_remote(struct archive *ar, const char *url);
int HIDDEN flappy_archive_errno(struct archive *ar);

flappy_array_t HIDDEN flappy_get_pkg_fulldeptree(struct flappy_handle *,
		const char *, bool);
struct flappy_repo HIDDEN *flappy_regget_repo(struct flappy_handle *,
		const char *);
int HIDDEN flappy_conf_init(struct flappy_handle *);

#endif /* !_FLAPPY_API_IMPL_H_ */

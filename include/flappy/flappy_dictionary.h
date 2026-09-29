/*	$NetBSD: prop_dictionary.h,v 1.9 2008/04/28 20:22:51 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FLAPPY_DICTIONARY_H_
#define	_FLAPPY_DICTIONARY_H_

#include <stdint.h>
#include <flappy/flappy_object.h>
#include <flappy/flappy_array.h>

typedef struct _prop_dictionary *flappy_dictionary_t;
typedef struct _prop_dictionary_keysym *flappy_dictionary_keysym_t;

#ifdef __cplusplus
extern "C" {
#endif

flappy_dictionary_t flappy_dictionary_create(void);
flappy_dictionary_t flappy_dictionary_create_with_capacity(unsigned int);

flappy_dictionary_t flappy_dictionary_copy(flappy_dictionary_t);
flappy_dictionary_t flappy_dictionary_copy_mutable(flappy_dictionary_t);

unsigned int	flappy_dictionary_count(flappy_dictionary_t);
bool		flappy_dictionary_ensure_capacity(flappy_dictionary_t,
						unsigned int);

void		flappy_dictionary_make_immutable(flappy_dictionary_t);

flappy_object_iterator_t flappy_dictionary_iterator(flappy_dictionary_t);
flappy_array_t	flappy_dictionary_all_keys(flappy_dictionary_t);

flappy_object_t	flappy_dictionary_get(flappy_dictionary_t, const char *);
bool		flappy_dictionary_set(flappy_dictionary_t, const char *,
				    flappy_object_t);
void		flappy_dictionary_remove(flappy_dictionary_t, const char *);

flappy_object_t	flappy_dictionary_get_keysym(flappy_dictionary_t,
					   flappy_dictionary_keysym_t);
bool		flappy_dictionary_set_keysym(flappy_dictionary_t,
					   flappy_dictionary_keysym_t,
					   flappy_object_t);
void		flappy_dictionary_remove_keysym(flappy_dictionary_t,
					      flappy_dictionary_keysym_t);

bool		flappy_dictionary_equals(flappy_dictionary_t, flappy_dictionary_t);

char *		flappy_dictionary_externalize(flappy_dictionary_t);
flappy_dictionary_t flappy_dictionary_internalize(const char *);

bool		flappy_dictionary_externalize_to_file(flappy_dictionary_t,
						    const char *);
bool		flappy_dictionary_externalize_to_zfile(flappy_dictionary_t,
						     const char *);
flappy_dictionary_t flappy_dictionary_internalize_from_file(const char *);
flappy_dictionary_t flappy_dictionary_internalize_from_zfile(const char *);

const char *	flappy_dictionary_keysym_cstring_nocopy(flappy_dictionary_keysym_t);

bool		flappy_dictionary_keysym_equals(flappy_dictionary_keysym_t,
					      flappy_dictionary_keysym_t);

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool		flappy_dictionary_get_dict(flappy_dictionary_t, const char *,
					 flappy_dictionary_t *);
bool		flappy_dictionary_get_bool(flappy_dictionary_t, const char *,
					 bool *);
bool		flappy_dictionary_set_bool(flappy_dictionary_t, const char *,
					 bool);

bool		flappy_dictionary_get_int8(flappy_dictionary_t, const char *,
					 int8_t *);
bool		flappy_dictionary_get_uint8(flappy_dictionary_t, const char *,
					  uint8_t *);
bool		flappy_dictionary_set_int8(flappy_dictionary_t, const char *,
					 int8_t);
bool		flappy_dictionary_set_uint8(flappy_dictionary_t, const char *,
					  uint8_t);

bool		flappy_dictionary_get_int16(flappy_dictionary_t, const char *,
					  int16_t *);
bool		flappy_dictionary_get_uint16(flappy_dictionary_t, const char *,
					   uint16_t *);
bool		flappy_dictionary_set_int16(flappy_dictionary_t, const char *,
					  int16_t);
bool		flappy_dictionary_set_uint16(flappy_dictionary_t, const char *,
					   uint16_t);

bool		flappy_dictionary_get_int32(flappy_dictionary_t, const char *,
					  int32_t *);
bool		flappy_dictionary_get_uint32(flappy_dictionary_t, const char *,
					   uint32_t *);
bool		flappy_dictionary_set_int32(flappy_dictionary_t, const char *,
					  int32_t);
bool		flappy_dictionary_set_uint32(flappy_dictionary_t, const char *,
					   uint32_t);

bool		flappy_dictionary_get_int64(flappy_dictionary_t, const char *,
					  int64_t *);
bool		flappy_dictionary_get_uint64(flappy_dictionary_t, const char *,
					   uint64_t *);
bool		flappy_dictionary_set_int64(flappy_dictionary_t, const char *,
					  int64_t);
bool		flappy_dictionary_set_uint64(flappy_dictionary_t, const char *,
					   uint64_t);

bool		flappy_dictionary_get_cstring(flappy_dictionary_t, const char *,
					     char **);
bool		flappy_dictionary_set_cstring(flappy_dictionary_t, const char *,
					    const char *);

bool		flappy_dictionary_get_cstring_nocopy(flappy_dictionary_t,
						   const char *,
						   const char **);
bool		flappy_dictionary_set_cstring_nocopy(flappy_dictionary_t,
						   const char *,
						   const char *);
bool		flappy_dictionary_set_and_rel(flappy_dictionary_t,
					    const char *,
					    flappy_object_t);

#ifdef __cplusplus
}
#endif

#endif /* _FLAPPY_DICTIONARY_H_ */

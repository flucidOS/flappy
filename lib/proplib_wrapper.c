/*-
 * Copyright (c) 2013-2015 Juan Romero Pardines.
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
#include <prop/proplib.h>

/* prop_array */

flappy_array_t
flappy_array_create(void)
{
	return prop_array_create();
}

flappy_array_t
flappy_array_create_with_capacity(unsigned int capacity)
{
	return prop_array_create_with_capacity(capacity);
}

flappy_array_t
flappy_array_copy(flappy_array_t a)
{
	return prop_array_copy(a);
}

flappy_array_t
flappy_array_copy_mutable(flappy_array_t a)
{
	return prop_array_copy_mutable(a);
}

unsigned int
flappy_array_capacity(flappy_array_t a)
{
	return prop_array_capacity(a);
}

unsigned int
flappy_array_count(flappy_array_t a)
{
	return prop_array_count(a);
}

bool
flappy_array_ensure_capacity(flappy_array_t a, unsigned int i)
{
	return prop_array_ensure_capacity(a, i);
}

void
flappy_array_make_immutable(flappy_array_t a)
{
	prop_array_make_immutable(a);
}

bool
flappy_array_mutable(flappy_array_t a)
{
	return prop_array_mutable(a);
}

flappy_object_iterator_t
flappy_array_iterator(flappy_array_t a)
{
	return prop_array_iterator(a);
}

flappy_object_t
flappy_array_get(flappy_array_t a, unsigned int i)
{
	return prop_array_get(a, i);
}

bool
flappy_array_set(flappy_array_t a, unsigned int i, flappy_object_t obj)
{
	return prop_array_set(a, i, obj);
}

bool
flappy_array_add(flappy_array_t a, flappy_object_t obj)
{
	return prop_array_add(a, obj);
}

bool
flappy_array_add_first(flappy_array_t a, flappy_object_t obj)
{
	return prop_array_add_first(a, obj);
}

void
flappy_array_remove(flappy_array_t a, unsigned int i)
{
	prop_array_remove(a, i);
}

bool
flappy_array_equals(flappy_array_t a, flappy_array_t b)
{
	return prop_array_equals(a, b);
}

char *
flappy_array_externalize(flappy_array_t a)
{
	return prop_array_externalize(a);
}

flappy_array_t
flappy_array_internalize(const char *s)
{
	return prop_array_internalize(s);
}

bool
flappy_array_externalize_to_file(flappy_array_t a, const char *s)
{
	return prop_array_externalize_to_file(a, s);
}

bool
flappy_array_externalize_to_zfile(flappy_array_t a, const char *s)
{
	return prop_array_externalize_to_zfile(a, s);
}

flappy_array_t
flappy_array_internalize_from_file(const char *s)
{
	return prop_array_internalize_from_file(s);
}

flappy_array_t
flappy_array_internalize_from_zfile(const char *s)
{
	return prop_array_internalize_from_zfile(s);
}

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool
flappy_array_get_bool(flappy_array_t a, unsigned int i, bool *b)
{
	return prop_array_get_bool(a, i, b);
}

bool
flappy_array_set_bool(flappy_array_t a, unsigned int i, bool b)
{
	return prop_array_set_bool(a, i, b);
}

bool
flappy_array_get_int8(flappy_array_t a, unsigned int i, int8_t *v)
{
	return prop_array_get_int8(a, i, v);
}

bool
flappy_array_get_uint8(flappy_array_t a, unsigned int i, uint8_t *v)
{
	return prop_array_get_uint8(a, i, v);
}

bool
flappy_array_set_int8(flappy_array_t a, unsigned int i, int8_t v)
{
	return prop_array_set_int8(a, i, v);
}

bool
flappy_array_set_uint8(flappy_array_t a, unsigned int i, uint8_t v)
{
	return prop_array_set_uint8(a, i, v);
}

bool
flappy_array_get_int16(flappy_array_t a, unsigned int i, int16_t *v)
{
	return prop_array_get_int16(a, i, v);
}

bool
flappy_array_get_uint16(flappy_array_t a, unsigned int i, uint16_t *v)
{
	return prop_array_get_uint16(a, i, v);
}

bool
flappy_array_set_int16(flappy_array_t a, unsigned int i, int16_t v)
{
	return prop_array_set_int16(a, i, v);
}

bool
flappy_array_set_uint16(flappy_array_t a, unsigned int i, uint16_t v)
{
	return prop_array_set_uint16(a, i, v);
}

bool
flappy_array_get_int32(flappy_array_t a, unsigned int i, int32_t *v)
{
	return prop_array_get_int32(a, i, v);
}

bool
flappy_array_get_uint32(flappy_array_t a, unsigned int i, uint32_t *v)
{
	return prop_array_get_uint32(a, i, v);
}

bool
flappy_array_set_int32(flappy_array_t a, unsigned int i, int32_t v)
{
	return prop_array_set_int32(a, i, v);
}

bool
flappy_array_set_uint32(flappy_array_t a, unsigned int i, uint32_t v)
{
	return prop_array_set_uint32(a, i, v);
}

bool
flappy_array_get_int64(flappy_array_t a, unsigned int i, int64_t *v)
{
	return prop_array_get_int64(a, i, v);
}

bool
flappy_array_get_uint64(flappy_array_t a, unsigned int i, uint64_t *v)
{
	return prop_array_get_uint64(a, i, v);
}

bool
flappy_array_set_int64(flappy_array_t a, unsigned int i, int64_t v)
{
	return prop_array_set_int64(a, i, v);
}

bool
flappy_array_set_uint64(flappy_array_t a, unsigned int i, uint64_t v)
{
	return prop_array_set_uint64(a, i, v);
}

bool
flappy_array_add_int8(flappy_array_t a, int8_t v)
{
	return prop_array_add_int8(a, v);
}

bool
flappy_array_add_uint8(flappy_array_t a, uint8_t v)
{
	return prop_array_add_uint8(a, v);
}

bool
flappy_array_add_int16(flappy_array_t a, int16_t v)
{
	return prop_array_add_int16(a, v);
}

bool
flappy_array_add_uint16(flappy_array_t a, uint16_t v)
{
	return prop_array_add_uint16(a, v);
}

bool
flappy_array_add_int32(flappy_array_t a, int32_t v)
{
	return prop_array_add_int32(a, v);
}

bool
flappy_array_add_uint32(flappy_array_t a, uint32_t v)
{
	return prop_array_add_uint32(a, v);
}

bool
flappy_array_add_int64(flappy_array_t a, int64_t v)
{
	return prop_array_add_int64(a, v);
}

bool
flappy_array_add_uint64(flappy_array_t a, uint64_t v)
{
	return prop_array_add_uint64(a, v);
}

bool
flappy_array_get_cstring(flappy_array_t a, unsigned int i, char **s)
{
	return prop_array_get_cstring(a, i, s);
}

bool
flappy_array_set_cstring(flappy_array_t a, unsigned int i, const char *s)
{
	return prop_array_set_cstring(a, i, s);
}

bool
flappy_array_add_cstring(flappy_array_t a, const char *s)
{
	return prop_array_add_cstring(a, s);
}

bool
flappy_array_add_cstring_nocopy(flappy_array_t a, const char *s)
{
	return prop_array_add_cstring_nocopy(a, s);
}

bool
flappy_array_get_cstring_nocopy(flappy_array_t a, unsigned int i, const char **s)
{
	return prop_array_get_cstring_nocopy(a, i, s);
}

bool
flappy_array_set_cstring_nocopy(flappy_array_t a, unsigned int i, const char *s)
{
	return prop_array_set_cstring_nocopy(a, i, s);
}

bool
flappy_array_add_and_rel(flappy_array_t a, flappy_object_t o)
{
	return prop_array_add_and_rel(a, o);
}

/* prop_bool */

flappy_bool_t
flappy_bool_create(bool v)
{
	return prop_bool_create(v);
}

flappy_bool_t
flappy_bool_copy(flappy_bool_t b)
{
	return prop_bool_copy(b);
}

bool
flappy_bool_true(flappy_bool_t b)
{
	return prop_bool_true(b);
}

bool
flappy_bool_equals(flappy_bool_t a, flappy_bool_t b)
{
	return prop_bool_equals(a, b);
}

/* prop_data */

flappy_data_t
flappy_data_create_data(const void *v, size_t s)
{
	return prop_data_create_data(v, s);
}

flappy_data_t
flappy_data_create_data_nocopy(const void *v, size_t s)
{
	return prop_data_create_data_nocopy(v, s);
}

flappy_data_t
flappy_data_copy(flappy_data_t d)
{
	return prop_data_copy(d);
}

size_t
flappy_data_size(flappy_data_t d)
{
	return prop_data_size(d);
}

void *
flappy_data_data(flappy_data_t d)
{
	return prop_data_data(d);
}

const void *
flappy_data_data_nocopy(flappy_data_t d)
{
	return prop_data_data_nocopy(d);
}

bool
flappy_data_equals(flappy_data_t a, flappy_data_t b)
{
	return prop_data_equals(a, b);
}

bool
flappy_data_equals_data(flappy_data_t d, const void *v, size_t s)
{
	return prop_data_equals_data(d, v, s);
}

/* prop_dictionary */

flappy_dictionary_t
flappy_dictionary_create(void)
{
	return prop_dictionary_create();
}

flappy_dictionary_t
flappy_dictionary_create_with_capacity(unsigned int i)
{
	return prop_dictionary_create_with_capacity(i);
}

flappy_dictionary_t
flappy_dictionary_copy(flappy_dictionary_t d)
{
	return prop_dictionary_copy(d);
}

flappy_dictionary_t
flappy_dictionary_copy_mutable(flappy_dictionary_t d)
{
	return prop_dictionary_copy_mutable(d);
}

unsigned int
flappy_dictionary_count(flappy_dictionary_t d)
{
	return prop_dictionary_count(d);
}

bool
flappy_dictionary_ensure_capacity(flappy_dictionary_t d, unsigned int i)
{
	return prop_dictionary_ensure_capacity(d, i);
}

void
flappy_dictionary_make_immutable(flappy_dictionary_t d)
{
	prop_dictionary_make_immutable(d);
}

flappy_object_iterator_t
flappy_dictionary_iterator(flappy_dictionary_t d)
{
	return prop_dictionary_iterator(d);
}

flappy_array_t
flappy_dictionary_all_keys(flappy_dictionary_t d)
{
	return prop_dictionary_all_keys(d);
}

flappy_object_t
flappy_dictionary_get(flappy_dictionary_t d, const char *s)
{
	return prop_dictionary_get(d, s);
}

bool
flappy_dictionary_set(flappy_dictionary_t d, const char *s, flappy_object_t o)
{
	return prop_dictionary_set(d, s, o);
}

void
flappy_dictionary_remove(flappy_dictionary_t d, const char *s)
{
	prop_dictionary_remove(d, s);
}

flappy_object_t
flappy_dictionary_get_keysym(flappy_dictionary_t d, flappy_dictionary_keysym_t k)
{
	return prop_dictionary_get_keysym(d, k);
}

bool
flappy_dictionary_set_keysym(flappy_dictionary_t d, flappy_dictionary_keysym_t k,
					   flappy_object_t o)
{
	return prop_dictionary_set_keysym(d, k, o);
}

void
flappy_dictionary_remove_keysym(flappy_dictionary_t d, flappy_dictionary_keysym_t k)
{
	prop_dictionary_remove_keysym(d, k);
}

bool
flappy_dictionary_equals(flappy_dictionary_t a, flappy_dictionary_t b)
{
	return prop_dictionary_equals(a, b);
}

char *
flappy_dictionary_externalize(flappy_dictionary_t d)
{
	return prop_dictionary_externalize(d);
}

flappy_dictionary_t
flappy_dictionary_internalize(const char *s)
{
	return prop_dictionary_internalize(s);
}

bool
flappy_dictionary_externalize_to_file(flappy_dictionary_t d, const char *s)
{
	return prop_dictionary_externalize_to_file(d, s);
}

bool
flappy_dictionary_externalize_to_zfile(flappy_dictionary_t d, const char *s)
{
	return prop_dictionary_externalize_to_zfile(d, s);
}

flappy_dictionary_t
flappy_dictionary_internalize_from_file(const char *s)
{
	return prop_dictionary_internalize_from_file(s);
}

flappy_dictionary_t
flappy_dictionary_internalize_from_zfile(const char *s)
{
	return prop_dictionary_internalize_from_zfile(s);
}

const char *
flappy_dictionary_keysym_cstring_nocopy(flappy_dictionary_keysym_t k)
{
	return prop_dictionary_keysym_cstring_nocopy(k);
}

bool
flappy_dictionary_keysym_equals(flappy_dictionary_keysym_t a, flappy_dictionary_keysym_t b)
{
	return prop_dictionary_keysym_equals(a, b);
}

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool
flappy_dictionary_get_dict(flappy_dictionary_t d, const char *s,
					 flappy_dictionary_t *rd)
{
	return prop_dictionary_get_dict(d, s, rd);
}

bool
flappy_dictionary_get_bool(flappy_dictionary_t d, const char *s, bool *b)
{
	return prop_dictionary_get_bool(d, s, b);
}

bool
flappy_dictionary_set_bool(flappy_dictionary_t d, const char *s, bool b)
{
	return prop_dictionary_set_bool(d, s, b);
}

bool
flappy_dictionary_get_int8(flappy_dictionary_t d, const char *s, int8_t *v)
{
	return prop_dictionary_get_int8(d, s, v);
}

bool
flappy_dictionary_get_uint8(flappy_dictionary_t d, const char *s, uint8_t *v)
{
	return prop_dictionary_get_uint8(d, s, v);
}

bool
flappy_dictionary_set_int8(flappy_dictionary_t d, const char *s, int8_t v)
{
	return prop_dictionary_set_int8(d, s, v);
}

bool
flappy_dictionary_set_uint8(flappy_dictionary_t d, const char *s, uint8_t v)
{
	return prop_dictionary_set_uint8(d, s, v);
}

bool
flappy_dictionary_get_int16(flappy_dictionary_t d, const char *s, int16_t *v)
{
	return prop_dictionary_get_int16(d, s, v);
}

bool
flappy_dictionary_get_uint16(flappy_dictionary_t d, const char *s, uint16_t *v)
{
	return prop_dictionary_get_uint16(d, s, v);
}

bool
flappy_dictionary_set_int16(flappy_dictionary_t d, const char *s, int16_t v)
{
	return prop_dictionary_set_int16(d, s, v);
}

bool
flappy_dictionary_set_uint16(flappy_dictionary_t d, const char *s, uint16_t v)
{
	return prop_dictionary_set_uint16(d, s, v);
}

bool
flappy_dictionary_get_int32(flappy_dictionary_t d, const char *s, int32_t *v)
{
	return prop_dictionary_get_int32(d, s, v);
}

bool
flappy_dictionary_get_uint32(flappy_dictionary_t d, const char *s, uint32_t *v)
{
	return prop_dictionary_get_uint32(d, s, v);
}

bool
flappy_dictionary_set_int32(flappy_dictionary_t d, const char *s, int32_t v)
{
	return prop_dictionary_set_int32(d, s, v);
}

bool
flappy_dictionary_set_uint32(flappy_dictionary_t d, const char *s, uint32_t v)
{
	return prop_dictionary_set_uint32(d, s, v);
}

bool
flappy_dictionary_get_int64(flappy_dictionary_t d, const char *s, int64_t *v)
{
	return prop_dictionary_get_int64(d, s, v);
}

bool
flappy_dictionary_get_uint64(flappy_dictionary_t d, const char *s, uint64_t *v)
{
	return prop_dictionary_get_uint64(d, s, v);
}

bool
flappy_dictionary_set_int64(flappy_dictionary_t d, const char *s, int64_t v)
{
	return prop_dictionary_set_int64(d, s, v);
}

bool
flappy_dictionary_set_uint64(flappy_dictionary_t d, const char *s, uint64_t v)
{
	return prop_dictionary_set_uint64(d, s, v);
}

bool
flappy_dictionary_get_cstring(flappy_dictionary_t d, const char *s, char **ss)
{
	return prop_dictionary_get_cstring(d, s, ss);
}

bool
flappy_dictionary_set_cstring(flappy_dictionary_t d, const char *s, const char *ss)
{
	return prop_dictionary_set_cstring(d, s, ss);
}

bool
flappy_dictionary_get_cstring_nocopy(flappy_dictionary_t d, const char *s, const char **ss)
{
	return prop_dictionary_get_cstring_nocopy(d, s, ss);
}

bool
flappy_dictionary_set_cstring_nocopy(flappy_dictionary_t d, const char *s, const char *ss)
{
	return prop_dictionary_set_cstring_nocopy(d, s, ss);
}

bool
flappy_dictionary_set_and_rel(flappy_dictionary_t d, const char *s, flappy_object_t o)
{
	return prop_dictionary_set_and_rel(d, s, o);
}

/* prop_number */

flappy_number_t
flappy_number_create_integer(int64_t v)
{
	return prop_number_create_integer(v);
}

flappy_number_t
flappy_number_create_unsigned_integer(uint64_t v)
{
	return prop_number_create_unsigned_integer(v);
}

flappy_number_t
flappy_number_copy(flappy_number_t n)
{
	return prop_number_copy(n);
}

int
flappy_number_size(flappy_number_t n)
{
	return prop_number_size(n);
}

bool
flappy_number_unsigned(flappy_number_t n)
{
	return prop_number_unsigned(n);
}

int64_t
flappy_number_integer_value(flappy_number_t n)
{
	return prop_number_integer_value(n);
}

uint64_t
flappy_number_unsigned_integer_value(flappy_number_t n)
{
	return prop_number_unsigned_integer_value(n);
}

bool
flappy_number_equals(flappy_number_t n, flappy_number_t nn)
{
	return prop_number_equals(n, nn);
}

bool
flappy_number_equals_integer(flappy_number_t n, int64_t v)
{
	return prop_number_equals_integer(n, v);
}

bool
flappy_number_equals_unsigned_integer(flappy_number_t n, uint64_t v)
{
	return prop_number_equals_unsigned_integer(n, v);
}

/* prop_object */

void
flappy_object_retain(flappy_object_t o)
{
	prop_object_retain(o);
}

void
flappy_object_release(flappy_object_t o)
{
	prop_object_release(o);
}

flappy_type_t
flappy_object_type(flappy_object_t o)
{
	return (flappy_type_t)prop_object_type(o);
}

bool
flappy_object_equals(flappy_object_t o, flappy_object_t oo)
{
	return prop_object_equals(o, oo);
}

bool
flappy_object_equals_with_error(flappy_object_t o, flappy_object_t oo, bool *b)
{
	return prop_object_equals_with_error(o, oo, b);
}

flappy_object_t
flappy_object_iterator_next(flappy_object_iterator_t o)
{
	return prop_object_iterator_next(o);
}

void
flappy_object_iterator_reset(flappy_object_iterator_t o)
{
	prop_object_iterator_reset(o);
}

void
flappy_object_iterator_release(flappy_object_iterator_t o)
{
	prop_object_iterator_release(o);
}

/* prop_string */

flappy_string_t
flappy_string_create(void)
{
	return prop_string_create();
}

flappy_string_t
flappy_string_create_cstring(const char *s)
{
	return prop_string_create_cstring(s);
}

flappy_string_t
flappy_string_create_cstring_nocopy(const char *s)
{
	return prop_string_create_cstring_nocopy(s);
}

flappy_string_t
flappy_string_copy(flappy_string_t s)
{
	return prop_string_copy(s);
}

flappy_string_t
flappy_string_copy_mutable(flappy_string_t s)
{
	return prop_string_copy_mutable(s);
}

size_t
flappy_string_size(flappy_string_t s)
{
	return prop_string_size(s);
}

bool
flappy_string_mutable(flappy_string_t s)
{
	return prop_string_mutable(s);
}

char *
flappy_string_cstring(flappy_string_t s)
{
	return prop_string_cstring(s);
}

const char *
flappy_string_cstring_nocopy(flappy_string_t s)
{
	return prop_string_cstring_nocopy(s);
}

bool
flappy_string_append(flappy_string_t s, flappy_string_t ss)
{
	return prop_string_append(s, ss);
}

bool
flappy_string_append_cstring(flappy_string_t s, const char *ss)
{
	return prop_string_append_cstring(s, ss);
}

bool
flappy_string_equals(flappy_string_t s, flappy_string_t ss)
{
	return prop_string_equals(s, ss);
}

bool
flappy_string_equals_cstring(flappy_string_t s, const char *ss)
{
	return prop_string_equals_cstring(s, ss);
}

/* flappy specific helpers */
flappy_array_t
flappy_plist_array_from_file(const char *path)
{
	flappy_array_t a;

	a = flappy_array_internalize_from_zfile(path);
	if (flappy_object_type(a) != FLAPPY_TYPE_ARRAY) {
		flappy_dbg_printf(
		    "flappy: failed to internalize array from %s\n", path);
	}
	return a;
}

flappy_dictionary_t
flappy_plist_dictionary_from_file(const char *path)
{
	flappy_dictionary_t d;

	d = flappy_dictionary_internalize_from_zfile(path);
	if (flappy_object_type(d) != FLAPPY_TYPE_DICTIONARY) {
		flappy_dbg_printf(
		    "flappy: failed to internalize dict from %s\n", path);
	}
	return d;
}

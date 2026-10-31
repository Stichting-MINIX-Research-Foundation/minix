/*	$NetBSD: prop_dictionary_util.c,v 1.9 2022/08/03 21:13:46 riastradh Exp $	*/

/*-
 * Copyright (c) 2006, 2020 The NetBSD Foundation, Inc.
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

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 *
 * Note: There is no special magic going on here.  We use the standard
 * proplib(3) APIs to do all of this work.  Any application could do
 * exactly what we're doing here.
 */

#include "prop_object_impl.h"	/* only to hide kernel vs. not-kernel */
#include <prop/proplib.h>

bool
prop_dictionary_get_dict(prop_dictionary_t dict, const char *key,
			 prop_dictionary_t *dp)
{
	prop_object_t o;

	o = prop_dictionary_get(dict, key);
	if (prop_object_type(o) != PROP_TYPE_DICTIONARY)
		return false;
	*dp = o;
	return true;

}

bool
prop_dictionary_get_bool(prop_dictionary_t dict, const char *key, bool *valp)
{
	prop_bool_t b;

	b = prop_dictionary_get(dict, key);
	if (prop_object_type(b) != PROP_TYPE_BOOL)
		return (false);

	*valp = prop_bool_true(b);

	return (true);
}

bool
prop_dictionary_set_bool(prop_dictionary_t dict, const char *key, bool val)
{

	return prop_dictionary_set_and_rel(dict, key, prop_bool_create(val));
}

#define	TEMPLATE(name, typ)						\
bool									\
prop_dictionary_get_ ## name (prop_dictionary_t dict,			\
			      const char *key,				\
			      typ *valp)				\
{									\
	return prop_number_ ## name ## _value(				\
	    prop_dictionary_get(dict, key), valp);			\
}
TEMPLATE(schar,    signed char)
TEMPLATE(short,    short)
TEMPLATE(int,      int)
TEMPLATE(long,     long)
TEMPLATE(longlong, long long)
TEMPLATE(intptr,   intptr_t)
TEMPLATE(int8,     int8_t)
TEMPLATE(int16,    int16_t)
TEMPLATE(int32,    int32_t)
TEMPLATE(int64,    int64_t)

TEMPLATE(uchar,     unsigned char)
TEMPLATE(ushort,    unsigned short)
TEMPLATE(uint,      unsigned int)
TEMPLATE(ulong,     unsigned long)
TEMPLATE(ulonglong, unsigned long long)
TEMPLATE(uintptr,   uintptr_t)
TEMPLATE(uint8,     uint8_t)
TEMPLATE(uint16,    uint16_t)
TEMPLATE(uint32,    uint32_t)
TEMPLATE(uint64,    uint64_t)

#undef TEMPLATE

static bool
prop_dictionary_set_signed_number(prop_dictionary_t dict, const char *key,
				  intmax_t val)
{
	return prop_dictionary_set_and_rel(dict, key,
					   prop_number_create_signed(val));
}

static bool
prop_dictionary_set_unsigned_number(prop_dictionary_t dict, const char *key,
				    uintmax_t val)
{
	/*LINTED: for conversion from 'long long' to 'long'*/		\
	return prop_dictionary_set_and_rel(dict, key,
					   prop_number_create_unsigned(val));
}

#define	TEMPLATE(name, which, typ)					\
bool									\
prop_dictionary_set_ ## name (prop_dictionary_t dict,			\
			      const char *key,				\
			      typ val)					\
{									\
	/*LINTED: for conversion from long long to 'long'*/		\
	return prop_dictionary_set_ ## which ## _number(dict, key, val);\
}

#define	STEMPLATE(name, typ)	TEMPLATE(name, signed, typ)
#define	UTEMPLATE(name, typ)	TEMPLATE(name, unsigned, typ)

STEMPLATE(schar,    signed char)
STEMPLATE(short,    short)
STEMPLATE(int,      int)
STEMPLATE(long,     long)
STEMPLATE(longlong, long long)
STEMPLATE(intptr,   intptr_t)
STEMPLATE(int8,     int8_t)
STEMPLATE(int16,    int16_t)
STEMPLATE(int32,    int32_t)
STEMPLATE(int64,    int64_t)

UTEMPLATE(uchar,     unsigned char)
UTEMPLATE(ushort,    unsigned short)
UTEMPLATE(uint,      unsigned int)
UTEMPLATE(ulong,     unsigned long)
UTEMPLATE(ulonglong, unsigned long long)
UTEMPLATE(uintptr,   uintptr_t)
UTEMPLATE(uint8,     uint8_t)
UTEMPLATE(uint16,    uint16_t)
UTEMPLATE(uint32,    uint32_t)
UTEMPLATE(uint64,    uint64_t)

#undef STEMPLATE
#undef UTEMPLATE
#undef TEMPLATE

bool
prop_dictionary_get_string(prop_dictionary_t dict, const char *key,
			   const char **cpp)
{
	prop_string_t str;
	const char *cp;

	str = prop_dictionary_get(dict, key);
	if (prop_object_type(str) != PROP_TYPE_STRING)
		return (false);

	cp = prop_string_value(str);
	if (cp == NULL)
		return (false);

	*cpp = cp;
	return (true);
}

bool
prop_dictionary_set_string(prop_dictionary_t dict, const char *key,
			   const char *cp)
{
	return prop_dictionary_set_and_rel(dict, key,
					   prop_string_create_copy(cp));
}

bool
prop_dictionary_set_string_nocopy(prop_dictionary_t dict,
				  const char *key,
				  const char *cp)
{
	return prop_dictionary_set_and_rel(dict, key,
					   prop_string_create_nocopy(cp));
}

bool
prop_dictionary_get_data(prop_dictionary_t dict, const char *key,
			 const void **vp, size_t *sizep)
{
	prop_data_t data;
	const void *v;

	data = prop_dictionary_get(dict, key);
	if (prop_object_type(data) != PROP_TYPE_DATA)
		return (false);

	v = prop_data_value(data);
	if (v == NULL)
		return (false);

	*vp = v;
	if (sizep != NULL)
		*sizep = prop_data_size(data);
	return (true);
}

bool
prop_dictionary_set_data(prop_dictionary_t dict, const char *key,
			 const void *v, size_t size)
{
	return prop_dictionary_set_and_rel(dict, key,
					   prop_data_create_copy(v, size));
}

bool
prop_dictionary_set_data_nocopy(prop_dictionary_t dict, const char *key,
			        const void *v, size_t size)
{
	return prop_dictionary_set_and_rel(dict, key,
					   prop_data_create_nocopy(v, size));
}

_PROP_DEPRECATED(prop_dictionary_get_cstring,
    "this program uses prop_dictionary_get_cstring(), "
    "which is deprecated; use prop_dictionary_get_string() and copy instead.")
bool
prop_dictionary_get_cstring(prop_dictionary_t dict,
			    const char *key,
			    char **cpp)
{
	prop_string_t str;
	char *cp;
	size_t len;
	bool rv;

	str = prop_dictionary_get(dict, key);
	if (prop_object_type(str) != PROP_TYPE_STRING)
		return (false);

	len = prop_string_size(str);
	cp = _PROP_MALLOC(len + 1, M_TEMP);
	if (cp == NULL)
		return (false);

	rv = prop_string_copy_value(str, cp, len + 1);
	if (rv)
		*cpp = cp;
	else
		_PROP_FREE(cp, M_TEMP);

	return (rv);
}

_PROP_DEPRECATED(prop_string_get_cstring_nocopy,
    "this program uses prop_string_get_cstring_nocopy(), "
    "which is deprecated; use prop_dictionary_get_string() instead.")
bool
prop_dictionary_get_cstring_nocopy(prop_dictionary_t dict,
				   const char *key,
				   const char **cpp)
{
	return prop_dictionary_get_string(dict, key, cpp);
}

_PROP_DEPRECATED(prop_dictionary_set_cstring,
    "this program uses prop_dictionary_set_cstring(), "
    "which is deprecated; use prop_dictionary_set_string() instead.")
bool
prop_dictionary_set_cstring(prop_dictionary_t dict,
			    const char *key,
			    const char *cp)
{
	return prop_dictionary_set_string(dict, key, cp);
}

_PROP_DEPRECATED(prop_dictionary_set_cstring_nocopy,
    "this program uses prop_dictionary_set_cstring_nocopy(), "
    "which is deprecated; use prop_dictionary_set_string_nocopy() instead.")
bool
prop_dictionary_set_cstring_nocopy(prop_dictionary_t dict,
				   const char *key,
				   const char *cp)
{
	return prop_dictionary_set_string_nocopy(dict, key, cp);
}

bool
prop_dictionary_set_and_rel(prop_dictionary_t dict, const char *key,
			    prop_object_t po)
{
	bool rv;

	if (po == NULL)
		return false;
	rv = prop_dictionary_set(dict, key, po);
	prop_object_release(po);
	return rv;
}

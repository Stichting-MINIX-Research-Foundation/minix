/*	$NetBSD: prop_string.c,v 1.12 2014/03/26 18:12:46 christos Exp $	*/

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

#include <prop/prop_string.h>
#include "prop_object_impl.h"

struct _prop_string {
	struct _prop_object	ps_obj;
	union {
		char *		psu_mutable;
		const char *	psu_immutable;
	} ps_un;
#define	ps_mutable		ps_un.psu_mutable
#define	ps_immutable		ps_un.psu_immutable
	size_t			ps_size;	/* not including \0 */
	int			ps_flags;
};

#define	PS_F_NOCOPY		0x01

_PROP_POOL_INIT(_prop_string_pool, sizeof(struct _prop_string), "propstng")

_PROP_MALLOC_DEFINE(M_PROP_STRING, "prop string",
		    "property string container object")

static _prop_object_free_rv_t
		_prop_string_free(prop_stack_t, prop_object_t *);
static bool	_prop_string_externalize(
				struct _prop_object_externalize_context *,
				void *);
static _prop_object_equals_rv_t
		_prop_string_equals(prop_object_t, prop_object_t,
				    void **, void **,
				    prop_object_t *, prop_object_t *);

static const struct _prop_object_type _prop_object_type_string = {
	.pot_type	=	PROP_TYPE_STRING,
	.pot_free	=	_prop_string_free,
	.pot_extern	=	_prop_string_externalize,
	.pot_equals	=	_prop_string_equals,
};

#define	prop_object_is_string(x)	\
	((x) != NULL && (x)->ps_obj.po_type == &_prop_object_type_string)
#define	prop_string_contents(x)  ((x)->ps_immutable ? (x)->ps_immutable : "")

/* ARGSUSED */
static _prop_object_free_rv_t
_prop_string_free(prop_stack_t stack, prop_object_t *obj)
{
	prop_string_t ps = *obj;

	if ((ps->ps_flags & PS_F_NOCOPY) == 0 && ps->ps_mutable != NULL)
	    	_PROP_FREE(ps->ps_mutable, M_PROP_STRING);
	_PROP_POOL_PUT(_prop_string_pool, ps);

	return (_PROP_OBJECT_FREE_DONE);
}

static bool
_prop_string_externalize(struct _prop_object_externalize_context *ctx,
			 void *v)
{
	prop_string_t ps = v;

	if (ps->ps_size == 0)
		return (_prop_object_externalize_empty_tag(ctx, "string"));

	if (_prop_object_externalize_start_tag(ctx, "string") == false ||
	    _prop_object_externalize_append_encoded_cstring(ctx,
	    					ps->ps_immutable) == false ||
	    _prop_object_externalize_end_tag(ctx, "string") == false)
		return (false);
	
	return (true);
}

/* ARGSUSED */
static _prop_object_equals_rv_t
_prop_string_equals(prop_object_t v1, prop_object_t v2,
    void **stored_pointer1, void **stored_pointer2,
    prop_object_t *next_obj1, prop_object_t *next_obj2)
{
	prop_string_t str1 = v1;
	prop_string_t str2 = v2;

	if (str1 == str2)
		return (_PROP_OBJECT_EQUALS_TRUE);
	if (str1->ps_size != str2->ps_size)
		return (_PROP_OBJECT_EQUALS_FALSE);
	if (strcmp(prop_string_contents(str1), prop_string_contents(str2)))
		return (_PROP_OBJECT_EQUALS_FALSE);
	else
		return (_PROP_OBJECT_EQUALS_TRUE);
}

static prop_string_t
_prop_string_alloc(void)
{
	prop_string_t ps;

	ps = _PROP_POOL_GET(_prop_string_pool);
	if (ps != NULL) {
		_prop_object_init(&ps->ps_obj, &_prop_object_type_string);

		ps->ps_mutable = NULL;
		ps->ps_size = 0;
		ps->ps_flags = 0;
	}

	return (ps);
}

/*
 * prop_string_create --
 *	Create an empty mutable string.
 */
prop_string_t
prop_string_create(void)
{

	return (_prop_string_alloc());
}

/*
 * prop_string_create_cstring --
 *	Create a string that contains a copy of the provided C string.
 */
prop_string_t
prop_string_create_cstring(const char *str)
{
	prop_string_t ps;
	char *cp;
	size_t len;

	ps = _prop_string_alloc();
	if (ps != NULL) {
		len = strlen(str);
		cp = _PROP_MALLOC(len + 1, M_PROP_STRING);
		if (cp == NULL) {
			prop_object_release(ps);
			return (NULL);
		}
		strcpy(cp, str);
		ps->ps_mutable = cp;
		ps->ps_size = len;
	}
	return (ps);
}

/*
 * prop_string_create_cstring_nocopy --
 *	Create an immutable string that contains a refrence to the
 *	provided C string.
 */
prop_string_t
prop_string_create_cstring_nocopy(const char *str)
{
	prop_string_t ps;
	
	ps = _prop_string_alloc();
	if (ps != NULL) {
		ps->ps_immutable = str;
		ps->ps_size = strlen(str);
		ps->ps_flags |= PS_F_NOCOPY;
	}
	return (ps);
}

/*
 * prop_string_copy --
 *	Copy a string.  If the original string is immutable, then the
 *	copy is also immutable and references the same external data.
 */
prop_string_t
prop_string_copy(prop_string_t ops)
{
	prop_string_t ps;

	if (! prop_object_is_string(ops))
		return (NULL);

	ps = _prop_string_alloc();
	if (ps != NULL) {
		ps->ps_size = ops->ps_size;
		ps->ps_flags = ops->ps_flags;
		if (ops->ps_flags & PS_F_NOCOPY)
			ps->ps_immutable = ops->ps_immutable;
		else {
			char *cp = _PROP_MALLOC(ps->ps_size + 1, M_PROP_STRING);
			if (cp == NULL) {
				prop_object_release(ps);
				return (NULL);
			}
			strcpy(cp, prop_string_contents(ops));
			ps->ps_mutable = cp;
		}
	}
	return (ps);
}

/*
 * prop_string_copy_mutable --
 *	Copy a string, always returning a mutable copy.
 */
prop_string_t
prop_string_copy_mutable(prop_string_t ops)
{
	prop_string_t ps;
	char *cp;

	if (! prop_object_is_string(ops))
		return (NULL);

	ps = _prop_string_alloc();
	if (ps != NULL) {
		ps->ps_size = ops->ps_size;
		cp = _PROP_MALLOC(ps->ps_size + 1, M_PROP_STRING);
		if (cp == NULL) {
			prop_object_release(ps);
			return (NULL);
		}
		strcpy(cp, prop_string_contents(ops));
		ps->ps_mutable = cp;
	}
	return (ps);
}

/*
 * prop_string_size --
 *	Return the size of the string, not including the terminating NUL.
 */
size_t
prop_string_size(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (0);

	return (ps->ps_size);
}

/*
 * prop_string_mutable --
 *	Return true if the string is a mutable string.
 */
bool
prop_string_mutable(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (false);

	return ((ps->ps_flags & PS_F_NOCOPY) == 0);
}

/*
 * prop_string_cstring --
 *	Return a copy of the contents of the string as a C string.
 *	The string is allocated with the M_TEMP malloc type.
 */
char *
prop_string_cstring(prop_string_t ps)
{
	char *cp;

	if (! prop_object_is_string(ps))
		return (NULL);

	cp = _PROP_MALLOC(ps->ps_size + 1, M_TEMP);
	if (cp != NULL)
		strcpy(cp, prop_string_contents(ps));
	
	return (cp);
}

/*
 * prop_string_cstring_nocopy --
 *	Return an immutable reference to the contents of the string
 *	as a C string.
 */
const char *
prop_string_cstring_nocopy(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (NULL);

	return (prop_string_contents(ps));
}

/*
 * prop_string_append --
 *	Append the contents of one string to another.  Returns true
 *	upon success.  The destination string must be mutable.
 */
bool
prop_string_append(prop_string_t dst, prop_string_t src)
{
	char *ocp, *cp;
	size_t len;

	if (! (prop_object_is_string(dst) &&
	       prop_object_is_string(src)))
		return (false);

	if (dst->ps_flags & PS_F_NOCOPY)
		return (false);

	len = dst->ps_size + src->ps_size;
	cp = _PROP_MALLOC(len + 1, M_PROP_STRING);
	if (cp == NULL)
		return (false);
	snprintf(cp, len + 1, "%s%s", prop_string_contents(dst),
		prop_string_contents(src));
	ocp = dst->ps_mutable;
	dst->ps_mutable = cp;
	dst->ps_size = len;
	if (ocp != NULL)
		_PROP_FREE(ocp, M_PROP_STRING);
	
	return (true);
}

/*
 * prop_string_append_cstring --
 *	Append a C string to a string.  Returns true upon success.
 *	The destination string must be mutable.
 */
bool
prop_string_append_cstring(prop_string_t dst, const char *src)
{
	char *ocp, *cp;
	size_t len;

	if (! prop_object_is_string(dst))
		return (false);

	_PROP_ASSERT(src != NULL);

	if (dst->ps_flags & PS_F_NOCOPY)
		return (false);
	
	len = dst->ps_size + strlen(src);
	cp = _PROP_MALLOC(len + 1, M_PROP_STRING);
	if (cp == NULL)
		return (false);
	snprintf(cp, len + 1, "%s%s", prop_string_contents(dst), src);
	ocp = dst->ps_mutable;
	dst->ps_mutable = cp;
	dst->ps_size = len;
	if (ocp != NULL)
		_PROP_FREE(ocp, M_PROP_STRING);
	
	return (true);
}

/*
 * prop_string_equals --
 *	Return true if two strings are equivalent.
 */
bool
prop_string_equals(prop_string_t str1, prop_string_t str2)
{
	if (!prop_object_is_string(str1) || !prop_object_is_string(str2))
		return (false);

	return prop_object_equals(str1, str2);
}

/*
 * prop_string_equals_cstring --
 *	Return true if the string is equivalent to the specified
 *	C string.
 */
bool
prop_string_equals_cstring(prop_string_t ps, const char *cp)
{

	if (! prop_object_is_string(ps))
		return (false);

	return (strcmp(prop_string_contents(ps), cp) == 0);
}

/*
 * _prop_string_internalize --
 *	Parse a <string>...</string> and return the object created from the
 *	external representation.
 */
/* ARGSUSED */
bool
_prop_string_internalize(prop_stack_t stack, prop_object_t *obj,
    struct _prop_object_internalize_context *ctx)
{
	prop_string_t string;
	char *str;
	size_t len, alen;

	if (ctx->poic_is_empty_element) {
		*obj = prop_string_create();
		return (true);
	}
	
	/* No attributes recognized here. */
	if (ctx->poic_tagattr != NULL)
		return (true);

	/* Compute the length of the result. */
	if (_prop_object_internalize_decode_string(ctx, NULL, 0, &len,
						   NULL) == false)
		return (true);
	
	str = _PROP_MALLOC(len + 1, M_PROP_STRING);
	if (str == NULL)
		return (true);
	
	if (_prop_object_internalize_decode_string(ctx, str, len, &alen,
						   &ctx->poic_cp) == false ||
	    alen != len) {
		_PROP_FREE(str, M_PROP_STRING);
		return (true);
	}
	str[len] = '\0';

	if (_prop_object_internalize_find_tag(ctx, "string",
					      _PROP_TAG_TYPE_END) == false) {
		_PROP_FREE(str, M_PROP_STRING);
		return (true);
	}

	string = _prop_string_alloc();
	if (string == NULL) {
		_PROP_FREE(str, M_PROP_STRING);
		return (true);
	}

	string->ps_mutable = str;
	string->ps_size = len;
	*obj = string;

	return (true);
}

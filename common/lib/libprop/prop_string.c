/*	$NetBSD: prop_string.c,v 1.17 2022/08/03 21:13:46 riastradh Exp $	*/

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

#include "prop_object_impl.h"
#include <prop/prop_string.h>

#include <sys/rbtree.h>
#if defined(_KERNEL) || defined(_STANDALONE)
#include <sys/stdarg.h>
#else
#include <stdarg.h>
#endif /* _KERNEL || _STANDALONE */

struct _prop_string {
	struct _prop_object	ps_obj;
	union {
		char *		psu_mutable;
		const char *	psu_immutable;
	} ps_un;
#define	ps_mutable		ps_un.psu_mutable
#define	ps_immutable		ps_un.psu_immutable
	size_t			ps_size;	/* not including \0 */
	struct rb_node		ps_link;
	int			ps_flags;
};

#define	PS_F_NOCOPY		0x01
#define	PS_F_MUTABLE		0x02

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

/*
 * In order to reduce memory usage, all immutable string objects are
 * de-duplicated.
 */

static int
/*ARGSUSED*/
_prop_string_rb_compare_nodes(void *ctx _PROP_ARG_UNUSED,
			      const void *n1, const void *n2)
{
	const struct _prop_string * const ps1 = n1;
	const struct _prop_string * const ps2 = n2;

	_PROP_ASSERT(ps1->ps_immutable != NULL);
	_PROP_ASSERT(ps2->ps_immutable != NULL);

	return strcmp(ps1->ps_immutable, ps2->ps_immutable);
}

static int
/*ARGSUSED*/
_prop_string_rb_compare_key(void *ctx _PROP_ARG_UNUSED,
			    const void *n, const void *v)
{
	const struct _prop_string * const ps = n;
	const char * const cp = v;

	_PROP_ASSERT(ps->ps_immutable != NULL);

	return strcmp(ps->ps_immutable, cp);
}

static const rb_tree_ops_t _prop_string_rb_tree_ops = {
	.rbto_compare_nodes = _prop_string_rb_compare_nodes,
	.rbto_compare_key = _prop_string_rb_compare_key,
	.rbto_node_offset = offsetof(struct _prop_string, ps_link),
	.rbto_context = NULL
};

static struct rb_tree _prop_string_tree;

_PROP_ONCE_DECL(_prop_string_init_once)
_PROP_MUTEX_DECL_STATIC(_prop_string_tree_mutex)

static int
_prop_string_init(void)
{

	_PROP_MUTEX_INIT(_prop_string_tree_mutex);
	rb_tree_init(&_prop_string_tree,
		     &_prop_string_rb_tree_ops);

	return 0;
}

/* ARGSUSED */
static _prop_object_free_rv_t
_prop_string_free(prop_stack_t stack, prop_object_t *obj)
{
	prop_string_t ps = *obj;

	if ((ps->ps_flags & PS_F_MUTABLE) == 0) {
		_PROP_MUTEX_LOCK(_prop_string_tree_mutex);
		/*
		 * Double-check the retain count now that we've
		 * acquired the tree lock; holding this lock prevents
		 * new retains from coming in by finding it in the
		 * tree.
		 */
		if (_PROP_ATOMIC_LOAD(&ps->ps_obj.po_refcnt) == 0)
			rb_tree_remove_node(&_prop_string_tree, ps);
		else
			ps = NULL;
		_PROP_MUTEX_UNLOCK(_prop_string_tree_mutex);

		if (ps == NULL)
			return (_PROP_OBJECT_FREE_DONE);
	}

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
_prop_string_alloc(int const flags)
{
	prop_string_t ps;

	ps = _PROP_POOL_GET(_prop_string_pool);
	if (ps != NULL) {
		_prop_object_init(&ps->ps_obj, &_prop_object_type_string);

		ps->ps_mutable = NULL;
		ps->ps_size = 0;
		ps->ps_flags = flags;
	}

	return (ps);
}

static prop_string_t
_prop_string_instantiate(int const flags, const char * const str,
    size_t const len)
{
	prop_string_t ps;

	_PROP_ONCE_RUN(_prop_string_init_once, _prop_string_init);

	ps = _prop_string_alloc(flags);
	if (ps != NULL) {
		ps->ps_immutable = str;
		ps->ps_size = len;

		if ((flags & PS_F_MUTABLE) == 0) {
			prop_string_t ops;

			_PROP_MUTEX_LOCK(_prop_string_tree_mutex);
			ops = rb_tree_insert_node(&_prop_string_tree, ps);
			if (ops != ps) {
				/*
				 * Equivalent string object already exist;
				 * free the new one and return a reference
				 * to the existing object.
				 */
				prop_object_retain(ops);
				_PROP_MUTEX_UNLOCK(_prop_string_tree_mutex);
				_PROP_POOL_PUT(_prop_string_pool, ps);
				ps = ops;
			} else {
				_PROP_MUTEX_UNLOCK(_prop_string_tree_mutex);
			}
		}
	}

	return (ps);
}

_PROP_DEPRECATED(prop_string_create,
    "this program uses prop_string_create(); all functions "
    "supporting mutable prop_strings are deprecated.")
prop_string_t
prop_string_create(void)
{

	return (_prop_string_alloc(PS_F_MUTABLE));
}

_PROP_DEPRECATED(prop_string_create_cstring,
    "this program uses prop_string_create_cstring(); all functions "
    "supporting mutable prop_strings are deprecated.")
prop_string_t
prop_string_create_cstring(const char *str)
{
	prop_string_t ps;
	char *cp;
	size_t len;

	_PROP_ASSERT(str != NULL);

	ps = _prop_string_alloc(PS_F_MUTABLE);
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

_PROP_DEPRECATED(prop_string_create_cstring_nocopy,
    "this program uses prop_string_create_cstring_nocopy(), "
    "which is deprecated; use prop_string_create_nocopy() instead.")
prop_string_t
prop_string_create_cstring_nocopy(const char *str)
{
	return prop_string_create_nocopy(str);
}

/*
 * prop_string_create_format --
 *	Create a string object using the provided format string.
 */
prop_string_t __printflike(1, 2)
prop_string_create_format(const char *fmt, ...)
{
	prop_string_t ps;
	char *str = NULL;
	int len;
	size_t nlen;
	va_list ap;

	_PROP_ASSERT(fmt != NULL);

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (len < 0)
		return (NULL);
	nlen = len + 1;

	str = _PROP_MALLOC(nlen, M_PROP_STRING);
	if (str == NULL)
		return (NULL);

	va_start(ap, fmt);
	vsnprintf(str, nlen, fmt, ap);
	va_end(ap);

	ps = _prop_string_instantiate(0, str, (size_t)len);
	if (ps == NULL)
		_PROP_FREE(str, M_PROP_STRING);

	return (ps);
}

/*
 * prop_string_create_copy --
 *	Create a string object by coping the provided constant string.
 */
prop_string_t
prop_string_create_copy(const char *str)
{
	return prop_string_create_format("%s", str);
}

/*
 * prop_string_create_nocopy --
 *	Create a string object using the provided external constant
 *	string.
 */
prop_string_t
prop_string_create_nocopy(const char *str)
{

	_PROP_ASSERT(str != NULL);

	return _prop_string_instantiate(PS_F_NOCOPY, str, strlen(str));
}

/*
 * prop_string_copy --
 *	Copy a string.  This reduces to a retain in the common case.
 *	Deprecated mutable string objects must be copied.
 */
prop_string_t
prop_string_copy(prop_string_t ops)
{
	prop_string_t ps;
	char *cp;

	if (! prop_object_is_string(ops))
		return (NULL);

	if ((ops->ps_flags & PS_F_MUTABLE) == 0) {
		prop_object_retain(ops);
		return (ops);
	}

	cp = _PROP_MALLOC(ops->ps_size + 1, M_PROP_STRING);
	if (cp == NULL)
		return NULL;

	strcpy(cp, prop_string_contents(ops));

	ps = _prop_string_instantiate(PS_F_MUTABLE, cp, ops->ps_size);
	if (ps == NULL)
		_PROP_FREE(cp, M_PROP_STRING);

	return (ps);
}

_PROP_DEPRECATED(prop_string_copy_mutable,
    "this program uses prop_string_copy_mutable(); all functions "
    "supporting mutable prop_strings are deprecated.")
prop_string_t
prop_string_copy_mutable(prop_string_t ops)
{
	prop_string_t ps;
	char *cp;

	if (! prop_object_is_string(ops))
		return (NULL);

	cp = _PROP_MALLOC(ops->ps_size + 1, M_PROP_STRING);
	if (cp == NULL)
		return NULL;

	strcpy(cp, prop_string_contents(ops));

	ps = _prop_string_instantiate(PS_F_MUTABLE, cp, ops->ps_size);
	if (ps == NULL)
		_PROP_FREE(cp, M_PROP_STRING);

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
 * prop_string_value --
 *	Returns a pointer to the string object's value.  This pointer
 *	remains valid only as long as the string object.
 */
const char *
prop_string_value(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (NULL);

	if ((ps->ps_flags & PS_F_MUTABLE) == 0)
		return (ps->ps_immutable);

	return (prop_string_contents(ps));
}

/*
 * prop_string_copy_value --
 *	Copy the string object's value into the supplied buffer.
 */
bool
prop_string_copy_value(prop_string_t ps, void *buf, size_t buflen)
{

	if (! prop_object_is_string(ps))
		return (false);

	if (buf == NULL || buflen < ps->ps_size + 1)
		return (false);

	strcpy(buf, prop_string_contents(ps));

	return (true);
}

_PROP_DEPRECATED(prop_string_mutable,
    "this program uses prop_string_mutable(); all functions "
    "supporting mutable prop_strings are deprecated.")
bool
prop_string_mutable(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (false);

	return ((ps->ps_flags & PS_F_MUTABLE) != 0);
}

_PROP_DEPRECATED(prop_string_cstring,
    "this program uses prop_string_cstring(), "
    "which is deprecated; use prop_string_copy_value() instead.")
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

_PROP_DEPRECATED(prop_string_cstring_nocopy,
    "this program uses prop_string_cstring_nocopy(), "
    "which is deprecated; use prop_string_value() instead.")
const char *
prop_string_cstring_nocopy(prop_string_t ps)
{

	if (! prop_object_is_string(ps))
		return (NULL);

	return (prop_string_contents(ps));
}

_PROP_DEPRECATED(prop_string_append,
    "this program uses prop_string_append(); all functions "
    "supporting mutable prop_strings are deprecated.")
bool
prop_string_append(prop_string_t dst, prop_string_t src)
{
	char *ocp, *cp;
	size_t len;

	if (! (prop_object_is_string(dst) &&
	       prop_object_is_string(src)))
		return (false);

	if ((dst->ps_flags & PS_F_MUTABLE) == 0)
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

_PROP_DEPRECATED(prop_string_append_cstring,
    "this program uses prop_string_append_cstring(); all functions "
    "supporting mutable prop_strings are deprecated.")
bool
prop_string_append_cstring(prop_string_t dst, const char *src)
{
	char *ocp, *cp;
	size_t len;

	if (! prop_object_is_string(dst))
		return (false);

	_PROP_ASSERT(src != NULL);

	if ((dst->ps_flags & PS_F_MUTABLE) == 0)
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
 * prop_string_equals_string --
 *	Return true if the string object is equivalent to the specified
 *	C string.
 */
bool
prop_string_equals_string(prop_string_t ps, const char *cp)
{

	if (! prop_object_is_string(ps))
		return (false);

	return (strcmp(prop_string_contents(ps), cp) == 0);
}

_PROP_DEPRECATED(prop_string_equals_cstring,
    "this program uses prop_string_equals_cstring(), "
    "which is deprecated; prop_string_equals_string() instead.")
bool
prop_string_equals_cstring(prop_string_t ps, const char *cp)
{
	return prop_string_equals_string(ps, cp);
}

/*
 * prop_string_compare --
 *	Compare two string objects, using strcmp() semantics.
 */
int
prop_string_compare(prop_string_t ps1, prop_string_t ps2)
{
	if (!prop_object_is_string(ps1) || !prop_object_is_string(ps2))
		return (-666);	/* arbitrary */

	return (strcmp(prop_string_contents(ps1),
		       prop_string_contents(ps2)));
}

/*
 * prop_string_compare_string --
 *	Compare a string object to the specified C string, using
 *	strcmp() semantics.
 */
int
prop_string_compare_string(prop_string_t ps, const char *cp)
{
	if (!prop_object_is_string(ps))
		return (-666);	/* arbitrary */

	return (strcmp(prop_string_contents(ps), cp));
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

	string = _prop_string_instantiate(0, str, len);
	if (string == NULL)
		_PROP_FREE(str, M_PROP_STRING);

	*obj = string;
	return (true);
}

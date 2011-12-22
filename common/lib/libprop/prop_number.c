/*	$NetBSD: prop_number.c,v 1.23 2010/09/24 22:51:52 rmind Exp $	*/

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

#include <prop/prop_number.h>
#include "prop_object_impl.h"
#include "prop_rb_impl.h"

#if defined(_KERNEL)
#include <sys/systm.h>
#elif defined(_STANDALONE)
#include <sys/param.h>
#include <lib/libkern/libkern.h>
#else
#include <errno.h>
#include <stdlib.h>
#endif

struct _prop_number {
	struct _prop_object	pn_obj;
	struct rb_node		pn_link;
	struct _prop_number_value {
		union {
			int64_t  pnu_signed;
			uint64_t pnu_unsigned;
		} pnv_un;
#define	pnv_signed	pnv_un.pnu_signed
#define	pnv_unsigned	pnv_un.pnu_unsigned
		unsigned int	pnv_is_unsigned	:1,
						:31;
	} pn_value;
};

_PROP_POOL_INIT(_prop_number_pool, sizeof(struct _prop_number), "propnmbr")

static _prop_object_free_rv_t
		_prop_number_free(prop_stack_t, prop_object_t *);
static bool	_prop_number_externalize(
				struct _prop_object_externalize_context *,
				void *);
static _prop_object_equals_rv_t
		_prop_number_equals(prop_object_t, prop_object_t,
				    void **, void **,
				    prop_object_t *, prop_object_t *);

static void _prop_number_lock(void);
static void _prop_number_unlock(void);

static const struct _prop_object_type _prop_object_type_number = {
	.pot_type	=	PROP_TYPE_NUMBER,
	.pot_free	=	_prop_number_free,
	.pot_extern	=	_prop_number_externalize,
	.pot_equals	=	_prop_number_equals,
	.pot_lock       =       _prop_number_lock,
	.pot_unlock     =    	_prop_number_unlock,
};

#define	prop_object_is_number(x)	\
	((x) != NULL && (x)->pn_obj.po_type == &_prop_object_type_number)

/*
 * Number objects are immutable, and we are likely to have many number
 * objects that have the same value.  So, to save memory, we unique'ify
 * numbers so we only have one copy of each.
 */

static int
_prop_number_compare_values(const struct _prop_number_value *pnv1,
			    const struct _prop_number_value *pnv2)
{

	/* Signed numbers are sorted before unsigned numbers. */

	if (pnv1->pnv_is_unsigned) {
		if (! pnv2->pnv_is_unsigned)
			return (1);
		if (pnv1->pnv_unsigned < pnv2->pnv_unsigned)
			return (-1);
		if (pnv1->pnv_unsigned > pnv2->pnv_unsigned)
			return (1);
		return (0);
	}

	if (pnv2->pnv_is_unsigned)
		return (-1);
	if (pnv1->pnv_signed < pnv2->pnv_signed)
		return (-1);
	if (pnv1->pnv_signed > pnv2->pnv_signed)
		return (1);
	return (0);
}

static int
/*ARGSUSED*/
_prop_number_rb_compare_nodes(void *ctx __unused,
			      const void *n1, const void *n2)
{
	const struct _prop_number *pn1 = n1;
	const struct _prop_number *pn2 = n2;

	return _prop_number_compare_values(&pn1->pn_value, &pn2->pn_value);
}

static int
/*ARGSUSED*/
_prop_number_rb_compare_key(void *ctx __unused, const void *n, const void *v)
{
	const struct _prop_number *pn = n;
	const struct _prop_number_value *pnv = v;

	return _prop_number_compare_values(&pn->pn_value, pnv);
}

static const rb_tree_ops_t _prop_number_rb_tree_ops = {
	.rbto_compare_nodes = _prop_number_rb_compare_nodes,
	.rbto_compare_key = _prop_number_rb_compare_key,
	.rbto_node_offset = offsetof(struct _prop_number, pn_link),
	.rbto_context = NULL
};

static struct rb_tree _prop_number_tree;
_PROP_MUTEX_DECL_STATIC(_prop_number_tree_mutex)

/* ARGSUSED */
static _prop_object_free_rv_t
_prop_number_free(prop_stack_t stack, prop_object_t *obj)
{
	prop_number_t pn = *obj;

	_prop_rb_tree_remove_node(&_prop_number_tree, pn);

	_PROP_POOL_PUT(_prop_number_pool, pn);

	return (_PROP_OBJECT_FREE_DONE);
}

_PROP_ONCE_DECL(_prop_number_init_once)

static int
_prop_number_init(void)
{

	_PROP_MUTEX_INIT(_prop_number_tree_mutex);
	_prop_rb_tree_init(&_prop_number_tree, &_prop_number_rb_tree_ops);
	return 0;
}

static void 
_prop_number_lock(void)
{
	/* XXX: init necessary? */
	_PROP_ONCE_RUN(_prop_number_init_once, _prop_number_init);
	_PROP_MUTEX_LOCK(_prop_number_tree_mutex);
}

static void
_prop_number_unlock(void)
{
	_PROP_MUTEX_UNLOCK(_prop_number_tree_mutex);
}
	
static bool
_prop_number_externalize(struct _prop_object_externalize_context *ctx,
			 void *v)
{
	prop_number_t pn = v;
	char tmpstr[32];

	/*
	 * For unsigned numbers, we output in hex.  For signed numbers,
	 * we output in decimal.
	 */
	if (pn->pn_value.pnv_is_unsigned)
		sprintf(tmpstr, "0x%" PRIx64, pn->pn_value.pnv_unsigned);
	else
		sprintf(tmpstr, "%" PRIi64, pn->pn_value.pnv_signed);

	if (_prop_object_externalize_start_tag(ctx, "integer") == false ||
	    _prop_object_externalize_append_cstring(ctx, tmpstr) == false ||
	    _prop_object_externalize_end_tag(ctx, "integer") == false)
		return (false);
	
	return (true);
}

/* ARGSUSED */
static _prop_object_equals_rv_t
_prop_number_equals(prop_object_t v1, prop_object_t v2,
    void **stored_pointer1, void **stored_pointer2,
    prop_object_t *next_obj1, prop_object_t *next_obj2)
{
	prop_number_t num1 = v1;
	prop_number_t num2 = v2;

	/*
	 * There is only ever one copy of a number object at any given
	 * time, so we can reduce this to a simple pointer equality check
	 * in the common case.
	 */
	if (num1 == num2)
		return (_PROP_OBJECT_EQUALS_TRUE);

	/*
	 * If the numbers are the same signed-ness, then we know they
	 * cannot be equal because they would have had pointer equality.
	 */
	if (num1->pn_value.pnv_is_unsigned == num2->pn_value.pnv_is_unsigned)
		return (_PROP_OBJECT_EQUALS_FALSE);

	/*
	 * We now have one signed value and one unsigned value.  We can
	 * compare them iff:
	 *	- The unsigned value is not larger than the signed value
	 *	  can represent.
	 *	- The signed value is not smaller than the unsigned value
	 *	  can represent.
	 */
	if (num1->pn_value.pnv_is_unsigned) {
		/*
		 * num1 is unsigned and num2 is signed.
		 */
		if (num1->pn_value.pnv_unsigned > INT64_MAX)
			return (_PROP_OBJECT_EQUALS_FALSE);
		if (num2->pn_value.pnv_signed < 0)
			return (_PROP_OBJECT_EQUALS_FALSE);
	} else {
		/*
		 * num1 is signed and num2 is unsigned.
		 */
		if (num1->pn_value.pnv_signed < 0)
			return (_PROP_OBJECT_EQUALS_FALSE);
		if (num2->pn_value.pnv_unsigned > INT64_MAX)
			return (_PROP_OBJECT_EQUALS_FALSE);
	}

	if (num1->pn_value.pnv_signed == num2->pn_value.pnv_signed)
		return _PROP_OBJECT_EQUALS_TRUE;
	else
		return _PROP_OBJECT_EQUALS_FALSE;
}

static prop_number_t
_prop_number_alloc(const struct _prop_number_value *pnv)
{
	prop_number_t opn, pn, rpn;

	_PROP_ONCE_RUN(_prop_number_init_once, _prop_number_init);

	/*
	 * Check to see if this already exists in the tree.  If it does,
	 * we just retain it and return it.
	 */
	_PROP_MUTEX_LOCK(_prop_number_tree_mutex);
	opn = _prop_rb_tree_find(&_prop_number_tree, pnv);
	if (opn != NULL) {
		prop_object_retain(opn);
		_PROP_MUTEX_UNLOCK(_prop_number_tree_mutex);
		return (opn);
	}
	_PROP_MUTEX_UNLOCK(_prop_number_tree_mutex);

	/*
	 * Not in the tree.  Create it now.
	 */

	pn = _PROP_POOL_GET(_prop_number_pool);
	if (pn == NULL)
		return (NULL);

	_prop_object_init(&pn->pn_obj, &_prop_object_type_number);

	pn->pn_value = *pnv;

	/*
	 * We dropped the mutex when we allocated the new object, so
	 * we have to check again if it is in the tree.
	 */
	_PROP_MUTEX_LOCK(_prop_number_tree_mutex);
	opn = _prop_rb_tree_find(&_prop_number_tree, pnv);
	if (opn != NULL) {
		prop_object_retain(opn);
		_PROP_MUTEX_UNLOCK(_prop_number_tree_mutex);
		_PROP_POOL_PUT(_prop_number_pool, pn);
		return (opn);
	}
	rpn = _prop_rb_tree_insert_node(&_prop_number_tree, pn);
	_PROP_ASSERT(rpn == pn);
	_PROP_MUTEX_UNLOCK(_prop_number_tree_mutex);
	return (pn);
}

/*
 * prop_number_create_integer --
 *	Create a prop_number_t and initialize it with the
 *	provided integer value.
 */
prop_number_t
prop_number_create_integer(int64_t val)
{
	struct _prop_number_value pnv;

	memset(&pnv, 0, sizeof(pnv));
	pnv.pnv_signed = val;
	pnv.pnv_is_unsigned = false;

	return (_prop_number_alloc(&pnv));
}

/*
 * prop_number_create_unsigned_integer --
 *	Create a prop_number_t and initialize it with the
 *	provided unsigned integer value.
 */
prop_number_t
prop_number_create_unsigned_integer(uint64_t val)
{
	struct _prop_number_value pnv;

	memset(&pnv, 0, sizeof(pnv));
	pnv.pnv_unsigned = val;
	pnv.pnv_is_unsigned = true;

	return (_prop_number_alloc(&pnv));
}

/*
 * prop_number_copy --
 *	Copy a prop_number_t.
 */
prop_number_t
prop_number_copy(prop_number_t opn)
{

	if (! prop_object_is_number(opn))
		return (NULL);

	/*
	 * Because we only ever allocate one object for any given
	 * value, this can be reduced to a simple retain operation.
	 */
	prop_object_retain(opn);
	return (opn);
}

/*
 * prop_number_unsigned --
 *	Returns true if the prop_number_t has an unsigned value.
 */
bool
prop_number_unsigned(prop_number_t pn)
{

	return (pn->pn_value.pnv_is_unsigned);
}

/*
 * prop_number_size --
 *	Return the size, in bits, required to hold the value of
 *	the specified number.
 */
int
prop_number_size(prop_number_t pn)
{
	struct _prop_number_value *pnv;

	if (! prop_object_is_number(pn))
		return (0);

	pnv = &pn->pn_value;

	if (pnv->pnv_is_unsigned) {
		if (pnv->pnv_unsigned > UINT32_MAX)
			return (64);
		if (pnv->pnv_unsigned > UINT16_MAX)
			return (32);
		if (pnv->pnv_unsigned > UINT8_MAX)
			return (16);
		return (8);
	}

	if (pnv->pnv_signed > INT32_MAX || pnv->pnv_signed < INT32_MIN)
	    	return (64);
	if (pnv->pnv_signed > INT16_MAX || pnv->pnv_signed < INT16_MIN)
		return (32);
	if (pnv->pnv_signed > INT8_MAX  || pnv->pnv_signed < INT8_MIN)
		return (16);
	return (8);
}

/*
 * prop_number_integer_value --
 *	Get the integer value of a prop_number_t.
 */
int64_t
prop_number_integer_value(prop_number_t pn)
{

	/*
	 * XXX Impossible to distinguish between "not a prop_number_t"
	 * XXX and "prop_number_t has a value of 0".
	 */
	if (! prop_object_is_number(pn))
		return (0);

	return (pn->pn_value.pnv_signed);
}

/*
 * prop_number_unsigned_integer_value --
 *	Get the unsigned integer value of a prop_number_t.
 */
uint64_t
prop_number_unsigned_integer_value(prop_number_t pn)
{

	/*
	 * XXX Impossible to distinguish between "not a prop_number_t"
	 * XXX and "prop_number_t has a value of 0".
	 */
	if (! prop_object_is_number(pn))
		return (0);

	return (pn->pn_value.pnv_unsigned);
}

/*
 * prop_number_equals --
 *	Return true if two numbers are equivalent.
 */
bool
prop_number_equals(prop_number_t num1, prop_number_t num2)
{
	if (!prop_object_is_number(num1) || !prop_object_is_number(num2))
		return (false);

	return (prop_object_equals(num1, num2));
}

/*
 * prop_number_equals_integer --
 *	Return true if the number is equivalent to the specified integer.
 */
bool
prop_number_equals_integer(prop_number_t pn, int64_t val)
{

	if (! prop_object_is_number(pn))
		return (false);

	if (pn->pn_value.pnv_is_unsigned &&
	    (pn->pn_value.pnv_unsigned > INT64_MAX || val < 0))
		return (false);
	
	return (pn->pn_value.pnv_signed == val);
}

/*
 * prop_number_equals_unsigned_integer --
 *	Return true if the number is equivalent to the specified
 *	unsigned integer.
 */
bool
prop_number_equals_unsigned_integer(prop_number_t pn, uint64_t val)
{

	if (! prop_object_is_number(pn))
		return (false);
	
	if (! pn->pn_value.pnv_is_unsigned &&
	    (pn->pn_value.pnv_signed < 0 || val > INT64_MAX))
		return (false);
	
	return (pn->pn_value.pnv_unsigned == val);
}

static bool
_prop_number_internalize_unsigned(struct _prop_object_internalize_context *ctx,
				  struct _prop_number_value *pnv)
{
	char *cp;

	_PROP_ASSERT(/*CONSTCOND*/sizeof(unsigned long long) ==
		     sizeof(uint64_t));

#ifndef _KERNEL
	errno = 0;
#endif
	pnv->pnv_unsigned = (uint64_t) strtoull(ctx->poic_cp, &cp, 0);
#ifndef _KERNEL		/* XXX can't check for ERANGE in the kernel */
	if (pnv->pnv_unsigned == UINT64_MAX && errno == ERANGE)
		return (false);
#endif
	pnv->pnv_is_unsigned = true;
	ctx->poic_cp = cp;

	return (true);
}

static bool
_prop_number_internalize_signed(struct _prop_object_internalize_context *ctx,
				struct _prop_number_value *pnv)
{
	char *cp;

	_PROP_ASSERT(/*CONSTCOND*/sizeof(long long) == sizeof(int64_t));

#ifndef _KERNEL
	errno = 0;
#endif
	pnv->pnv_signed = (int64_t) strtoll(ctx->poic_cp, &cp, 0);
#ifndef _KERNEL		/* XXX can't check for ERANGE in the kernel */
	if ((pnv->pnv_signed == INT64_MAX || pnv->pnv_signed == INT64_MIN) &&
	    errno == ERANGE)
	    	return (false);
#endif
	pnv->pnv_is_unsigned = false;
	ctx->poic_cp = cp;

	return (true);
}

/*
 * _prop_number_internalize --
 *	Parse a <number>...</number> and return the object created from
 *	the external representation.
 */
/* ARGSUSED */
bool
_prop_number_internalize(prop_stack_t stack, prop_object_t *obj,
    struct _prop_object_internalize_context *ctx)
{
	struct _prop_number_value pnv;

	memset(&pnv, 0, sizeof(pnv));

	/* No attributes, no empty elements. */
	if (ctx->poic_tagattr != NULL || ctx->poic_is_empty_element)
		return (true);

	/*
	 * If the first character is '-', then we treat as signed.
	 * If the first two characters are "0x" (i.e. the number is
	 * in hex), then we treat as unsigned.  Otherwise, we try
	 * signed first, and if that fails (presumably due to ERANGE),
	 * then we switch to unsigned.
	 */
	if (ctx->poic_cp[0] == '-') {
		if (_prop_number_internalize_signed(ctx, &pnv) == false)
			return (true);
	} else if (ctx->poic_cp[0] == '0' && ctx->poic_cp[1] == 'x') {
		if (_prop_number_internalize_unsigned(ctx, &pnv) == false)
			return (true);
	} else {
		if (_prop_number_internalize_signed(ctx, &pnv) == false &&
		    _prop_number_internalize_unsigned(ctx, &pnv) == false)
		    	return (true);
	}

	if (_prop_object_internalize_find_tag(ctx, "integer",
					      _PROP_TAG_TYPE_END) == false)
		return (true);

	*obj = _prop_number_alloc(&pnv);
	return (true);
}

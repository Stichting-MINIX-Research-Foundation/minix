/* $NetBSD: prop_stack.c,v 1.2 2007/08/30 12:23:54 joerg Exp $ */

/*-
 * Copyright (c) 2007 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "prop_stack.h"
#include "prop_object_impl.h"

void
_prop_stack_init(prop_stack_t stack)
{
	stack->used_intern_elems = 0;
	SLIST_INIT(&stack->extern_elems);
}

bool
_prop_stack_push(prop_stack_t stack, prop_object_t obj, void *data1,
    void *data2, void *data3)
{
	struct _prop_stack_extern_elem *eelem;
	struct _prop_stack_intern_elem *ielem;

	if (stack->used_intern_elems == PROP_STACK_INTERN_ELEMS) {
		eelem = _PROP_MALLOC(sizeof(*eelem), M_TEMP);

		if (eelem == NULL)
			return false;

		eelem->object = obj;
		eelem->object_data[0] = data1;
		eelem->object_data[1] = data2;
		eelem->object_data[2] = data3;

		SLIST_INSERT_HEAD(&stack->extern_elems, eelem, stack_link);

		return true;
	}

	_PROP_ASSERT(stack->used_intern_elems < PROP_STACK_INTERN_ELEMS);
	_PROP_ASSERT(SLIST_EMPTY(&stack->extern_elems));

	ielem = &stack->intern_elems[stack->used_intern_elems];
	ielem->object = obj;
	ielem->object_data[0] = data1;
	ielem->object_data[1] = data2;
	ielem->object_data[2] = data3;

	++stack->used_intern_elems;

	return true;
}

bool
_prop_stack_pop(prop_stack_t stack, prop_object_t *obj, void **data1,
    void **data2, void **data3)
{
	struct _prop_stack_extern_elem *eelem;
	struct _prop_stack_intern_elem *ielem;

	if (stack->used_intern_elems == 0)
		return false;

	if ((eelem = SLIST_FIRST(&stack->extern_elems)) != NULL) {
		_PROP_ASSERT(stack->used_intern_elems == PROP_STACK_INTERN_ELEMS);

		SLIST_REMOVE_HEAD(&stack->extern_elems, stack_link);
		if (obj)
			*obj = eelem->object;
		if (data1)
			*data1 = eelem->object_data[0];
		if (data2)
			*data2 = eelem->object_data[1];
		if (data3)
			*data3 = eelem->object_data[2];
		_PROP_FREE(eelem, M_TEMP);
		return true;
	}

	--stack->used_intern_elems;
	ielem = &stack->intern_elems[stack->used_intern_elems];

	if (obj)
		*obj = ielem->object;
	if (data1)
		*data1 = ielem->object_data[0];
	if (data2)
		*data2 = ielem->object_data[1];
	if (data3)
		*data3 = ielem->object_data[2];

	return true;
}

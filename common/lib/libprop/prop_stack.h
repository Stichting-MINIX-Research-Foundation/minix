/* $NetBSD: prop_stack.h,v 1.2 2007/08/30 12:23:54 joerg Exp $ */

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

#ifndef _PROP_STACK_H
#define _PROP_STACK_H

#include <sys/queue.h>

#include <prop/prop_object.h>

struct _prop_stack_intern_elem {
	prop_object_t object;
	void *object_data[3];
};

struct _prop_stack_extern_elem {
	SLIST_ENTRY(_prop_stack_extern_elem) stack_link;
	prop_object_t object;
	void *object_data[3];
};

#define	PROP_STACK_INTERN_ELEMS	16

struct _prop_stack {
	struct _prop_stack_intern_elem intern_elems[PROP_STACK_INTERN_ELEMS];
	size_t used_intern_elems;
	SLIST_HEAD(, _prop_stack_extern_elem) extern_elems;
};

typedef struct _prop_stack *prop_stack_t;

void	_prop_stack_init(prop_stack_t);
bool	_prop_stack_push(prop_stack_t, prop_object_t, void *, void *, void *);
bool	_prop_stack_pop(prop_stack_t, prop_object_t *, void **, void **, void **);

#endif

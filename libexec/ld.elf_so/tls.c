/*	$NetBSD: tls.c,v 1.7 2011/04/23 16:40:08 joerg Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: tls.c,v 1.7 2011/04/23 16:40:08 joerg Exp $");

#include <sys/param.h>
#include <sys/ucontext.h>
#include <lwp.h>
#include <string.h>
#include "rtld.h"

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)

static struct tls_tcb *_rtld_tls_allocate_locked(void);

#ifndef TLS_DTV_OFFSET
#define	TLS_DTV_OFFSET	0
#endif

static size_t _rtld_tls_static_space;	/* Static TLS space allocated */
static size_t _rtld_tls_static_offset;	/* Next offset for static TLS to use */
size_t _rtld_tls_dtv_generation = 1;
size_t _rtld_tls_max_index = 1;

#define	DTV_GENERATION(dtv)	((size_t)((dtv)[0]))
#define	DTV_MAX_INDEX(dtv)	((size_t)((dtv)[-1]))
#define	SET_DTV_GENERATION(dtv, val)	(dtv)[0] = (void *)(size_t)(val)
#define	SET_DTV_MAX_INDEX(dtv, val)	(dtv)[-1] = (void *)(size_t)(val)

void *
_rtld_tls_get_addr(void *tls, size_t idx, size_t offset)
{
	struct tls_tcb *tcb = tls;
	void **dtv, **new_dtv;
	sigset_t mask;

	_rtld_exclusive_enter(&mask);

	dtv = tcb->tcb_dtv;

	if (__predict_false(DTV_GENERATION(dtv) != _rtld_tls_dtv_generation)) {
		size_t to_copy = DTV_MAX_INDEX(dtv);

		new_dtv = xcalloc((2 + _rtld_tls_max_index) * sizeof(*dtv));
		++new_dtv;
		if (to_copy > _rtld_tls_max_index)
			to_copy = _rtld_tls_max_index;
		memcpy(new_dtv + 1, dtv + 1, to_copy * sizeof(*dtv));
		xfree(dtv - 1);
		dtv = tcb->tcb_dtv = new_dtv;
		SET_DTV_MAX_INDEX(dtv, _rtld_tls_max_index);
		SET_DTV_GENERATION(dtv, _rtld_tls_dtv_generation);
	}

	if (__predict_false(dtv[idx] == NULL))
		dtv[idx] = _rtld_tls_module_allocate(idx);

	_rtld_exclusive_exit(&mask);

	return (uint8_t *)dtv[idx] + offset;
}

void
_rtld_tls_initial_allocation(void)
{
	struct tls_tcb *tcb;

	_rtld_tls_static_space = _rtld_tls_static_offset +
	    RTLD_STATIC_TLS_RESERVATION;

#ifndef __HAVE_TLS_VARIANT_I
	_rtld_tls_static_space = roundup2(_rtld_tls_static_space,
	    sizeof(void *));
#endif

	tcb = _rtld_tls_allocate_locked();
#ifdef __HAVE___LWP_SETTCB
	__lwp_settcb(tcb);
#ifdef __powerpc__
	/*
	 * Save the tcb pointer so that libc can retrieve it.  Older
	 * crt0 will obliterate r2 so there is code in libc to restore it.
	 */
	_lwp_setprivate(tcb);
#endif
#else
	_lwp_setprivate(tcb);
#endif
}

static struct tls_tcb *
_rtld_tls_allocate_locked(void)
{
	Obj_Entry *obj;
	struct tls_tcb *tcb;
	uint8_t *p, *q;

	p = xcalloc(_rtld_tls_static_space + sizeof(struct tls_tcb));
#ifdef __HAVE_TLS_VARIANT_I
	tcb = (struct tls_tcb *)p;
	p += sizeof(struct tls_tcb);
#else
	p += _rtld_tls_static_space;
	tcb = (struct tls_tcb *)p;
	tcb->tcb_self = tcb;
#endif
	tcb->tcb_dtv = xcalloc(sizeof(*tcb->tcb_dtv) * (2 + _rtld_tls_max_index));
	++tcb->tcb_dtv;
	SET_DTV_MAX_INDEX(tcb->tcb_dtv, _rtld_tls_max_index);
	SET_DTV_GENERATION(tcb->tcb_dtv, _rtld_tls_dtv_generation);

	for (obj = _rtld_objlist; obj != NULL; obj = obj->next) {
		if (obj->tlssize) {
#ifdef __HAVE_TLS_VARIANT_I
			q = p + obj->tlsoffset;
#else
			q = p - obj->tlsoffset;
#endif
			memcpy(q, obj->tlsinit, obj->tlsinitsize);
			tcb->tcb_dtv[obj->tlsindex] = q;
		}
	}

	return tcb;
}

struct tls_tcb *
_rtld_tls_allocate(void)
{
	struct tls_tcb *tcb;
	sigset_t mask;

	_rtld_exclusive_enter(&mask);
	tcb = _rtld_tls_allocate_locked();
	_rtld_exclusive_exit(&mask);

	return tcb;
}

void
_rtld_tls_free(struct tls_tcb *tcb)
{
	size_t i, max_index;
	uint8_t *p;
	sigset_t mask;

	_rtld_exclusive_enter(&mask);

	max_index = DTV_MAX_INDEX(tcb->tcb_dtv);
	for (i = 1; i <= max_index; ++i)
		xfree(tcb->tcb_dtv[i]);
	xfree(tcb->tcb_dtv - 1);

#ifdef __HAVE_TLS_VARIANT_I
	p = (uint8_t *)tcb;
#else
	p = (uint8_t *)tcb - _rtld_tls_static_space;
#endif
	xfree(p);

	_rtld_exclusive_exit(&mask);
}

void *
_rtld_tls_module_allocate(size_t idx)
{
	Obj_Entry *obj;
	uint8_t *p;

	for (obj = _rtld_objlist; obj != NULL; obj = obj->next) {
		if (obj->tlsindex == idx)
			break;
	}
	if (obj == NULL) {
		_rtld_error("Module for TLS index %zu missing", idx);
		_rtld_die();
	}

	p = xmalloc(obj->tlssize);
	memcpy(p, obj->tlsinit, obj->tlsinitsize);
	memset(p + obj->tlsinitsize, 0, obj->tlssize - obj->tlsinitsize);

	return p;
}

int
_rtld_tls_offset_allocate(Obj_Entry *obj)
{
	size_t offset, next_offset;

	if (obj->tls_done)
		return 0;
	if (obj->tlssize == 0) {
		obj->tlsoffset = 0;
		obj->tls_done = 1;
		return 0;
	}

#ifdef __HAVE_TLS_VARIANT_I
	offset = roundup2(_rtld_tls_static_offset, obj->tlsalign);
	next_offset = offset + obj->tlssize;
#else
	offset = roundup2(_rtld_tls_static_offset + obj->tlssize,
	    obj->tlsalign);
	next_offset = offset;
#endif

	/*
	 * Check if the static allocation was already done.
	 * This happens if dynamically loaded modules want to use
	 * static TLS space.
	 *
	 * XXX Keep an actual free list and callbacks for initialisation.
	 */
	if (_rtld_tls_static_space) {
		if (obj->tlsinitsize) {
			_rtld_error("%s: Use of initialized "
			    "Thread Local Storage with model initial-exec "
			    "and dlopen is not supported",
			    obj->path);
			return -1;
		}
		if (next_offset > _rtld_tls_static_space) {
			_rtld_error("%s: No space available "
			    "for static Thread Local Storage",
			    obj->path);
			return -1;
		}
	}
	obj->tlsoffset = offset;
	_rtld_tls_static_offset = next_offset;
	obj->tls_done = 1;

	return 0;
}

void
_rtld_tls_offset_free(Obj_Entry *obj)
{

	/*
	 * XXX See above.
	 */
	obj->tls_done = 0;
	return;
}

#ifdef __HAVE_COMMON___TLS_GET_ADDR
/*
 * The fast path is access to an already allocated DTV entry.
 * This checks the current limit and the entry without needing any
 * locking. Entries are only freed on dlclose() and it is an application
 * bug if code of the module is still running at that point.
 */
void *
__tls_get_addr(void *arg_)
{
	size_t *arg = (size_t *)arg_;
	void **dtv;
#ifdef __HAVE___LWP_GETTCB_FAST
	struct tls_tcb * const tcb = __lwp_gettcb_fast();
#else
	struct tls_tcb * const tcb = __lwp_getprivate_fast();
#endif
	size_t idx = arg[0], offset = arg[1] + TLS_DTV_OFFSET;

	dtv = tcb->tcb_dtv;

	if (__predict_true(idx < DTV_MAX_INDEX(dtv) && dtv[idx] != NULL))
		return (uint8_t *)dtv[idx] + offset;

	return _rtld_tls_get_addr(tcb, idx, offset);
}
#endif

#endif /* __HAVE_TLS_VARIANT_I || __HAVE_TLS_VARIANT_II */

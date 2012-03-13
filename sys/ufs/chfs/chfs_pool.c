/*	$NetBSD: chfs_pool.c,v 1.1 2011/11/24 15:51:31 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Pool allocator and convenience routines for chfs.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

#include "chfs.h"
//#include </root/xipffs/netbsd.chfs/chfs.h>

/* --------------------------------------------------------------------- */

void *	chfs_pool_page_alloc(struct pool *, int);
void	chfs_pool_page_free(struct pool *, void *);

extern void*	pool_page_alloc_nointr(struct pool *, int);
extern void	pool_page_free_nointr(struct pool *, void *);

/* --------------------------------------------------------------------- */

struct pool_allocator chfs_pool_allocator = {
	.pa_alloc = chfs_pool_page_alloc,
	.pa_free = chfs_pool_page_free,
};

/* --------------------------------------------------------------------- */

void
chfs_pool_init(struct chfs_pool *chpp, size_t size, const char *what,
    struct chfs_mount *chmp)
{
	int cnt;

	cnt = snprintf(chpp->chp_name, sizeof(chpp->chp_name),
	    "%s_chfs_%p", what, chmp);
	KASSERT(cnt < sizeof(chpp->chp_name));

	pool_init(&chpp->chp_pool, size, 0, 0, 0, chpp->chp_name,
	    &chfs_pool_allocator, IPL_NONE);
	chpp->chp_mount = chmp;
}

/* --------------------------------------------------------------------- */

void
chfs_pool_destroy(struct chfs_pool *chpp)
{
	pool_destroy((struct pool *)chpp);
}

/* --------------------------------------------------------------------- */

void *
chfs_pool_page_alloc(struct pool *pp, int flags)
{
	struct chfs_pool *chpp;
	struct chfs_mount *chmp;
	unsigned int pages;
	void *page;
	dbg("CHFS: pool_page_alloc()\n");

	chpp = (struct chfs_pool *)pp;
	chmp = chpp->chp_mount;

	pages = atomic_inc_uint_nv(&chmp->chm_pages_used);
	if (pages >= CHFS_PAGES_MAX(chmp)) {
		atomic_dec_uint(&chmp->chm_pages_used);
		return NULL;
	}
	page = pool_page_alloc_nointr(pp, flags | PR_WAITOK);
	if (page == NULL) {
		atomic_dec_uint(&chmp->chm_pages_used);
	}

	return page;
}

/* --------------------------------------------------------------------- */

void
chfs_pool_page_free(struct pool *pp, void *v)
{
	struct chfs_pool *chpp;
	struct chfs_mount *chmp;
	dbg("CHFS: pool_page_free()\n");

	chpp = (struct chfs_pool *)pp;
	chmp = chpp->chp_mount;

	atomic_dec_uint(&chmp->chm_pages_used);
	pool_page_free_nointr(pp, v);
}

/* --------------------------------------------------------------------- */

void
chfs_str_pool_init(struct chfs_str_pool *chsp, struct chfs_mount *chmp)
{
	dbg("CHFS: str_pool_init()\n");

	chfs_pool_init(&chsp->chsp_pool_16,   16,   "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_32,   32,   "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_64,   64,   "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_128,  128,  "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_256,  256,  "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_512,  512,  "str", chmp);
	chfs_pool_init(&chsp->chsp_pool_1024, 1024, "str", chmp);
}

/* --------------------------------------------------------------------- */

void
chfs_str_pool_destroy(struct chfs_str_pool *chsp)
{
	dbg("CHFS: str_pool_destroy()\n");

	chfs_pool_destroy(&chsp->chsp_pool_16);
	chfs_pool_destroy(&chsp->chsp_pool_32);
	chfs_pool_destroy(&chsp->chsp_pool_64);
	chfs_pool_destroy(&chsp->chsp_pool_128);
	chfs_pool_destroy(&chsp->chsp_pool_256);
	chfs_pool_destroy(&chsp->chsp_pool_512);
	chfs_pool_destroy(&chsp->chsp_pool_1024);
}

/* --------------------------------------------------------------------- */

char *
chfs_str_pool_get(struct chfs_str_pool *chsp, size_t len, int flags)
{
	struct chfs_pool *p;
	dbg("CHFS: str_pool_get()\n");

	KASSERT(len <= 1024);

	if      (len <= 16)   p = &chsp->chsp_pool_16;
	else if (len <= 32)   p = &chsp->chsp_pool_32;
	else if (len <= 64)   p = &chsp->chsp_pool_64;
	else if (len <= 128)  p = &chsp->chsp_pool_128;
	else if (len <= 256)  p = &chsp->chsp_pool_256;
	else if (len <= 512)  p = &chsp->chsp_pool_512;
	else if (len <= 1024) p = &chsp->chsp_pool_1024;
	else {
		KASSERT(0);
		p = NULL; /* Silence compiler warnings */
	}

	return (char *)CHFS_POOL_GET(p, flags);
}

/* --------------------------------------------------------------------- */

void
chfs_str_pool_put(struct chfs_str_pool *chsp, char *str, size_t len)
{
	struct chfs_pool *p;
	dbg("CHFS: str_pool_put()\n");

	KASSERT(len <= 1024);

	if      (len <= 16)   p = &chsp->chsp_pool_16;
	else if (len <= 32)   p = &chsp->chsp_pool_32;
	else if (len <= 64)   p = &chsp->chsp_pool_64;
	else if (len <= 128)  p = &chsp->chsp_pool_128;
	else if (len <= 256)  p = &chsp->chsp_pool_256;
	else if (len <= 512)  p = &chsp->chsp_pool_512;
	else if (len <= 1024) p = &chsp->chsp_pool_1024;
	else {
		KASSERT(0);
		p = NULL; /* Silence compiler warnings */
	}

	CHFS_POOL_PUT(p, str);
}

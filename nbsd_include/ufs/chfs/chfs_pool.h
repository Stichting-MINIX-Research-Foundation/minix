/*	$NetBSD: chfs_pool.h,v 1.1 2011/11/24 15:51:31 ahoka Exp $	*/

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

#ifndef _FS_CHFS_CHFS_POOL_H_
#define _FS_CHFS_CHFS_POOL_H_


/* --------------------------------------------------------------------- */

struct chfs_pool {
	struct pool		chp_pool;
	struct chfs_mount *	chp_mount;
	char			chp_name[64];
};

/* --------------------------------------------------------------------- */

struct chfs_str_pool {
	struct chfs_pool	chsp_pool_16;
	struct chfs_pool	chsp_pool_32;
	struct chfs_pool	chsp_pool_64;
	struct chfs_pool	chsp_pool_128;
	struct chfs_pool	chsp_pool_256;
	struct chfs_pool	chsp_pool_512;
	struct chfs_pool	chsp_pool_1024;
};

/* --------------------------------------------------------------------- */
#ifdef _KERNEL

/*
 * Convenience functions and macros to manipulate a chfs_pool.
 */

void	chfs_pool_init(struct chfs_pool *chpp, size_t size,
    const char *what, struct chfs_mount *chmp);
void	chfs_pool_destroy(struct chfs_pool *chpp);

#define	CHFS_POOL_GET(chpp, flags) pool_get((struct pool *)(chpp), flags)
#define	CHFS_POOL_PUT(chpp, v) pool_put((struct pool *)(chpp), v)

/* --------------------------------------------------------------------- */

/*
 * Functions to manipulate a chfs_str_pool.
 */

void	chfs_str_pool_init(struct chfs_str_pool *, struct chfs_mount *);
void	chfs_str_pool_destroy(struct chfs_str_pool *);
char *	chfs_str_pool_get(struct chfs_str_pool *, size_t, int);
void	chfs_str_pool_put(struct chfs_str_pool *, char *, size_t);

#endif

#endif /* _FS_CHFS_CHFS_POOL_H_ */

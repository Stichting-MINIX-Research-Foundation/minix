/*	$NetBSD: buf.h,v 1.13 2018/09/03 16:29:37 riastradh Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FFS_BUF_H
#define	_FFS_BUF_H

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include <err.h>

struct componentname {
	char *cn_nameptr;
	size_t cn_namelen;
};

struct makefs_fsinfo;
struct vnode {
	struct makefs_fsinfo *fs;
	void *v_data;
};

#define vput(a) ((void)(a))

struct buf {
	void *		b_data;
	long		b_bufsize;
	long		b_bcount;
	daddr_t		b_blkno;
	daddr_t		b_lblkno;
	struct makefs_fsinfo * b_fs;

	TAILQ_ENTRY(buf)	b_tailq;
};

struct kauth_cred;
void		bcleanup(void);
int		bread(struct vnode *, daddr_t, int, int, struct buf **);
void		brelse(struct buf *, int);
int		bwrite(struct buf *);
struct buf *	getblk(struct vnode *, daddr_t, int, int, int);

#define	bdwrite(bp)	bwrite(bp)
#define	clrbuf(bp)	memset((bp)->b_data, 0, (u_int)(bp)->b_bcount)

#define	B_MODIFY	0
#define	BC_AGE		0

#define min(a, b) MIN((a), (b))

static inline unsigned int
uimin(unsigned int a, unsigned int b)
{

	return (a < b ? a : b);
}

static inline unsigned int
uimax(unsigned int a, unsigned int b)
{

	return (a > b ? a : b);
}

static inline void
microtime(struct timeval *tv)
{
	extern struct stat stampst;

	if (stampst.st_ino) {
		tv->tv_sec = stampst.st_mtime;
		tv->tv_usec = 0;
	} else {
	    gettimeofday((tv), NULL);
	}
}

#define KASSERT(a)
#define IO_SYNC	1

struct pool {
	size_t size;
};

#define pool_init(p, s, a1, a2, a3, a4, a5, a6)	(p)->size = (s)
#define pool_get(p, f)	ecalloc(1, (p)->size)
#define pool_put(p, a)	free(a)
#define pool_destroy(p)

#define MALLOC_DECLARE(a)
#define malloc_type_attach(a)
#define malloc_type_detach(a)

#define mutex_enter(m)
#define mutex_exit(m)
#define mutex_init(m, t, i)
#define mutex_destroy(m)

#define desiredvnodes 10000
#define NOCRED NULL
#define DEV_BSHIFT 9

#endif	/* _FFS_BUF_H */

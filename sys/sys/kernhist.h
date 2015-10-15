/*	$NetBSD: kernhist.h,v 1.9 2014/03/30 15:53:37 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: NetBSD: uvm_stat.h,v 1.49 2011/04/23 18:14:13 rmind Exp
 * from: Id: uvm_stat.h,v 1.1.2.4 1998/02/07 01:16:56 chs Exp
 */

#ifndef _SYS_KERNHIST_H_
#define _SYS_KERNHIST_H_

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#include "opt_kernhist.h"
#endif

#include <sys/queue.h>
#ifdef KERNHIST
#include <sys/cpu.h>
#endif

/*
 * kernel history/tracing, was uvm_stat
 */

struct kern_history_ent {
	struct timeval tv; 		/* time stamp */
	int cpunum;
	const char *fmt;		/* printf format */
	size_t fmtlen;			/* length of printf format */
	const char *fn;			/* function name */
	size_t fnlen;			/* length of function name */
	u_long call;			/* function call number */
	u_long v[4];			/* values */
};

struct kern_history {
	const char *name;		/* name of this history */
	size_t namelen;			/* length of name, not including null */
	LIST_ENTRY(kern_history) list;	/* link on list of all histories */
	unsigned int n;			/* number of entries */
	unsigned int f;			/* next free one */
	struct kern_history_ent *e;	/* the allocated entries */
};

LIST_HEAD(kern_history_head, kern_history);

/*
 * grovelling lists all at once.  we currently do not allow more than
 * 32 histories to exist, as the way to dump a number of them at once
 * is by calling kern_hist() with a bitmask.
 *
 * XXX extend this to have a registration function?  however, there
 * needs to be static ones as UVM requires this before almost anything
 * else is setup.
 */

/* this is used to set the size of some arrays */
#define	MAXHISTS		32

/* and these are the bit values of each history */
#define	KERNHIST_UVMMAPHIST	0x00000001	/* maphist */
#define	KERNHIST_UVMPDHIST	0x00000002	/* pdhist */
#define	KERNHIST_UVMUBCHIST	0x00000004	/* ubchist */
#define	KERNHIST_UVMLOANHIST	0x00000008	/* loanhist */

#ifdef _KERNEL

/*
 * macros to use the history/tracing code.  note that KERNHIST_LOG
 * must take 4 arguments (even if they are ignored by the format).
 */
#ifndef KERNHIST
#define KERNHIST_DECL(NAME)
#define KERNHIST_DEFINE(NAME)
#define KERNHIST_INIT(NAME,N)
#define KERNHIST_INIT_STATIC(NAME,BUF)
#define KERNHIST_LOG(NAME,FMT,A,B,C,D)
#define KERNHIST_CALLARGS(NAME,FMT,A,B,C,D)
#define KERNHIST_CALLED(NAME)
#define KERNHIST_FUNC(FNAME)
#define KERNHIST_DUMP(NAME)
#else
#include <sys/kernel.h>		/* for "cold" variable */
#include <sys/atomic.h>
#include <sys/kmem.h>

extern	struct kern_history_head kern_histories;

#define KERNHIST_DECL(NAME) extern struct kern_history NAME
#define KERNHIST_DEFINE(NAME) struct kern_history NAME

#define KERNHIST_INIT(NAME,N) \
do { \
	(NAME).name = __STRING(NAME); \
	(NAME).namelen = strlen(__STRING(NAME)); \
	(NAME).n = (N); \
	(NAME).f = 0; \
	(NAME).e = (struct kern_history_ent *) \
		kmem_zalloc(sizeof(struct kern_history_ent) * (N), KM_SLEEP); \
	LIST_INSERT_HEAD(&kern_histories, &(NAME), list); \
} while (/*CONSTCOND*/ 0)

#define KERNHIST_INITIALIZER(NAME,BUF) \
{ \
	.name = __STRING(NAME), \
	.namelen = sizeof(__STRING(NAME)) - 1, \
	.n = sizeof(BUF) / sizeof(struct kern_history_ent), \
	.f = 0, \
	.e = (struct kern_history_ent *) (BUF), \
	/* BUF will inititalized to zeroes by being in .bss */ \
}

#define KERNHIST_LINK_STATIC(NAME) \
	LIST_INSERT_HEAD(&kern_histories, &(NAME), list)

#define KERNHIST_INIT_STATIC(NAME,BUF) \
do { \
	(NAME).name = __STRING(NAME); \
	(NAME).namelen = strlen(__STRING(NAME)); \
	(NAME).n = sizeof(BUF) / sizeof(struct kern_history_ent); \
	(NAME).f = 0; \
	(NAME).e = (struct kern_history_ent *) (BUF); \
	memset((NAME).e, 0, sizeof(struct kern_history_ent) * (NAME).n); \
	KERNHIST_LINK_STATIC(NAME); \
} while (/*CONSTCOND*/ 0)

#ifndef KERNHIST_DELAY
#define KERNHIST_DELAY	100000
#endif

#if defined(KERNHIST_PRINT)
extern int kernhist_print_enabled;
#define KERNHIST_PRINTNOW(E) \
do { \
		if (kernhist_print_enabled) { \
			kernhist_entry_print(E); \
			if (KERNHIST_DELAY != 0) \
				DELAY(KERNHIST_DELAY); \
		} \
} while (/*CONSTCOND*/ 0)
#else
#define KERNHIST_PRINTNOW(E) /* nothing */
#endif

#define KERNHIST_LOG(NAME,FMT,A,B,C,D) \
do { \
	unsigned int _i_, _j_; \
	do { \
		_i_ = (NAME).f; \
		_j_ = (_i_ + 1 < (NAME).n) ? _i_ + 1 : 0; \
	} while (atomic_cas_uint(&(NAME).f, _i_, _j_) != _i_); \
	struct kern_history_ent * const _e_ = &(NAME).e[_i_]; \
	if (__predict_true(!cold)) \
		microtime(&_e_->tv); \
	_e_->cpunum = cpu_number(); \
	_e_->fmt = (FMT); \
	_e_->fmtlen = strlen(FMT); \
	_e_->fn = _kernhist_name; \
	_e_->fnlen = strlen(_kernhist_name); \
	_e_->call = _kernhist_call; \
	_e_->v[0] = (u_long)(A); \
	_e_->v[1] = (u_long)(B); \
	_e_->v[2] = (u_long)(C); \
	_e_->v[3] = (u_long)(D); \
	KERNHIST_PRINTNOW(_e_); \
} while (/*CONSTCOND*/ 0)

#define KERNHIST_CALLED(NAME) \
do { \
	_kernhist_call = atomic_inc_uint_nv(&_kernhist_cnt); \
	KERNHIST_LOG(NAME, "called!", 0, 0, 0, 0); \
} while (/*CONSTCOND*/ 0)

/*
 * This extends kernhist to avoid wasting a separate "called!" entry on every
 * function.
 */
#define KERNHIST_CALLARGS(NAME, FMT, A, B, C, D) \
do { \
	_kernhist_call = atomic_inc_uint_nv(&_kernhist_cnt); \
	KERNHIST_LOG(NAME, "called: "FMT, (A), (B), (C), (D)); \
} while (/*CONSTCOND*/ 0)

#define KERNHIST_FUNC(FNAME) \
	static unsigned int _kernhist_cnt = 0; \
	static const char *const _kernhist_name = FNAME; \
	unsigned int _kernhist_call = 0;

#ifdef DDB
#define KERNHIST_DUMP(NAME)	kernhist_dump(&NAME)
#else
#define KERNHIST_DUMP(NAME)
#endif


static inline void kernhist_entry_print(const struct kern_history_ent *);

static inline void
kernhist_entry_print(const struct kern_history_ent *e)
{
	printf("%06" PRIu64 ".%06d ", e->tv.tv_sec, e->tv.tv_usec);
	printf("%s#%ld@%d: ", e->fn, e->call, e->cpunum);
	printf(e->fmt, e->v[0], e->v[1], e->v[2], e->v[3]);
	printf("\n");
}

#if defined(DDB)
void	kernhist_dump(struct kern_history *);
void	kernhist_print(void (*)(const char *, ...) __printflike(1, 2));
#endif /* DDB */

#endif /* KERNHIST */

#endif /* _KERNEL */

#endif /* _SYS_KERNHIST_H_ */

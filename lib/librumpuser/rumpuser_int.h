/*	$NetBSD: rumpuser_int.h,v 1.10 2014/07/22 22:41:58 justin Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>

#include <rump/rumpuser.h>

#define seterror(value) do { if (error) *error = value;} while (/*CONSTCOND*/0)

extern struct rumpuser_hyperup rumpuser__hyp;

static inline void
rumpkern_unsched(int *nlocks, void *interlock)
{

	rumpuser__hyp.hyp_backend_unschedule(0, nlocks, interlock);
}

static inline void
rumpkern_sched(int nlocks, void *interlock)
{

	rumpuser__hyp.hyp_backend_schedule(nlocks, interlock);
}

#define KLOCK_WRAP(a)							\
do {									\
	int nlocks;							\
	rumpkern_unsched(&nlocks, NULL);				\
	a;								\
	rumpkern_sched(nlocks, NULL);					\
} while (/*CONSTCOND*/0)

#define DOCALL(rvtype, call)						\
{									\
	rvtype rv;							\
	rv = call;							\
	if (rv == -1)							\
		seterror(errno);					\
	else								\
		seterror(0);						\
	return rv;							\
}

#define DOCALL_KLOCK(rvtype, call)					\
{									\
	rvtype rv;							\
	int nlocks;							\
	rumpkern_unsched(&nlocks, NULL);				\
	rv = call;							\
	rumpkern_sched(nlocks, NULL);					\
	if (rv == -1)							\
		seterror(errno);					\
	else								\
		seterror(0);						\
	return rv;							\
}

void rumpuser__thrinit(void);

#define NOFAIL(a) do {if (!(a)) abort();} while (/*CONSTCOND*/0)

#define NOFAIL_ERRNO(a)							\
do {									\
	int fail_rv = (a);						\
	if (fail_rv) {							\
		printf("panic: rumpuser fatal failure %d (%s)\n",	\
		    fail_rv, strerror(fail_rv));			\
		abort();						\
	}								\
} while (/*CONSTCOND*/0)

int  rumpuser__sig_rump2host(int);
int  rumpuser__errtrans(int);
#ifdef __NetBSD__
#define ET(_v_) return (_v_);
#else
#define ET(_v_) return (_v_) ? rumpuser__errtrans(_v_) : 0;
#endif

int rumpuser__random_init(void);

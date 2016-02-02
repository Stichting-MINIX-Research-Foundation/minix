/*	$NetBSD: rndsource.h,v 1.3 2015/04/21 03:53:07 riastradh Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>.  This code uses ideas and
 * algorithms from the Linux driver written by Ted Ts'o.
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

#ifndef	_SYS_RNDSOURCE_H
#define	_SYS_RNDSOURCE_H

#ifndef _KERNEL			/* XXX */
#error <sys/rndsource.h> is meant for kernel consumers only.
#endif

#include <sys/types.h>
#include <sys/rndio.h>		/* RND_TYPE_*, RND_FLAG_* */
#include <sys/rngtest.h>

typedef struct rnd_delta_estimator {
	uint64_t	x;
	uint64_t	dx;
	uint64_t	d2x;
	uint64_t	insamples;
	uint64_t	outbits;
} rnd_delta_t;

typedef struct krndsource {
	LIST_ENTRY(krndsource) list;	/* the linked list */
        char            name[16];       /* device name */
	rnd_delta_t	time_delta;	/* time delta estimator */
	rnd_delta_t	value_delta;	/* value delta estimator */
        uint32_t        total;          /* entropy from this source */
        uint32_t        type;           /* type */
        uint32_t        flags;          /* flags */
        void            *state;         /* state information */
        size_t          test_cnt;       /* how much test data accumulated? */
	void		(*get)(size_t, void *);	/* pool wants N bytes (badly) */
	void		*getarg;	/* argument to get-function */
	void		(*enable)(struct krndsource *, bool); /* turn on/off */
	rngtest_t	*test;		/* test data for RNG type sources */
	unsigned	refcnt;
} krndsource_t;

static inline void
rndsource_setcb(struct krndsource *const rs, void (*const cb)(size_t, void *),
    void *const arg)
{
	rs->get = cb;
	rs->getarg = arg;
}

static inline void
rndsource_setenable(struct krndsource *const rs, void *const cb)
{
	rs->enable = cb;
}

#define RND_ENABLED(rp) \
        (((rp)->flags & RND_FLAG_NO_COLLECT) == 0)

void		_rnd_add_uint32(krndsource_t *, uint32_t);
void		_rnd_add_uint64(krndsource_t *, uint64_t);
void		rnd_add_data(krndsource_t *, const void *const, uint32_t,
		    uint32_t);
void		rnd_attach_source(krndsource_t *, const char *,
		    uint32_t, uint32_t);
void		rnd_detach_source(krndsource_t *);

static inline void
rnd_add_uint32(krndsource_t *kr, uint32_t val)
{
	if (__predict_true(kr)) {
		if (RND_ENABLED(kr)) {
			_rnd_add_uint32(kr, val);
		}
	} else {
		rnd_add_data(NULL, &val, sizeof(val), 0);
	}
}

static inline void
rnd_add_uint64(krndsource_t *kr, uint64_t val)
{
	if (__predict_true(kr)) {
		if (RND_ENABLED(kr)) {
			_rnd_add_uint64(kr, val);
		}
	} else {
		rnd_add_data(NULL, &val, sizeof(val), 0);
	}
}

#endif	/* _SYS_RNDSOURCE_H */

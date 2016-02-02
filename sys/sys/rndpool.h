/*	$NetBSD: rndpool.h,v 1.3 2015/04/14 13:14:20 riastradh Exp $	*/

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

#ifndef	_SYS_RNDPOOL_H
#define	_SYS_RNDPOOL_H

#ifndef _KERNEL			/* XXX */
#error <sys/rndpool.h> is meant for kernel consumers only.
#endif

#include <sys/types.h>
#include <sys/rndio.h>		/* rndpoolstat_t */

/*
 * Size of entropy pool in 32-bit words.  This _MUST_ be a power of 2.  Don't
 * change this unless you really know what you are doing...
 */
#ifndef RND_POOLWORDS
#define RND_POOLWORDS	128
#endif
#define RND_POOLBITS	(RND_POOLWORDS * 32)

typedef struct {
        uint32_t        cursor;         /* current add point in the pool */
        uint32_t        rotate;         /* how many bits to rotate by */
        rndpoolstat_t   stats;          /* current statistics */
        uint32_t        pool[RND_POOLWORDS]; /* random pool data */
} rndpool_t;

/* Mode for rnd_extract_data.  */
#define RND_EXTRACT_ANY		0  /* extract as many bits as requested */
#define RND_EXTRACT_GOOD	1  /* extract as many bits as we have counted
				    * entropy */

void		rndpool_init(rndpool_t *);
uint32_t	rndpool_get_entropy_count(rndpool_t *);
void		rndpool_set_entropy_count(rndpool_t *, uint32_t);
void		rndpool_get_stats(rndpool_t *, void *, int);
void		rndpool_add_data(rndpool_t *,
				 const void *const , uint32_t, uint32_t);
uint32_t	rndpool_extract_data(rndpool_t *, void *, uint32_t, uint32_t);

#endif	/* _SYS_RNDPOOL_H */

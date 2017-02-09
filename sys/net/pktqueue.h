/*	$NetBSD: pktqueue.h,v 1.4 2014/06/16 00:40:10 ozaki-r Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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

#ifndef _NET_PKTQUEUE_H_
#define _NET_PKTQUEUE_H_

#if !defined(_KERNEL)
#error "not supposed to be exposed to userland."
#endif

#include <sys/sysctl.h>

struct mbuf;

typedef struct pktqueue pktqueue_t;

typedef enum { PKTQ_MAXLEN, PKTQ_NITEMS, PKTQ_DROPS } pktq_count_t;

pktqueue_t *	pktq_create(size_t, void (*)(void *), void *);
void		pktq_destroy(pktqueue_t *);

bool		pktq_enqueue(pktqueue_t *, struct mbuf *, const u_int);
struct mbuf *	pktq_dequeue(pktqueue_t *);
void		pktq_barrier(pktqueue_t *);
void		pktq_flush(pktqueue_t *);
int		pktq_set_maxlen(pktqueue_t *, size_t);

uint32_t	pktq_rps_hash(const struct mbuf *);
uint64_t	pktq_get_count(pktqueue_t *, pktq_count_t);

int		sysctl_pktq_maxlen(SYSCTLFN_PROTO, pktqueue_t *);
int		sysctl_pktq_count(SYSCTLFN_PROTO, pktqueue_t *, u_int);

#endif

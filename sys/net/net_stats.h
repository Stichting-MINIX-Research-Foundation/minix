/*	$NetBSD: net_stats.h,v 1.4 2014/09/05 06:01:24 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _NET_NET_STATS_H_
#define _NET_NET_STATS_H_

#ifdef _KERNEL
#include <sys/percpu.h>

#define	_NET_STAT_GETREF(stat)	((uint64_t *)percpu_getref((stat)))
#define	_NET_STAT_PUTREF(stat)	percpu_putref((stat))

#define	_NET_STATINC(stat, x)						\
do {									\
	uint64_t *_stat_ = _NET_STAT_GETREF(stat);			\
	_stat_[x]++;							\
	_NET_STAT_PUTREF(stat);						\
} while (/*CONSTCOND*/0)

#define	_NET_STATDEC(stat, x)						\
do {									\
	uint64_t *_stat_ = _NET_STAT_GETREF(stat);			\
	_stat_[x]--;							\
	_NET_STAT_PUTREF(stat);						\
} while (/*CONSTCOND*/0)

#define	_NET_STATADD(stat, x, v)					\
do {									\
	uint64_t *_stat_ = _NET_STAT_GETREF(stat);			\
	_stat_[x] += (v);						\
	_NET_STAT_PUTREF(stat);						\
} while (/*CONSTCOND*/0)

#define	_NET_STATSUB(stat, x, v)					\
do {									\
	uint64_t *_stat_ = _NET_STAT_GETREF(stat);			\
	_stat_[x] -= (v);						\
	_NET_STAT_PUTREF(stat);						\
} while (/*CONSTCOND*/0)

__BEGIN_DECLS
struct lwp;
struct sysctlnode;

int	netstat_sysctl(percpu_t *, u_int,
		       const int *, u_int, void *,
		       size_t *, const void *, size_t,
		       const int *, struct lwp *, const struct sysctlnode *);

#define	NETSTAT_SYSCTL(stat, nctrs)					\
	netstat_sysctl((stat), (nctrs), name, namelen, oldp, oldlenp,	\
		       newp, newlen, oname, l, rnode)
__END_DECLS
#endif /* _KERNEL */

#endif /* !_NET_NET_STATS_H_ */

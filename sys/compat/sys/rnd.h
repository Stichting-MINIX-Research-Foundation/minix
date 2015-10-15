/*	$NetBSD: rnd.h,v 1.4 2015/04/14 12:19:57 riastradh Exp $	*/

/*-
 * Copyright (c) 1997,2011 The NetBSD Foundation, Inc.
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

#ifndef _COMPAT_SYS_RND_H_
#define	_COMPAT_SYS_RND_H_

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#endif

#include <sys/types.h>
#include <sys/ioctl.h>

#ifdef COMPAT_NETBSD32
#include <compat/netbsd32/netbsd32.h>
#endif /* COMPAT_NETBSD32 */

#include <sys/rndio.h>

/*
 * NetBSD-5 used "void *state" in the rndsource_t struct.  rndsource_t
 * was used in rnstat_t and rnstat_name_t, which were used by
 * the NetBSD-5 RNDGETSRCNUM and RNDGETSRCNAME ioctls.
 *
 */

/* Sanitized random source view for userspace. */
typedef struct {
	char		name[16];	/* device name */
	uint32_t	unused_time;	/* was: last time recorded */
	uint32_t	unused_delta;	/* was: last delta value */
	uint32_t	unused_delta2;	/* was: last delta2 value */
	uint32_t	total;		/* entropy from this source */
	uint32_t	type;		/* type */
	uint32_t	flags;		/* flags */
	void		*unused_state;	/* was: internal state */
} rndsource50_t;

#ifdef COMPAT_NETBSD32
typedef struct {
	char		name[16];	/* device name */
	uint32_t	unused_time;	/* was: last time recorded */
	uint32_t	unused_delta;	/* was: last delta value */
	uint32_t	unused_delta2;	/* was: last delta2 value */
	uint32_t	total;		/* entropy from this source */
	uint32_t	type;		/* type */
	uint32_t	flags;		/* flags */
	netbsd32_voidp	unused_state;	/* was: internal state */
} rndsource50_32_t;
#endif /* COMPAT_NETBSD32 */

/*
 * NetBSD-5 defined RND_MAXSTATCOUNT as 10.  We define RND_MAXSTATCOUNT50
 * here, and check that the native RND_MAXSTATCOUNT is not smaller.
 */
#define	RND_MAXSTATCOUNT50	10	/* 10 sources at once max */
#if (RND_MAXSTATCOUNT50 > RND_MAXSTATCOUNT)
 #error "RND_MAXSTATCOUNT50 is too large"
#endif

/*
 * return "count" random entries, starting at "start"
 */
typedef struct {
	uint32_t	start;
	uint32_t	count;
	rndsource50_t source[RND_MAXSTATCOUNT50];
} rndstat50_t;

#ifdef COMPAT_NETBSD32
typedef struct {
	uint32_t	start;
	uint32_t	count;
	rndsource50_32_t source[RND_MAXSTATCOUNT50];
} rndstat50_32_t;
#endif /* COMPAT_NETBSD32 */

/*
 * return information on a specific source by name
 */
typedef struct {
	char		name[16];
	rndsource50_t source;
} rndstat_name50_t;

#ifdef COMPAT_NETBSD32
typedef struct {
	char		name[16];
	rndsource50_32_t source;
} rndstat_name50_32_t;
#endif /* COMPAT_NETBSD32 */

/*
 * NetBSD-5 defined RND_POOLWORDS as 128.  In NetBSD-6, the value
 * exposed to userland via the rnddata_t type was renamed to
 * RND_SAVEWORDS.  As long as RND_SAVEWORDS remains equal to 128, then
 * rnddata_t (used by ioctl RNDADDATA), and rndpoolstat_t (used by ioctl
 * RNDGETPOOLSTAT) remain ABI compatible without any extra effort, even
 * though the declarations in the source code have changed.
 */
#if (RND_SAVEWORDS != 128)
 #error "RND_SAVEWORDS must be 128 for NetBSD-5 compatibility"
#endif

/*
 * Compatibility with NetBSD-5 ioctls.
 */
#ifdef _KERNEL
int compat_50_rnd_ioctl(struct file *, u_long, void *);
#endif

#define	RNDGETSRCNUM50		_IOWR('R', 102, rndstat50_t)
#define	RNDGETSRCNAME50		_IOWR('R', 103, rndstat_name50_t)

#ifdef COMPAT_NETBSD32
#define	RNDGETSRCNUM50_32	_IOWR('R', 102, rndstat50_32_t)
#define	RNDGETSRCNAME50_32	_IOWR('R', 103, rndstat_name50_32_t)
#endif /* COMPAT_NETBSD32 */

#endif /* !_COMPAT_SYS_RND_H_ */

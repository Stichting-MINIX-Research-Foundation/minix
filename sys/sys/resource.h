/*	$NetBSD: resource.h,v 1.33 2012/06/09 02:31:15 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)resource.h	8.4 (Berkeley) 1/9/95
 */

#ifndef _SYS_RESOURCE_H_
#define	_SYS_RESOURCE_H_

#include <sys/featuretest.h>
#include <sys/time.h>

/*
 * Process priority specifications to get/setpriority.
 */
#define	PRIO_MIN	-20
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Resource limits
 */
#define RLIMIT_CORE	1
#define RLIMIT_CPU	2
#define RLIMIT_DATA	3
#define RLIMIT_FSIZE	4
#define RLIMIT_NOFILE	5
#define RLIMIT_STACK	6
#define RLIMIT_AS	7
#define	RLIMIT_VMEM	RLIMIT_AS	/* common alias */

#if defined(_NETBSD_SOURCE)
#define	RLIM_NLIMITS	8		/* number of resource limits */
#endif

#define RLIM_INFINITY ((rlim_t) -1)
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

struct rlimit
{
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

#include <sys/cdefs.h>

__BEGIN_DECLS
int	getpriority(int, int);
int	getrlimit(int, struct rlimit *);
int	setpriority(int, int, int);
__END_DECLS

#endif	/* !_SYS_RESOURCE_H_ */

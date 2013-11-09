/*	$NetBSD: kernel.h,v 1.28 2009/01/11 02:45:55 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kernel.h	8.3 (Berkeley) 1/21/94
 */

#ifndef _SYS_KERNEL_H_
#define _SYS_KERNEL_H_

#if defined(_KERNEL) || defined(_STANDALONE)
/* Global variables for the kernel. */

extern long hostid;
extern char hostname[MAXHOSTNAMELEN];
extern int hostnamelen;
extern char domainname[MAXHOSTNAMELEN];
extern int domainnamelen;

extern struct timespec boottime;

extern int rtc_offset;		/* offset of rtc from UTC in minutes */

extern int cold;		/* still working on startup */
extern int tick;		/* usec per tick (1000000 / hz) */
extern int tickadj;		/* "standard" clock skew, us./tick */
extern int hardclock_ticks;	/* # of hardclock ticks */
extern int hz;			/* system clock's frequency */
extern int stathz;		/* statistics clock's frequency */
extern int profhz;		/* profiling clock's frequency */

extern int profsrc;		/* profiling source */

#define PROFSRC_CLOCK	0

#endif

#endif /* _SYS_KERNEL_H_ */

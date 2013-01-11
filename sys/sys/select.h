/*	$NetBSD: select.h,v 1.36 2009/11/11 09:48:51 rmind Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)select.h	8.2 (Berkeley) 1/4/94
 */

#ifndef _SYS_SELECT_H_
#define	_SYS_SELECT_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/fd_set.h>

#ifdef _KERNEL
#include <sys/selinfo.h>		/* for struct selinfo */
#include <sys/signal.h>			/* for sigset_t */

struct lwp;
struct proc;
struct timespec;
struct cpu_info;
struct socket;

int	selcommon(register_t *, int, fd_set *, fd_set *, fd_set *,
    struct timespec *, sigset_t *);
void	selrecord(struct lwp *selector, struct selinfo *);
void	selnotify(struct selinfo *, int, long);
void	selsysinit(struct cpu_info *);
void	selinit(struct selinfo *);
void	seldestroy(struct selinfo *);
int	pollsock(struct socket *, const struct timespec *, int);

#else /* _KERNEL */

#include <sys/sigtypes.h>
#include <time.h>

__BEGIN_DECLS
#ifndef __LIBC12_SOURCE__
#ifndef __minix
int	pselect(int, fd_set * __restrict, fd_set * __restrict,
    fd_set * __restrict, const struct timespec * __restrict,
    const sigset_t * __restrict) __RENAME(__pselect50);
#endif /* !__minix */
int	select(int, fd_set * __restrict, fd_set * __restrict,
    fd_set * __restrict, struct timeval * __restrict) __RENAME(__select50);
#endif /* __LIBC12_SOURCE__ */
__END_DECLS
#endif /* _KERNEL */

#ifdef _MINIX
/* possible select() operation types; read, write, errors */
/* (FS/driver internal use only) */
#define SEL_RD		(1 << 0)
#define SEL_WR		(1 << 1)
#define SEL_ERR		(1 << 2)
#define SEL_NOTIFY	(1 << 3) /* not a real select operation */
#endif

#endif /* !_SYS_SELECT_H_ */

/*	$NetBSD: socket.h,v 1.12 2009/02/13 22:41:04 apb Exp $	*/

/*
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
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
 *	@(#)socket.h	8.6 (Berkeley) 5/3/95
 */

#ifndef _COMPAT_SYS_SOCKET_H_
#define	_COMPAT_SYS_SOCKET_H_

#ifdef _KERNEL_OPT

#include "opt_compat_linux.h"
#include "opt_compat_svr4.h"
#include "opt_compat_ultrix.h"
#include "opt_compat_43.h"
#include "opt_modular.h"

#if defined(COMPAT_43) || defined(COMPAT_LINUX) || defined(COMPAT_SVR4) || \
    defined(COMPAT_ULTRIX) || defined(MODULAR)
#define COMPAT_OSOCK
#endif

#else
#define COMPAT_OSOCK
#endif

/*
 * 4.3 compat sockaddr
 */
struct osockaddr {
	uint16_t	sa_family;	/* address family */
	char		sa_data[14];	/* up to 14 bytes of direct address */
};

/*
 * 4.3-compat message header
 */
struct omsghdr {
	void *		msg_name;	/* optional address */
	int		msg_namelen;	/* size of address */
	struct iovec	*msg_iov;	/* scatter/gather array */
	int		msg_iovlen;	/* # elements in msg_iov */
	void *		msg_accrights;	/* access rights sent/received */
	int		msg_accrightslen;
};

#ifdef _KERNEL

#define	SO_OSNDTIMEO	0x1005
#define	SO_ORCVTIMEO	0x1006
#define	SO_OTIMESTAMP	0x0400
#define	SCM_OTIMESTAMP	0x2

__BEGIN_DECLS
struct socket;
struct proc;
u_long compat_cvtcmd(u_long cmd);
int compat_ifioctl(struct socket *, u_long, u_long, void *, struct lwp *);
int compat43_set_accrights(struct msghdr *, void *, int);
__END_DECLS
#else
int	__socket30(int, int, int);
#endif

#endif /* !_COMPAT_SYS_SOCKET_H_ */

/*	$NetBSD: ttycom.h,v 1.2 2012/10/19 17:16:55 apb Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alan Barrett
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

#ifndef	_COMPAT_SYS_TTYCOM_H_
#define	_COMPAT_SYS_TTYCOM_H_

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#endif

#include <sys/types.h>
#include <sys/ioctl.h>

#ifdef COMPAT_60

/*
 * The cn and sn fields in struct ptmget changed size from
 * char[16] to char[PATH_MAX] in NetBSD-6.99.14.
 */
struct compat_60_ptmget {
	int	cfd;
	int	sfd;
	char	cn[16];
	char	sn[16];
};

#define COMPAT_60_TIOCPTMGET 	 _IOR('t', 70, struct compat_60_ptmget)
#define COMPAT_60_TIOCPTSNAME 	 _IOR('t', 72, struct compat_60_ptmget)

#ifdef _KERNEL
int compat_60_ttioctl(struct tty *, u_long, void *, int, struct lwp *);
int compat_60_ptmioctl(dev_t, u_long, void *, int, struct lwp *);
#endif

#endif /* COMPAT_60 */

#endif /* !_COMPAT_SYS_TTYCOM_H_ */

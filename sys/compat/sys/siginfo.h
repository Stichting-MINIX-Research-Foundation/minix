/*	$NetBSD: siginfo.h,v 1.4 2008/04/28 20:23:46 martin Exp $	 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_COMPAT_SYS_SIGINFO_H_
#define	_COMPAT_SYS_SIGINFO_H_

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd32.h"
#endif

#if defined(COMPAT_NETBSD32) && defined(_KERNEL)

typedef union sigval32 {
	int sival_int;
	uint32_t sival_ptr;
} sigval32_t;

struct __ksiginfo32 {
	int	_signo;
	int	_code;
	int	_errno;

	union {
		struct {
			pid_t _pid;
			uid_t _uid;
			sigval32_t _value;
		} _rt;

		struct {
			pid_t _pid;
			uid_t _uid;
			int _status;
			clock_t _utime;
			clock_t _stime;
		} _child;

		struct {
			uint32_t _addr;
			int _trap;
		} _fault;

		struct {
			int32_t _band;
			int _fd;
		} _poll;
	} _reason;
};

typedef union siginfo32 {
	char	si_pad[128];
	struct __ksiginfo32 _info;
} siginfo32_t;

#endif /* COMPAT_NETBSD32 && _KERNEL */

#endif /* !_COMPAT_SYS_SIGINFO_H_ */

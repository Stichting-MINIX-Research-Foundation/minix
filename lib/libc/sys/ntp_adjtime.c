/*	$NetBSD: ntp_adjtime.c,v 1.13 2012/03/20 16:26:12 matt Exp $ */

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.      
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ntp_adjtime.c,v 1.13 2012/03/20 16:26:12 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <sys/clockctl.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(ntp_adjtime,_ntp_adjtime)
#endif

extern int __clockctl_fd;

int __ntp_adjtime(struct timex *);

int
ntp_adjtime(struct timex *tp)
{
	struct clockctl_ntp_adjtime args;
	int error;
	int rv;

	/*
	 * we always attempt to use the syscall unless we had to
	 * use the clockctl device before
	 *
	 * ntp_adjtime() is callable for mortals if tp->modes == 0 !
	 */
	if (__clockctl_fd == -1) {
		rv = __ntp_adjtime(tp);
	
		/*
		 * if we fail with EPERM we try the clockctl device
		 */
		if (rv != -1 || errno != EPERM)
			return rv;

		/*
		 * If this fails, it means that we are not root
		 * and we cannot open clockctl. This is a true
		 * failure.
		 */
		__clockctl_fd = open(_PATH_CLOCKCTL, O_WRONLY | O_CLOEXEC, 0);
		if (__clockctl_fd == -1) {
			/* original error was EPERM - don't leak open errors */
			errno = EPERM;
			return -1;
		}
	}

	/*
	 * If __clockctl_fd >=0, clockctl has already been open
	 * and used, so we carry on using it.
	 */
	args.tp = tp;
	error = ioctl(__clockctl_fd, CLOCKCTL_NTP_ADJTIME, &args);

	/*
	 * There is no way to get retval set through ioctl(), hence we
	 * have to handle it here, using args.retval
	 */
	if (error == 0) {
		rv = (int)args.retval;
		return rv;
	}

	return error;
}

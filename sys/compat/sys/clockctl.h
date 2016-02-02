/*      $NetBSD: clockctl.h,v 1.4 2015/09/06 06:00:59 dholland Exp $ */

/*-
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef _COMPAT_SYS_CLOCKCTL_H_
#define _COMPAT_SYS_CLOCKCTL_H_

#include <sys/ioccom.h>
#include <sys/time.h>
#include <sys/timex.h>

struct clockctl50_settimeofday {
	const struct timeval50 *tv;
	const void *tzp;
};

#define CLOCKCTL_OSETTIMEOFDAY _IOW('C', 0x1, struct clockctl50_settimeofday)

struct clockctl50_adjtime {
	const struct timeval50 *delta;
	struct timeval50 *olddelta;
};

#define CLOCKCTL_OADJTIME _IOWR('C', 0x2, struct clockctl50_adjtime)

struct clockctl50_clock_settime {
	clockid_t clock_id;
	const struct timespec50 *tp;
};

#define CLOCKCTL_OCLOCK_SETTIME _IOW('C', 0x3, struct clockctl50_clock_settime)

struct clockctl50_ntp_adjtime {
	struct timex *tp;
	register_t retval;
};

#define CLOCKCTL_ONTP_ADJTIME _IOWR('C', 0x4, struct clockctl50_ntp_adjtime)

#ifdef _KERNEL
struct lwp;
int compat50_clockctlioctl(dev_t, u_long, void *, int, struct lwp *);
#endif

#endif /* _COMPAT_SYS_CLOCKCTL_H_ */

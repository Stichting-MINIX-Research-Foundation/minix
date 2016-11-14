/*	$NetBSD: kern_time_30.c,v 1.4 2011/01/19 10:21:16 tsutsui Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_time_30.c,v 1.4 2011/01/19 10:21:16 tsutsui Exp $");

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timex.h>

#include <compat/common/compat_util.h>
#include <compat/sys/time.h>
#include <compat/sys/timex.h>

#include <sys/syscallargs.h>

int
compat_30_sys_ntp_gettime(struct lwp *l,
    const struct compat_30_sys_ntp_gettime_args *uap, register_t *retval)
{
#ifdef NTP
	/* {
		syscallarg(struct ntptimeval30 *) ontvp;
	} */
	struct ntptimeval ntv;
	struct ntptimeval30 ntv30;
	struct timeval tv;
	int error;

	if (SCARG(uap, ntvp)) {
		ntp_gettime(&ntv);
		TIMESPEC_TO_TIMEVAL(&tv, &ntv.time);
		timeval_to_timeval50(&tv, &ntv30.time);
		ntv30.maxerror = ntv.maxerror;
		ntv30.esterror = ntv.esterror;

		error = copyout(&ntv30, SCARG(uap, ntvp), sizeof(ntv30));
		if (error)
			return error;
 	}
	*retval = ntp_timestatus();
	return 0;
#else
	return ENOSYS;
#endif
}

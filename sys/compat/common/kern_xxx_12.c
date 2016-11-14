/*	$NetBSD: kern_xxx_12.c,v 1.15 2011/01/19 10:21:16 tsutsui Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_xxx.c	8.2 (Berkeley) 11/14/93
 * from NetBSD: kern_xxx.c,v 1.32 1996/04/22 01:38:41 christos Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_xxx_12.c,v 1.15 2011/01/19 10:21:16 tsutsui Exp $");

/*#ifdef COMPAT_12*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>

/* ARGSUSED */
int
compat_12_sys_reboot(struct lwp *l, const struct compat_12_sys_reboot_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) opt;
	} */
	int error;

	if ((error = kauth_authorize_system(l->l_cred,
	    KAUTH_SYSTEM_REBOOT, 0, NULL, NULL, NULL)) != 0)
		return (error);
	KERNEL_LOCK(1, NULL);
	cpu_reboot(SCARG(uap, opt), NULL);
	KERNEL_UNLOCK_ONE(NULL);
	return (0);
}
/*#endif COMPAT_12 */

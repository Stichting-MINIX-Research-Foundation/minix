/*	$NetBSD: vm_12.c,v 1.20 2011/01/19 10:21:16 tsutsui Exp $	*/

/*
 * Copyright (c) 1997 Matthew R. Green
 * All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: vm_12.c,v 1.20 2011/01/19 10:21:16 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syscallargs.h>

#include <sys/swap.h>
#include <sys/mman.h>

int
compat_12_sys_swapon(struct lwp *l, const struct compat_12_sys_swapon_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) name;
	} */
	struct sys_swapctl_args ua;

	SCARG(&ua, cmd) = SWAP_ON;
	/*XXXUNCONST*/
	SCARG(&ua, arg) = __UNCONST(SCARG(uap, name));
	SCARG(&ua, misc) = 0;	/* priority */
	return (sys_swapctl(l, &ua, retval));
}

int
compat_12_sys_msync(struct lwp *l, const struct compat_12_sys_msync_args *uap, register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */
	struct sys___msync13_args ua;

	SCARG(&ua, addr) = SCARG(uap, addr);
	SCARG(&ua, len) = SCARG(uap, len);
	SCARG(&ua, flags) = MS_SYNC | MS_INVALIDATE;
	return (sys___msync13(l, &ua, retval));
}

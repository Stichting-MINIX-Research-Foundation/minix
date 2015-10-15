/*	$NetBSD: ibcs2_machdep.h,v 1.17 2009/12/10 14:13:50 matt Exp $	*/

/*-
 * Copyright (c) 1997, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#ifndef _I386_IBCS2_MACHDEP_H_
#define _I386_IBCS2_MACHDEP_H_

#define COFF_MAGIC_I386	0x14c
#define	COFF_BADMAG(ex)	(ex->f_magic != COFF_MAGIC_I386)
#define	COFF_LDPGSZ	4096

#ifdef _KERNEL
struct exec_package;
struct exec_vmcmd;

void	ibcs2_setregs(struct lwp *, struct exec_package *, vaddr_t);
struct	ibcs2_sys_sysmachine_args;
int	ibcs2_sys_sysmachine(struct lwp *, const struct ibcs2_sys_sysmachine_args *, register_t *retval);

void	ibcs2_syscall_intern(struct proc *);
#endif /* _KERNEL */

#endif /* !_I386_IBCS2_MACHDEP_H_ */

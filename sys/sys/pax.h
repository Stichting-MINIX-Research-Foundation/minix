/* $NetBSD: pax.h,v 1.11 2007/12/27 15:21:53 elad Exp $ */

/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
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
 */

#ifndef _SYS_PAX_H_
#define _SYS_PAX_H_

#include <uvm/uvm_extern.h>

struct lwp;
struct exec_package;
struct vmspace;

#ifdef PAX_ASLR
/*
 * We stick this here because we need it in kern/exec_elf32.c for now.
 */
#ifndef PAX_ASLR_DELTA_EXEC_LEN
#define	PAX_ASLR_DELTA_EXEC_LEN	12
#endif
#endif /* PAX_ASLR */

void pax_init(void);
void pax_adjust(struct lwp *, uint32_t);

void pax_mprotect(struct lwp *, vm_prot_t *, vm_prot_t *);
int pax_segvguard(struct lwp *, struct vnode *, const char *, bool);

#define	PAX_ASLR_DELTA(delta, lsb, len)	\
    (((delta) & ((1UL << (len)) - 1)) << (lsb))
bool pax_aslr_active(struct lwp *);
void pax_aslr_init(struct lwp *, struct vmspace *);
void pax_aslr_stack(struct lwp *, struct exec_package *, u_long *);
void pax_aslr(struct lwp *, vaddr_t *, vaddr_t, int);

#endif /* !_SYS_PAX_H_ */

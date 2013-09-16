/*	$NetBSD: prot.h,v 1.2 2008/04/29 06:53:03 martin Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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

#ifndef _SYS_PROT_H_
#define	_SYS_PROT_H_

/*
 * flags that control when do_setres{u,g}id will do anything
 *
 * ID_XXX_EQ_YYY means
 * "allow modifying XXX uid to the given value if the new value of
 * XXX uid (or gid) equals the current value of YYY uid (or gid)."
 */

#define	ID_E_EQ_E	0x001		/* effective equals effective */
#define	ID_E_EQ_R	0x002		/* effective equals real */
#define	ID_E_EQ_S	0x004		/* effective equals saved */
#define	ID_R_EQ_E	0x010		/* real equals effective */
#define	ID_R_EQ_R	0x020		/* real equals real */
#define	ID_R_EQ_S	0x040		/* real equals saved */
#define	ID_S_EQ_E	0x100		/* saved equals effective */
#define	ID_S_EQ_R	0x200		/* saved equals real */
#define	ID_S_EQ_S	0x400		/* saved equals saved */

int do_setresuid(struct lwp *, uid_t, uid_t, uid_t, u_int);
int do_setresgid(struct lwp *, gid_t, gid_t, gid_t, u_int);

#endif /* !_SYS_PROT_H_ */

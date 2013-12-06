/*	$NetBSD: signalvar.h,v 1.2 2005/12/11 12:20:29 christos Exp $	*/

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)signalvar.h	8.6 (Berkeley) 2/19/95
 */

#ifndef	_COMPAT_SYS_SIGNALVAR_H_
#define	_COMPAT_SYS_SIGNALVAR_H_

#ifdef _KERNEL
/*
 * Compatibility functions.  See compat/common/kern_sig_13.c.
 */
void	native_sigset13_to_sigset(const sigset13_t *, sigset_t *);
void	native_sigset_to_sigset13(const sigset_t *, sigset13_t *);
void	native_sigaction13_to_sigaction(const struct sigaction13 *,
	    struct sigaction *);
void	native_sigaction_to_sigaction13(const struct sigaction *,
	    struct sigaction13 *);
void	native_sigaltstack13_to_sigaltstack(const struct sigaltstack13 *,
	    struct sigaltstack *);
void	native_sigaltstack_to_sigaltstack13(const struct sigaltstack *,
	    struct sigaltstack13 *);
#endif	/* _KERNEL */

#endif	/* !_COMPAT_SYS_SIGNALVAR_H_ */

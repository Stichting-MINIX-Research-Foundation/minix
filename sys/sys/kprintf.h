/*	$NetBSD: kprintf.h,v 1.11 2011/07/17 20:54:54 joerg Exp $	*/

/*-
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

#ifndef _SYS_KPRINTF_H_
#define	_SYS_KPRINTF_H_

#include <sys/stdarg.h>

/*
 * Implementation internals of the kernel printf.  Exposing them here
 * allows other subsystems to implement precisely the printf semantics
 * they need.
 */

/* max size buffer kprintf needs to print quad_t [size in base 8 + \0] */
#define KPRINTF_BUFSIZE         (sizeof(quad_t) * NBBY / 3 + 2)

/* flags for kprintf */
#define	TOCONS		0x0001	/* to the console */
#define	TOTTY		0x0002	/* to the process' tty */
#define	TOLOG		0x0004	/* to the kernel message buffer */
#define	TOBUFONLY	0x0008	/* to the buffer (only) [for snprintf] */
#define	TODDB		0x0010	/* to ddb console */
#define	NOLOCK		0x1000	/* don't acquire a tty lock */

void	kprintf_init(void);
void	kprintf_lock(void);
void	kprintf_unlock(void);
/*
 * NOTE: the kprintf mutex must be held when these functions are called!
 */
int	kprintf(const char *, int, void *, char *, va_list);
void	klogpri(int);

#endif /* _SYS_KPRINTF_H_ */

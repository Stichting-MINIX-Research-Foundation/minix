/*	$NetBSD: tcsendbreak.c,v 1.10 2012/06/25 22:32:46 abs Exp $	*/

/*-
 * Copyright (c) 1989, 1993
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)termios.c	8.2 (Berkeley) 2/21/94";
#else
__RCSID("$NetBSD: tcsendbreak.c,v 1.10 2012/06/25 22:32:46 abs Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#ifndef __minix
#include <sys/time.h>
#endif /* !__minix */

#include <assert.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(tcsendbreak,_tcsendbreak)
#endif

/*ARGSUSED*/
int
tcsendbreak(int fd, int len)
{
#ifdef __minix
	_DIAGASSERT(fd != -1);
	return ioctl(fd, TCSBRK, &len);
#else /* !__minix */
	static const struct timespec sleepytime = { 0, 400000000 };

	_DIAGASSERT(fd != -1);

	if (ioctl(fd, TIOCSBRK, 0) == -1)
		return (-1);
	(void)nanosleep(&sleepytime, NULL);
	if (ioctl(fd, TIOCCBRK, 0) == -1)
		return (-1);
	return (0);
#endif /* !__minix */
}

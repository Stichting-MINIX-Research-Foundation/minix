/*	$NetBSD: stack_protector.c,v 1.8 2012/03/13 21:13:39 christos Exp $	*/
/*	$OpenBSD: stack_protector.c,v 1.10 2006/03/31 05:34:44 deraadt Exp $	*/

/*
 * Copyright (c) 2002 Hiroaki Etoh, Federico G. Schwindt, and Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: stack_protector.c,v 1.8 2012/03/13 21:13:39 christos Exp $");

#ifdef _LIBC
#include "namespace.h"
#endif
#include <sys/param.h>
#include <sys/sysctl.h>
#include <ssp/ssp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#ifdef _LIBC
#include <syslog.h>
#include "extern.h"
#else
#define __sysctl sysctl
void xprintf(const char *fmt, ...);
#include <stdlib.h>
#endif

long __stack_chk_guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static void __fail(const char *) __attribute__((__noreturn__));
__dead void __stack_chk_fail_local(void);
void __guard_setup(void);

void
__guard_setup(void)
{
#ifndef __minix
	static const int mib[2] = { CTL_KERN, KERN_ARND };
	size_t len;
#endif

	if (__stack_chk_guard[0] != 0)
		return;

#ifndef __minix
	len = sizeof(__stack_chk_guard);
	if (__sysctl(mib, (u_int)__arraycount(mib), __stack_chk_guard, &len,
	    NULL, 0) == -1 || len != sizeof(__stack_chk_guard)) {
#endif
		/* If sysctl was unsuccessful, use the "terminator canary". */
		((unsigned char *)(void *)__stack_chk_guard)[0] = 0;
		((unsigned char *)(void *)__stack_chk_guard)[1] = 0;
		((unsigned char *)(void *)__stack_chk_guard)[2] = '\n';
		((unsigned char *)(void *)__stack_chk_guard)[3] = 255;
#ifndef __minix
	}
#endif
}

/*ARGSUSED*/
static void
__fail(const char *msg)
{
#ifdef _LIBC
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	struct sigaction sa;
#endif
	sigset_t mask;

	/* Immediately block all signal handlers from running code */
	(void)sigfillset(&mask);
	(void)sigdelset(&mask, SIGABRT);
	(void)sigprocmask(SIG_BLOCK, &mask, NULL);

#ifdef _LIBC
	/* This may fail on a chroot jail... */
	syslog_ss(LOG_CRIT, &sdata, "%s", msg);
#else
	xprintf("%s: %s\n", getprogname(), msg);
#endif

#ifdef _LIBC
	(void)memset(&sa, 0, sizeof(sa));
	(void)sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;
	(void)sigaction(SIGABRT, &sa, NULL);
	(void)raise(SIGABRT);
#endif
	_exit(127);
}

void
__stack_chk_fail(void)
{
	__fail("stack overflow detected; terminated");
}

void
__chk_fail(void)
{
	__fail("buffer overflow detected; terminated");
}

void
__stack_chk_fail_local(void)
{
	__stack_chk_fail();
}

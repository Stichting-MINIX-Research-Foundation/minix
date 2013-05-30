/*	$NetBSD: logwtmp.c,v 1.25 2006/09/23 16:03:50 xtraeme Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
 */


#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)logwtmp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: logwtmp.c,v 1.25 2006/09/23 16:03:50 xtraeme Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif
#include <util.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"

#ifdef SUPPORT_UTMP
static int fd = -1;

void
ftpd_initwtmp(void)
{
	const char *wf = _PATH_WTMP;
	if ((fd = open(wf, O_WRONLY|O_APPEND, 0)) == -1)
		syslog(LOG_ERR, "Cannot open `%s' (%m)", wf);
}

/*
 * Modified version of logwtmp that holds wtmp file open
 * after first call, for use with ftp (which may chroot
 * after login, but before logout).
 */
void
ftpd_logwtmp(const char *line, const char *name, const char *host)
{
	struct utmp ut;
	struct stat buf;

	if (fd < 0)
		return;
	if (fstat(fd, &buf) == 0) {
		(void)strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void)strncpy(ut.ut_name, name, sizeof(ut.ut_name));
		(void)strncpy(ut.ut_host, host, sizeof(ut.ut_host));
		(void)time(&ut.ut_time);
		if (write(fd, (char *)&ut, sizeof(struct utmp)) !=
		    sizeof(struct utmp))
			(void)ftruncate(fd, buf.st_size);
	}
}
#endif

#ifdef SUPPORT_UTMPX
static int fdx = -1;

void
ftpd_initwtmpx(void)
{
	const char *wf = _PATH_WTMPX;
	if ((fdx = open(wf, O_WRONLY|O_APPEND, 0)) == -1)
		syslog(LOG_ERR, "Cannot open `%s' (%m)", wf);
}

void
ftpd_logwtmpx(const char *line, const char *name, const char *host,
    struct sockinet *haddr, int status, int utx_type)
{
	struct utmpx ut;
	struct stat buf;
	
	if (fdx < 0) 
		return;
	if (fstat(fdx, &buf) == 0) {
		(void)strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void)strncpy(ut.ut_name, name, sizeof(ut.ut_name));
		(void)strncpy(ut.ut_host, host, sizeof(ut.ut_host));
		if (haddr)
			(void)memcpy(&ut.ut_ss, &haddr->si_su, haddr->su_len);
		else
			(void)memset(&ut.ut_ss, 0, sizeof(ut.ut_ss));
		ut.ut_type = utx_type;
		if (WIFEXITED(status))
			ut.ut_exit.e_exit = (uint16_t)WEXITSTATUS(status);
		if (WIFSIGNALED(status))
		ut.ut_exit.e_termination = (uint16_t)WTERMSIG(status);
		(void)gettimeofday(&ut.ut_tv, NULL);
		if(write(fdx, (char *)&ut, sizeof(struct utmpx)) !=
		    sizeof(struct utmpx))
			(void)ftruncate(fdx, buf.st_size);
	}
}
#endif

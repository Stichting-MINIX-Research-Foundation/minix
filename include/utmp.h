/*	$NetBSD: utmp.h,v 1.12 2009/01/11 03:04:12 christos Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
 *
 *	@(#)utmp.h	8.2 (Berkeley) 1/21/94
 */

#ifndef	_UTMP_H_
#define	_UTMP_H_

#ifdef __minix
#define _PATH_UTMP	"/etc/utmp"
#define _PATH_WTMP	"/usr/adm/wtmp"
#define _PATH_BTMP	"/usr/adm/btmp"
#define _PATH_LASTLOG	"/usr/adm/lastlog"
#define UTMP		_PATH_UTMP
#define WTMP		_PATH_WTMP
#define BTMP		_PATH_BTMP
#else
#define	_PATH_UTMP	"/var/run/utmp"
#define	_PATH_WTMP	"/var/log/wtmp"
#define	_PATH_LASTLOG	"/var/log/lastlog"
#endif

#define	UT_NAMESIZE	8
#ifdef __minix
#define UT_LINESIZE	12
#else
#define	UT_LINESIZE	8
#endif
#define	UT_HOSTSIZE	16

struct lastlog {
	time_t	ll_time;
	char	ll_line[UT_LINESIZE];
	char	ll_host[UT_HOSTSIZE];
};

#ifdef __minix
struct utmp {
  char ut_name[UT_NAMESIZE];		/* user name */
  char ut_id[4];		/* /etc/inittab ID */
  char ut_line[UT_LINESIZE];		/* terminal name */
  char ut_host[UT_HOSTSIZE];		/* host name, when remote */
  short ut_pid;			/* process id */
  short int ut_type;		/* type of entry */
  time_t ut_time;			/* login/logout time */
};

/* Definitions for ut_type. */
#define RUN_LVL            1	/* this is a RUN_LEVEL record */
#define BOOT_TIME          2	/* this is a REBOOT record */
#define INIT_PROCESS       5	/* this process was spawned by INIT */
#define LOGIN_PROCESS      6	/* this is a 'getty' process waiting */
#define USER_PROCESS       7	/* any other user process */
#define DEAD_PROCESS       8	/* this process has died (wtmp only) */

#else /* !__minix */

struct utmp {
	char	ut_line[UT_LINESIZE];
	char	ut_name[UT_NAMESIZE];
	char	ut_host[UT_HOSTSIZE];
	time_t	ut_time;
};
#endif /* __minix */

__BEGIN_DECLS
int utmpname(const char *);
void setutent(void);
#ifndef __LIBC12_SOURCE__
struct utmp *getutent(void) __RENAME(__getutent50);
#endif
void endutent(void);
__END_DECLS

#endif /* !_UTMP_H_ */

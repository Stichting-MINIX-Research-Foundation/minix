/*	$NetBSD: popen.c,v 1.32 2012/06/25 22:32:43 abs Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
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
static char sccsid[] = "@(#)popen.c	8.3 (Berkeley) 5/3/95";
#else
__RCSID("$NetBSD: popen.c,v 1.32 2012/06/25 22:32:43 abs Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "env.h"
#include "reentrant.h"

#ifdef __weak_alias
__weak_alias(popen,_popen)
__weak_alias(pclose,_pclose)
#endif

static struct pid {
	struct pid *next;
	FILE *fp;
#ifdef _REENTRANT
	int fd;
#endif
	pid_t pid;
} *pidlist; 
	
#ifdef _REENTRANT
static rwlock_t pidlist_lock = RWLOCK_INITIALIZER;
#endif

FILE *
popen(const char *command, const char *type)
{
	struct pid *cur, *old;
	FILE *iop;
	const char * volatile xtype = type;
	int pdes[2], pid, serrno;
	volatile int twoway;
	int flags;

	_DIAGASSERT(command != NULL);
	_DIAGASSERT(xtype != NULL);

	flags = strchr(xtype, 'e') ? O_CLOEXEC : 0;
	if (strchr(xtype, '+')) {
		int stype = flags ? (SOCK_STREAM | SOCK_CLOEXEC) : SOCK_STREAM;
		twoway = 1;
		xtype = "r+";
		if (socketpair(AF_LOCAL, stype, 0, pdes) < 0)
			return NULL;
	} else  {
		twoway = 0;
		xtype = strrchr(xtype, 'r') ? "r" : "w";
		if (pipe2(pdes, flags) == -1)
			return NULL;
	}

	if ((cur = malloc(sizeof(struct pid))) == NULL) {
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		errno = ENOMEM;
		return (NULL);
	}

#if defined(__minix)
	rwlock_rdlock(&pidlist_lock);
#else
	(void)rwlock_rdlock(&pidlist_lock);
#endif /* defined(__minix) */
	(void)__readlockenv();
	switch (pid = vfork()) {
	case -1:			/* Error. */
		serrno = errno;
		(void)__unlockenv();
#if defined(__minix)
		rwlock_unlock(&pidlist_lock);
#else
		(void)rwlock_unlock(&pidlist_lock);
#endif /* defined(__minix) */
		free(cur);
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		errno = serrno;
		return (NULL);
		/* NOTREACHED */
	case 0:				/* Child. */
		/* POSIX.2 B.3.2.2 "popen() shall ensure that any streams
		   from previous popen() calls that remain open in the 
		   parent process are closed in the new child process. */
		for (old = pidlist; old; old = old->next)
#ifdef _REENTRANT
			close(old->fd); /* don't allow a flush */
#else
			close(fileno(old->fp)); /* don't allow a flush */
#endif

		if (*xtype == 'r') {
			(void)close(pdes[0]);
			if (pdes[1] != STDOUT_FILENO) {
				(void)dup2(pdes[1], STDOUT_FILENO);
				(void)close(pdes[1]);
			}
			if (twoway)
				(void)dup2(STDOUT_FILENO, STDIN_FILENO);
		} else {
			(void)close(pdes[1]);
			if (pdes[0] != STDIN_FILENO) {
				(void)dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
		}

		execl(_PATH_BSHELL, "sh", "-c", command, NULL);
		_exit(127);
		/* NOTREACHED */
	}
	(void)__unlockenv();

	/* Parent; assume fdopen can't fail. */
	if (*xtype == 'r') {
		iop = fdopen(pdes[0], xtype);
#ifdef _REENTRANT
		cur->fd = pdes[0];
#endif
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], xtype);
#ifdef _REENTRANT
		cur->fd = pdes[1];
#endif
		(void)close(pdes[0]);
	}

	/* Link into list of file descriptors. */
	cur->fp = iop;
	cur->pid =  pid;
	cur->next = pidlist;
	pidlist = cur;
#if defined(__minix)
	rwlock_unlock(&pidlist_lock);
#else
	(void)rwlock_unlock(&pidlist_lock);
#endif /* defined(__minix) */

	return (iop);
}

/*
 * pclose --
 *	Pclose returns -1 if stream is not associated with a `popened' command,
 *	if already `pclosed', or waitpid returns an error.
 */
int
pclose(FILE *iop)
{
	struct pid *cur, *last;
	int pstat;
	pid_t pid;

	_DIAGASSERT(iop != NULL);

	rwlock_wrlock(&pidlist_lock);

	/* Find the appropriate file pointer. */
	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
		if (cur->fp == iop)
			break;
	if (cur == NULL) {
#if defined(__minix)
		rwlock_unlock(&pidlist_lock);
#else
		(void)rwlock_unlock(&pidlist_lock);
#endif /* defined(__minix) */
		return (-1);
	}

	(void)fclose(iop);

	/* Remove the entry from the linked list. */
	if (last == NULL)
		pidlist = cur->next;
	else
		last->next = cur->next;

#if defined(__minix)
	rwlock_unlock(&pidlist_lock);
#else
	(void)rwlock_unlock(&pidlist_lock);
#endif /* defined(__minix) */

	do {
		pid = waitpid(cur->pid, &pstat, 0);
	} while (pid == -1 && errno == EINTR);

	free(cur);

	return (pid == -1 ? -1 : pstat);
}

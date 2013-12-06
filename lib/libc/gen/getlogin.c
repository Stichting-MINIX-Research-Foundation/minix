/*	$NetBSD: getlogin.c,v 1.15 2009/01/11 02:46:27 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getlogin.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getlogin.c,v 1.15 2009/01/11 02:46:27 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "reentrant.h"
#include "extern.h"

#ifdef __weak_alias
__weak_alias(getlogin,_getlogin)
__weak_alias(getlogin_r,_getlogin_r)
#if !defined(__minix)
__weak_alias(setlogin,_setlogin)
#endif /* !defined(__minix) */
#endif

int	__logname_valid;		/* known to setlogin() */
static char logname[MAXLOGNAME + 1];

#ifdef _REENTRANT
static mutex_t	logname_mutex = MUTEX_INITIALIZER;
#endif

char *
getlogin(void)
{
	char *rv;

	mutex_lock(&logname_mutex);
	if (__logname_valid == 0) {
		if (__getlogin(logname, sizeof(logname) - 1) < 0) {
			mutex_unlock(&logname_mutex);
			return ((char *)NULL);
		}
		__logname_valid = 1;
	}
	rv = (*logname ? logname : (char *)NULL);
	mutex_unlock(&logname_mutex);

	return rv;
}

int
getlogin_r(char *name, size_t namelen)
{
	size_t len;
	int rv;

	mutex_lock(&logname_mutex);
	if (__logname_valid == 0) {
		if (__getlogin(logname, sizeof(logname) - 1) < 0) {
			rv = errno;
			mutex_unlock(&logname_mutex);
			return (rv);
		}
		__logname_valid = 1;
	}
	len = strlen(logname) + 1;
	if (len > namelen) {
		rv = ERANGE;
	} else {
		strncpy(name, logname, len);
		rv = 0;
	}
	mutex_unlock(&logname_mutex);

	return (rv);
}

#if !defined(__minix)
int
setlogin(const char *name)
{
	int retval; 

	retval = __setlogin(name);
	__logname_valid = 0;

	return (retval);
}
#endif /* !defined(__minix) */

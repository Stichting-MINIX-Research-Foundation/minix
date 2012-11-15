/*	$NetBSD: sysctlnametomib.c,v 1.7 2012/03/13 21:13:37 christos Exp $ */

/*-
 * Copyright (c) 2003,2004 The NetBSD Foundation, Inc.
 *	All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sysctlnametomib.c,v 1.7 2012/03/13 21:13:37 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#ifndef RUMP_ACTION
#include "namespace.h"
#endif
#include <sys/param.h>
#include <sys/sysctl.h>
#include <assert.h>

#ifdef RUMP_ACTION
#include <rump/rump_syscalls.h>
#define sysctl(a,b,c,d,e,f) rump_sys___sysctl(a,b,c,d,e,f)
#else
#ifdef __weak_alias
__weak_alias(sysctlnametomib,_sysctlnametomib)
#endif
#endif /* RUMP_ACTION */

/*
 * freebsd compatible sysctlnametomib() function, implemented as an
 * extremely thin wrapper around sysctlgetmibinfo().  i think the use
 * of size_t as the third argument is erroneous, but what can we do
 * about that?
 */
int
sysctlnametomib(const char *gname, int *iname, size_t *namelenp)
{
	u_int unamelen;
	int rc;

	_DIAGASSERT(__type_fit(u_int, *namelenp));
	unamelen = (u_int)*namelenp;
	rc = sysctlgetmibinfo(gname, iname, &unamelen, NULL, NULL, NULL,
			      SYSCTL_VERSION);
	*namelenp = unamelen;

	return (rc);
}

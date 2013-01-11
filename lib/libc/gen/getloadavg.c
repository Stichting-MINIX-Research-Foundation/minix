/*	$NetBSD: getloadavg.c,v 1.14 2012/03/13 21:13:36 christos Exp $	*/

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
static char sccsid[] = "@(#)getloadavg.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getloadavg.c,v 1.14 2012/03/13 21:13:36 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <uvm/uvm_param.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(getloadavg,_getloadavg)
#endif

/*
 * getloadavg() -- Get system load averages.
 *
 * Put `nelem' samples into `loadavg' array.
 * Return number of samples retrieved, or -1 on error.
 */
int
getloadavg(double loadavg[], int nelem)
{
	struct loadavg loadinfo;
	static const int mib[] = { CTL_VM, VM_LOADAVG };
	size_t size, i;

	_DIAGASSERT(loadavg != NULL);
	_DIAGASSERT(nelem >= 0);

	size = sizeof(loadinfo);
	if (sysctl(mib, (u_int)__arraycount(mib), &loadinfo, &size, NULL, 0)
	    == -1)
		return -1;

	size = MIN((size_t)nelem, __arraycount(loadinfo.ldavg));
	for (i = 0; i < size; i++)
		loadavg[i] = (double) loadinfo.ldavg[i] / loadinfo.fscale;
	return nelem;
}

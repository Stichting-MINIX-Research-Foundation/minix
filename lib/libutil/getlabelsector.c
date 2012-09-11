/*	$NetBSD: getlabelsector.c,v 1.5 2011/09/04 12:34:49 jmcneill Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
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
__RCSID("$NetBSD: getlabelsector.c,v 1.5 2011/09/04 12:34:49 jmcneill Exp $");
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <util.h>

int
getlabelsector(void)
{
	int sector, mib[2];
	size_t varlen;

	mib[0] = CTL_KERN;
	mib[1] = KERN_LABELSECTOR;
	varlen = sizeof(sector);
	if (sysctl(mib, 2, &sector, &varlen, NULL, (size_t)0) < 0)
		return (-1);

	return sector;
}

off_t
getlabeloffset(void)
{
	int offset, mib[2];
	size_t varlen;

	mib[0] = CTL_KERN;
	mib[1] = KERN_LABELOFFSET;
	varlen = sizeof(offset);
	if (sysctl(mib, 2, &offset, &varlen, NULL, (size_t)0) < 0)
		return (-1);

	return ((off_t)offset);
}

int
getlabelusesmbr(void)
{
	int use;
	size_t uselen;

	uselen = sizeof(use);
	if (sysctlbyname("kern.labelusesmbr", &use, &uselen,
	    NULL, (size_t)0) < 0)
		return (-1);

	return use;
}

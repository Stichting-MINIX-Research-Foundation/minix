/*	$NetBSD: getbootfile.c,v 1.5 2008/04/28 20:23:02 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thomas Klausner.
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
__RCSID("$NetBSD: getbootfile.c,v 1.5 2008/04/28 20:23:02 martin Exp $");
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <string.h>
#include <paths.h>
#include <util.h>

#ifdef CPU_BOOTED_KERNEL
static char name[MAXPATHLEN]; 
#endif

const char *
getbootfile(void)
{
#ifdef CPU_BOOTED_KERNEL
	int mib[2];
	size_t size;
#endif
	const char *kernel;

	kernel = _PATH_UNIX;
#ifdef CPU_BOOTED_KERNEL
	/* find real boot-kernel name */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_BOOTED_KERNEL;
	size = sizeof(name) - 1;
	if (sysctl(mib, 2, name + 1, &size, NULL, 0) == 0) {
		/*
		 * traditionally, this sysctl returns the relative
		 * path of the kernel with the leading slash stripped
		 * -- could be empty, though (e.g. when netbooting).
		 */
		if (name[1] != '\0') {
			name[0] = '/';
			kernel = name;
		}

		/* check if we got a valid and 'secure' filename */
		if (strcmp(kernel, _PATH_UNIX) != 0 && 
		    secure_path(kernel) != 0) {
			/* doesn't seems so, fall back to default */
			kernel = _PATH_UNIX;
		}
	}
#endif

	return (kernel);
}

/*	$NetBSD: ttyslot.c,v 1.13 2009/01/11 02:46:27 christos Exp $	*/

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
static char sccsid[] = "@(#)ttyslot.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: ttyslot.c,v 1.13 2009/01/11 02:46:27 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <ttyent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(ttyslot,_ttyslot)
#endif

int
ttyslot(void)
{
	struct ttyent *ttyp;
	int slot = 0, ispty = 0;
	char *p;
	int cnt;
	char *name;
#if !defined(__minix)
	struct ptmget ptm;
#endif /* !defined(__minix) */

	setttyent();
	for (cnt = 0; cnt < 3; ++cnt) {
#if !defined(__minix)
		if (ioctl(cnt, TIOCPTSNAME, &ptm) != -1) {
			ispty = 1;
			name = ptm.sn;
		} else if ((name = ttyname(cnt)) != NULL) {
#else
		if ((name = ttyname(cnt)) != NULL) {
#endif /* !defined(__minix) */
			ispty = 0;
		} else
			continue;

		if ((p = strstr(name, "/pts/")) != NULL)
			++p;
		else if ((p = strrchr(name, '/')) != NULL)
			++p;
		else
			p = name;

		for (slot = 1; (ttyp = getttyent()) != NULL; ++slot)
			if (!strcmp(ttyp->ty_name, p)) {
				endttyent();
				return slot;
			}
		break;
	}
	endttyent();
	if (ispty) {
		struct stat st;
		if (fstat(cnt, &st) == -1)
			return 0;
		return slot + (int)minor(st.st_rdev) + 1;
	}
	return 0;
}

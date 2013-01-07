/*	$NetBSD: utmp.c,v 1.9 2009/02/05 23:52:55 lukem Exp $	 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__RCSID("$NetBSD: utmp.c,v 1.9 2009/02/05 23:52:55 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <utmp.h>
#include <sys/stat.h>

static struct utmp utmp;
static FILE *ut;
static char utfile[MAXPATHLEN] = _PATH_UTMP;

void
setutent(void)
{
	if (ut == NULL)
		return;
	(void)fseeko(ut, (off_t)0, SEEK_SET);
}

struct utmp *
getutent(void)
{
	if (ut == NULL) {
		struct stat st;
		off_t numentries;
		if ((ut = fopen(utfile, "r")) == NULL)
			return NULL;
		if (fstat(fileno(ut), &st) == -1)
			goto out;
		/*
		 * If we have a an old version utmp file bail.
		 */
		numentries = st.st_size / sizeof(utmp);
		if ((off_t)(numentries * sizeof(utmp)) != st.st_size)
			goto out;
	}
	if (fread(&utmp, sizeof(utmp), 1, ut) == 1)
		return &utmp;
out:
	(void)fclose(ut);
	return NULL;
}

void
endutent(void)
{
	if (ut != NULL) {
		(void)fclose(ut);
		ut = NULL;
	}
}

int
utmpname(const char *fname)
{
	size_t len = strlen(fname);

	if (len >= sizeof(utfile))
		return 0;

	/* must not end in x! */
	if (fname[len - 1] == 'x')
		return 0;

	(void)strlcpy(utfile, fname, sizeof(utfile));
	endutent();
	return 1;
}

#ifdef __minix
__weak_alias(getutent, __getutent50)
#endif

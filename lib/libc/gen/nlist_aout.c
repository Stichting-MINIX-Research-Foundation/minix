/* $NetBSD: nlist_aout.c,v 1.23 2012/03/21 15:32:26 christos Exp $ */

/*
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

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)nlist.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: nlist_aout.c,v 1.23 2012/03/21 15:32:26 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

struct nlist;
#include "nlist_private.h"

#ifdef NLIST_AOUT
#include <a.out.h>
#include <sys/exec_aout.h>

int
__fdnlist_aout(int fd, struct nlist *list)
{
	struct nlist *p, *s;
	char *strtab;
	off_t stroff, symoff;
	int nent;
	size_t strsize, symsize, cc;
	struct nlist nbuf[1024];
	struct exec exec;
	struct stat st;
	char *scoreboard, *scored;

	_DIAGASSERT(fd != -1);
	_DIAGASSERT(list != NULL);

	if (pread(fd, &exec, sizeof(exec), (off_t)0) != sizeof(exec) ||
	    N_BADMAG(exec) || fstat(fd, &st) < 0)
		return (-1);

	symoff = N_SYMOFF(exec);
	symsize = (size_t)exec.a_syms;
	stroff = symoff + symsize;

	/* Check for files too large to mmap. */
	if ((uintmax_t)(st.st_size - stroff) > (uintmax_t)SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strsize = (size_t)(st.st_size - stroff);
	strtab = mmap(NULL, strsize, PROT_READ, MAP_PRIVATE|MAP_FILE,
	    fd, stroff);
	if (strtab == (char *)-1)
		return (-1);
	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 *
	 * XXX clearing anything other than n_type and n_value violates
	 * the semantics given in the man page.
	 */
	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}
	if (lseek(fd, symoff, SEEK_SET) == -1)
		return (-1);
#if defined(__SSP__) || defined(__SSP_ALL__)
	scoreboard = malloc((size_t)nent);
#else
	scoreboard = alloca((size_t)nent);
#endif
	if (scoreboard == NULL)
		return (-1);
	(void)memset(scoreboard, 0, (size_t)nent);

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(nbuf));
		if (read(fd, nbuf, cc) != (ssize_t) cc)
			break;
		symsize -= cc;
		for (s = nbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			long soff = s->n_un.n_strx;

			if (soff == 0 || (s->n_type & N_STAB) != 0)
				continue;
			for (p = list, scored = scoreboard; !ISLAST(p);
			    p++, scored++)
				if (*scored == 0 &&
				    !strcmp(&strtab[(size_t)soff],
				    p->n_un.n_name)) {
					p->n_value = s->n_value;
					p->n_type = s->n_type;
					p->n_desc = s->n_desc;
					p->n_other = s->n_other;
					*scored = 1;
					if (--nent <= 0)
						break;
				}
		}
	}
	munmap(strtab, strsize);
#if defined(__SSP__) || defined(__SSP_ALL__)
	free(scoreboard);
#endif
	return (nent);
}
#endif /* NLIST_AOUT */

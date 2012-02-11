/* $NetBSD: nlist_ecoff.c,v 1.18 2009/08/21 08:42:02 he Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: nlist_ecoff.c,v 1.18 2009/08/21 08:42:02 he Exp $");
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
#include <nlist.h>

#include "nlist_private.h"
#ifdef NLIST_ECOFF
#include <sys/exec_ecoff.h>
#endif

#ifdef NLIST_ECOFF
#define	check(off, size)	((off < 0) || (off + size > mappedsize))
#define	BAD			do { rv = -1; goto out; } while (/*CONSTCOND*/0)
#define	BADUNMAP		do { rv = -1; goto unmap; } while (/*CONSTCOND*/0)

int
__fdnlist_ecoff(fd, list)
	int fd;
	struct nlist *list;
{
	struct nlist *p;
	struct ecoff_exechdr *exechdrp;
	struct ecoff_symhdr *symhdrp;
	struct ecoff_extsym *esyms;
	struct stat st;
	char *mappedfile;
	size_t mappedsize;
	u_long symhdroff, extstroff;
	u_int symhdrsize;
	int rv, nent;
	long i, nesyms;

	_DIAGASSERT(fd != -1);
	_DIAGASSERT(list != NULL);

	rv = -1;

	/*
	 * If we can't fstat() the file, something bad is going on.
	 */
	if (fstat(fd, &st) < 0)
		BAD;

	/*
	 * Map the file in its entirety.
	 */
	if ((uintmax_t)st.st_size > (uintmax_t)SIZE_T_MAX) {
		errno = EFBIG;
		BAD;
	}
	mappedsize = st.st_size;
	mappedfile = mmap(NULL, mappedsize, PROT_READ, MAP_PRIVATE|MAP_FILE,
	    fd, 0);
	if (mappedfile == (char *)-1)
		BAD;

	/*
	 * Make sure we can access the executable's header
	 * directly, and make sure the recognize the executable
	 * as an ECOFF binary.
	 */
	if (check(0, sizeof *exechdrp))
		BADUNMAP;
	exechdrp = (struct ecoff_exechdr *)&mappedfile[0];

	if (ECOFF_BADMAG(exechdrp))
		BADUNMAP;

	/*
	 * Find the symbol list.
	 */
	symhdroff = exechdrp->f.f_symptr;
	symhdrsize = exechdrp->f.f_nsyms;

	if ((symhdroff + sizeof *symhdrp) > mappedsize ||
	    sizeof *symhdrp != symhdrsize)
		BADUNMAP;
	symhdrp = (struct ecoff_symhdr *)&mappedfile[symhdroff];

	nesyms = symhdrp->esymMax;
	if (check(symhdrp->cbExtOffset, nesyms * sizeof *esyms))
		BADUNMAP;
	esyms = (struct ecoff_extsym *)&mappedfile[symhdrp->cbExtOffset];
	extstroff = symhdrp->cbSsExtOffset;

	/*
	 * Clean out any left-over information for all valid entries.
	 * Type and value are defined to be 0 if not found; historical
	 * versions cleared other and desc as well.
	 *
	 * XXX Clearing anything other than n_type and n_value violates
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

	for (i = 0; i < nesyms; i++) {
		for (p = list; !ISLAST(p); p++) {
			const char *nlistname;
			char *symtabname;

			/* This may be incorrect */
			nlistname = N_NAME(p);
			if (*nlistname == '_')
				nlistname++;

			symtabname =
			    &mappedfile[extstroff + esyms[i].es_strindex];

			if (!strcmp(symtabname, nlistname)) {
				/*
				 * Translate (roughly) from ECOFF to nlist
				 */
				p->n_value = esyms[i].es_value;
				p->n_type = N_EXT;		/* XXX */
				p->n_desc = 0;			/* XXX */
				p->n_other = 0;			/* XXX */

				if (--nent <= 0)
					goto done;
				break;	/* into next run of outer loop */
			}
		}
	}

done:
	rv = nent;
unmap:
	munmap(mappedfile, mappedsize);
out:
	return (rv);
}

#endif /* NLIST_ECOFF */

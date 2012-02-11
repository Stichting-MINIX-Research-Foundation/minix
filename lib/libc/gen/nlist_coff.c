/* $NetBSD: nlist_coff.c,v 1.8 2009/08/21 08:42:02 he Exp $ */

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
__RCSID("$NetBSD: nlist_coff.c,v 1.8 2009/08/21 08:42:02 he Exp $");
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
#ifdef NLIST_COFF
#include <sys/exec_coff.h>
#endif

#ifdef NLIST_COFF
#define	BAD		do { rv = -1; goto out; } while (/*CONSTCOND*/0)
#define	BADUNMAP	do { rv = -1; goto unmap; } while (/*CONSTCOND*/0)

#define ES_LEN 18
struct coff_extsym {
	union {
		char u_name[8];
		struct {
			int u_zero;
			int u_offset;
		} s;
	} u;
	int32_t es_value;
	int16_t es_scnum;
	int16_t es_type;
	int8_t es_class;
	int8_t es_numaux;
};
#define es_name u.u_name
#define es_zero u.s.u_zero
#define es_offset u.s.u_offset

int
__fdnlist_coff(fd, list)
	int fd;
	struct nlist *list;
{
	struct nlist *p;
	struct coff_filehdr *filehdrp;
	struct stat st;
	char *mappedfile;
	size_t mappedsize;
	u_long symoff, extstroff;
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
	 * directly, and make sure we recognize the executable
	 * as an COFF binary.
	 */
	if (mappedsize < sizeof (struct coff_filehdr))
		BADUNMAP;
	filehdrp = (struct coff_filehdr *)&mappedfile[0];

	if (COFF_BADMAG(filehdrp))
		BADUNMAP;

	/*
	 * Find the symbol list.
	 */
	symoff = filehdrp->f_symptr;
	nesyms = filehdrp->f_nsyms;

	if (symoff + ES_LEN * nesyms > mappedsize)
		BADUNMAP;
	extstroff = symoff + ES_LEN * nesyms;

	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}

	for (i = 0; i < nesyms; i++) {
		char *symtabname;
		const char *nlistname;
		struct coff_extsym esym;
		char name[10];

		memcpy(&esym, &mappedfile[symoff + ES_LEN * i], ES_LEN);
		if (esym.es_numaux != 0) {
			i += esym.es_numaux;	/* XXX Skip aux entry */
			continue;
		}
			
		if (esym.es_zero != 0) {
			memcpy(name, esym.es_name, 8);
			name[8] = 0;
			symtabname = name;
		} else if (esym.es_offset != 0)
			symtabname = &mappedfile[extstroff + esym.es_offset];
		else
			continue;

		for (p = list; !ISLAST(p); p++) {
			nlistname = N_NAME(p);
			if (!strcmp(symtabname, nlistname)) {
				/*
				 * Translate (roughly) from COFF to nlist
				 */
				p->n_value = esym.es_value;
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

#endif /* NLIST_COFF */

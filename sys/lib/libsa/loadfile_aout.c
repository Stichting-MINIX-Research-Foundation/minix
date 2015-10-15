/* $NetBSD: loadfile_aout.c,v 1.15 2014/02/20 00:29:03 joerg Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Christos Zoulas.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#ifdef _STANDALONE
#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#endif

#include <sys/param.h>
#include <sys/exec_aout.h>

#include "loadfile.h"

#ifdef BOOT_AOUT

int
loadfile_aout(int fd, struct exec *x, u_long *marks, int flags)
{
	u_long entry = x->a_entry;
	paddr_t aoutp = 0;
	paddr_t minp, maxp;
	int cc;
	paddr_t offset = marks[MARK_START];
	u_long magic = N_GETMAGIC(*x);
	int sub;
	ssize_t nr;

	/* some ports don't use the offset */
	(void)offset;

	/* In OMAGIC and NMAGIC, exec header isn't part of text segment */
	if (magic == OMAGIC || magic == NMAGIC)
		sub = 0;
	else
		sub = sizeof(*x);

	minp = maxp = ALIGNENTRY(entry);

	if (lseek(fd, sizeof(*x), SEEK_SET) == -1)  {
		WARN(("lseek text"));
		return 1;
	}

	/*
	 * Leave a copy of the exec header before the text.
	 * The kernel may use this to verify that the
	 * symbols were loaded by this boot program.
	 */
	if (magic == OMAGIC || magic == NMAGIC) {
		if (flags & LOAD_HDR && maxp >= sizeof(*x))
			BCOPY(x, maxp - sizeof(*x), sizeof(*x));
	}
	else {
		if (flags & LOAD_HDR)
			BCOPY(x, maxp, sizeof(*x));
		if (flags & (LOAD_HDR|COUNT_HDR))
			maxp += sizeof(*x);
	}

	/*
	 * Read in the text segment.
	 */
	if (flags & LOAD_TEXT) {
		PROGRESS(("%ld", x->a_text));

		nr = READ(fd, maxp, x->a_text - sub);
		if (nr == -1) {
			WARN(("read text"));
			return 1;
		}
		if (nr != (ssize_t)(x->a_text - sub)) {
			errno = EIO;
			WARN(("read text"));
			return 1;
		}
	} else {
		if (lseek(fd, x->a_text - sub, SEEK_CUR) == -1) {
			WARN(("seek text"));
			return 1;
		}
	}
	if (flags & (LOAD_TEXT|COUNT_TEXT))
		maxp += x->a_text - sub;

	/*
	 * Provide alignment if required
	 */
	if (magic == ZMAGIC || magic == NMAGIC) {
		int size = -(unsigned int)maxp & (AOUT_LDPGSZ - 1);

		if (flags & LOAD_TEXTA) {
			PROGRESS(("/%d", size));
			BZERO(maxp, size);
		}

		if (flags & (LOAD_TEXTA|COUNT_TEXTA))
			maxp += size;
	}

	/*
	 * Read in the data segment.
	 */
	if (flags & LOAD_DATA) {
		PROGRESS(("+%ld", x->a_data));

		marks[MARK_DATA] = LOADADDR(maxp);
		nr = READ(fd, maxp, x->a_data);
		if (nr == -1) {
			WARN(("read data"));
			return 1;
		}
		if (nr != (ssize_t)x->a_data) {
			errno = EIO;
			WARN(("read data"));
			return 1;
		}
	}
	else {
		if (lseek(fd, x->a_data, SEEK_CUR) == -1) {
			WARN(("seek data"));
			return 1;
		}
	}
	if (flags & (LOAD_DATA|COUNT_DATA))
		maxp += x->a_data;

	/*
	 * Zero out the BSS section.
	 * (Kernel doesn't care, but do it anyway.)
	 */
	if (flags & LOAD_BSS) {
		PROGRESS(("+%ld", x->a_bss));

		BZERO(maxp, x->a_bss);
	}

	if (flags & (LOAD_BSS|COUNT_BSS))
		maxp += x->a_bss;

	/*
	 * Read in the symbol table and strings.
	 * (Always set the symtab size word.)
	 */
	if (flags & LOAD_SYM)
		BCOPY(&x->a_syms, maxp, sizeof(x->a_syms));

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		maxp += sizeof(x->a_syms);
		aoutp = maxp;
	}

	if (x->a_syms > 0) {
		/* Symbol table and string table length word. */

		if (flags & LOAD_SYM) {
			PROGRESS(("+[%ld", x->a_syms));

			nr = READ(fd, maxp, x->a_syms);
			if (nr == -1) {
				WARN(("read symbols"));
				return 1;
			}
			if (nr != (ssize_t)x->a_syms) {
				errno = EIO;
				WARN(("read symbols"));
				return 1;
			}
		} else  {
			if (lseek(fd, x->a_syms, SEEK_CUR) == -1) {
				WARN(("seek symbols"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += x->a_syms;

		nr = read(fd, &cc, sizeof(cc));
		if (nr == -1) {
			WARN(("read string table"));
			return 1;
		}
		if (nr != sizeof(cc)) {
			errno = EIO;
			WARN(("read string table"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			BCOPY(&cc, maxp, sizeof(cc));

			/* String table. Length word includes itself. */

			PROGRESS(("+%d]", cc));
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += sizeof(cc);

		cc -= sizeof(int);
		if (cc <= 0) {
			WARN(("symbol table too short"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			nr = READ(fd, maxp, cc);
			if (nr == -1) {
				WARN(("read strings"));
				return 1;
			}
			if (nr != cc) {
				errno = EIO;
				WARN(("read strings"));
				return 1;
			}
		} else {
			if (lseek(fd, cc, SEEK_CUR) == -1) {
				WARN(("seek strings"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += cc;
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(entry);
	marks[MARK_NSYM] = x->a_syms;
	marks[MARK_SYM] = LOADADDR(aoutp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}

#endif /* BOOT_AOUT */

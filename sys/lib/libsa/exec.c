/*	$NetBSD: exec.c,v 1.28 2009/12/29 20:21:46 elad Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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

#include <sys/param.h>
#include <sys/reboot.h>
#ifndef SA_EXEC_ANYOWNER
#include <sys/stat.h>
#endif
#include <sys/exec_aout.h>
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"

void
exec(char *path, char *loadaddr, int howto)
{
#ifndef SA_EXEC_ANYOWNER
	struct stat sb;
#endif
	struct exec x;
	int io, i;
	char *addr, *ssym, *esym;

	io = open(path, 0);
	if (io < 0)
		return;

#ifndef SA_EXEC_ANYOWNER
	(void) fstat(io, &sb);
	if (sb.st_uid || (sb.st_mode & 2)) {
		printf("non-secure file, will not load\n");
		close(io);
		errno = EPERM;
		return;
	}
#endif

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) || N_BADMAG(x)) {
		errno = EFTYPE;
		return;
	}

	/* Text */
	printf("%ld", x.a_text);
	addr = loadaddr;
	if (N_GETMAGIC(x) == ZMAGIC) {
		(void)memcpy(addr, &x, sizeof(x));
		addr += sizeof(x);
		x.a_text -= sizeof(x);
	}
	if (read(io, (char *)addr, x.a_text) != (ssize_t)x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((long)addr & (N_PAGSIZ(x) - 1))
			*addr++ = 0;

	/* Data */
	printf("+%ld", x.a_data);
	if (read(io, addr, x.a_data) != (ssize_t)x.a_data)
		goto shread;
	addr += x.a_data;

	/* Bss */
	printf("+%ld", x.a_bss);
	for (i = 0; i < (int)x.a_bss; i++)
		*addr++ = 0;

	/* Symbols */
	ssym = addr;
	(void)memcpy(addr, &x.a_syms, sizeof(x.a_syms));
	addr += sizeof(x.a_syms);
	if (x.a_syms) {
		printf("+[%ld", x.a_syms);
		if (read(io, addr, x.a_syms) != (ssize_t)x.a_syms)
			goto shread;
		addr += x.a_syms;
	}

	i = 0;
	if (x.a_syms && read(io, &i, sizeof(int)) != sizeof(int))
		goto shread;

	(void)memcpy(addr, &i, sizeof(int));
	if (i) {
		i -= sizeof(int);
		addr += sizeof(int);
		if (read(io, addr, i) != i)
			goto shread;
		addr += i;
	}

	if (x.a_syms) {
		/* and that many bytes of (debug symbols?) */
		printf("+%d]", i);
	}

	close(io);

#define	round_to_size(x) \
	(((int)(x) + sizeof(int) - 1) & ~(sizeof(int) - 1))
	esym = (char *)round_to_size(addr - loadaddr);
#undef round_to_size

	/* and note the end address of all this	*/
	printf(" total=0x%lx\n", (u_long)addr);

	/*
	 * Machine-dependent code must now adjust the
	 * entry point.  This used to be done here,
	 * but some systems may need to relocate the
	 * loaded file before jumping to it, and the
	 * displayed start address would be wrong.
	 */

#ifdef EXEC_DEBUG
	printf("ssym=0x%x esym=0x%x\n", ssym, esym);
	printf("\n\nReturn to boot...\n");
	getchar();
#endif

	machdep_start((char *)x.a_entry, howto, loadaddr, ssym, esym);

	/* exec failed */
	errno = ENOEXEC;
	return;

shread:
	close(io);
	errno = EIO;
	return;
}

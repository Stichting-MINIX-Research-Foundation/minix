/* $NetBSD: loadfile.c,v 1.30 2008/05/20 16:04:08 ad Exp $ */

/*-
 * Copyright (c) 1997, 2008 The NetBSD Foundation, Inc.
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
#include <sys/exec.h>

#include "loadfile.h"

uint32_t	netbsd_version;
u_int		netbsd_elf_class;

/*
 * Open 'filename', read in program and return the opened file
 * descriptor if ok, or -1 on error.
 * Fill in marks
 */
int
loadfile(const char *fname, u_long *marks, int flags)
{
	int fd, error;

	/* Open the file. */
	if ((fd = open(fname, 0)) < 0) {
		WARN(("open %s", fname ? fname : "<default>"));
		return -1;
	}

	/* Load it; save the value of errno across the close() call */
	if ((error = fdloadfile(fd, marks, flags)) != 0) {
		(void)close(fd);
		errno = error;
		return -1;
	}

	return fd;
}

/*
 * Read in program from the given file descriptor.
 * Return error code (0 on success).
 * Fill in marks.
 */
int
fdloadfile(int fd, u_long *marks, int flags)
{
	union {
#ifdef BOOT_ECOFF
		struct ecoff_exechdr coff;
#endif
#ifdef BOOT_ELF32
		Elf32_Ehdr elf32;
#endif
#ifdef BOOT_ELF64
		Elf64_Ehdr elf64;
#endif
#ifdef BOOT_AOUT
		struct exec aout;
#endif
	} hdr;
	ssize_t nr;
	int rval;

	/* Read the exec header. */
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		goto err;
	nr = read(fd, &hdr, sizeof(hdr));
	if (nr == -1) {
		WARN(("read header failed"));
		goto err;
	}
	if (nr != sizeof(hdr)) {
		WARN(("read header short"));
		errno = EFTYPE;
		goto err;
	}

#ifdef BOOT_ECOFF
	if (!ECOFF_BADMAG(&hdr.coff)) {
		rval = loadfile_coff(fd, &hdr.coff, marks, flags);
	} else
#endif
#ifdef BOOT_ELF32
	if (memcmp(hdr.elf32.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
	    	netbsd_elf_class = ELFCLASS32;
		rval = loadfile_elf32(fd, &hdr.elf32, marks, flags);
	} else
#endif
#ifdef BOOT_ELF64
	if (memcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
	    	netbsd_elf_class = ELFCLASS64;
		rval = loadfile_elf64(fd, &hdr.elf64, marks, flags);
	} else
#endif
#ifdef BOOT_AOUT
	if (OKMAGIC(N_GETMAGIC(hdr.aout))
#ifndef NO_MID_CHECK
	    && N_GETMID(hdr.aout) == MID_MACHINE
#endif
	    ) {
		rval = loadfile_aout(fd, &hdr.aout, marks, flags);
	} else
#endif
	{
		rval = 1;
		errno = EFTYPE;
	}

	if (rval == 0) {
		if ((flags & LOAD_ALL) != 0)
			PROGRESS(("=0x%lx\n",
				  marks[MARK_END] - marks[MARK_START]));
		return 0;
	}
err:
	return errno;
}

/*	$NetBSD: loadfile_machdep.h,v 1.6 2014/08/06 21:57:51 joerg Exp $	 */

/*-
 * Copyright (c) 1998, 2007, 2009 The NetBSD Foundation, Inc.
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

#define BOOT_ELF32
#define BOOT_ELF64

#define LOAD_KERNEL	(LOAD_ALL & ~LOAD_TEXTA)
#define COUNT_KERNEL	(COUNT_ALL & ~COUNT_TEXTA)

#ifdef _STANDALONE

#define LOADADDR(a)		((((u_long)(a)) & 0x07ffffff) + offset)
#define ALIGNENTRY(a)		((u_long)(a) & 0x00100000)
#define READ(f, b, c)		pread((f), (void *)LOADADDR(b), (c))
#define BCOPY(s, d, c)		vpbcopy((s), (void *)LOADADDR(d), (c))
#define BZERO(d, c)		pbzero((void *)LOADADDR(d), (c))
#define	WARN(a)			do { \
					(void)printf a; \
					if (errno) \
						(void)printf(": %s\n", \
						             strerror(errno)); \
					else \
						(void)printf("\n"); \
				} while(/* CONSTCOND */0)
#define PROGRESS(a)		x86_progress a
#define ALLOC(a)		alloc(a)
#define DEALLOC(a, b)		dealloc(a, b)
#define OKMAGIC(a)		((a) == ZMAGIC)

void x86_progress(const char *, ...) __printflike(1, 2);
void vpbcopy(const void *, void *, size_t);
void pbzero(void *, size_t);
ssize_t pread(int, void *, size_t);

#else
#ifdef TEST
#define LOADADDR(a)		offset
#define READ(f, b, c)		c
#define BCOPY(s, d, c)	
#define BZERO(d, c)	
#define PROGRESS(a)		(void) printf a
#else
#define LOADADDR(a)		(((u_long)(a)) + offset)
#define READ(f, b, c)		read((f), (void *)LOADADDR(b), (c))
#define BCOPY(s, d, c)		memcpy((void *)LOADADDR(d), (void *)(s), (c))
#define BZERO(d, c)		memset((void *)LOADADDR(d), 0, (c))
#define PROGRESS(a)		/* nothing */
#endif
#define WARN(a)			warn a
#define ALIGNENTRY(a)		((u_long)(a))
#define ALLOC(a)		malloc(a)
#define DEALLOC(a, b)		free(a)
#define OKMAGIC(a)		((a) == OMAGIC)

ssize_t vread(int, u_long, u_long *, size_t);
void vcopy(u_long, u_long, u_long *, size_t);
void vzero(u_long, u_long *, size_t);

#endif

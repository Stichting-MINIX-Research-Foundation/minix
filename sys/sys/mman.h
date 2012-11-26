/*	$NetBSD: mman.h,v 1.44 2012/01/05 15:19:52 reinoud Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
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
 *
 *	@(#)mman.h	8.2 (Berkeley) 1/9/95
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/featuretest.h>

#include <machine/ansi.h>
#include <minix/type.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#include <sys/ansi.h>

#ifndef	mode_t
typedef	__mode_t	mode_t;
#define	mode_t		__mode_t
#endif

#ifndef	off_t
typedef	__off_t		off_t;		/* file offset */
#define	off_t		__off_t
#endif


/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_NONE	0x00	/* no permissions */
#define	PROT_READ	0x01	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x04	/* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#ifndef __minix
#define	MAP_SHARED	0x0001	/* share changes */
#endif
#define	MAP_PRIVATE	0x0002	/* changes are private */

/*
 * Mapping type
 */
#define MAP_ANON	0x0004  /* anonymous memory */

/*
 * Minix specific flags.
 */
#define MAP_PREALLOC	0x0008		/* not on-demand */
#define MAP_CONTIG	0x0010		/* contiguous in physical memory */
#define MAP_LOWER16M	0x0020		/* physically below 16MB */
#define MAP_ALIGN64K	0x0040		/* physically aligned at 64kB */
#define MAP_LOWER1M	0x0080		/* physically below 16MB */
#define	MAP_ALIGNMENT_64KB	MAP_ALIGN64K

#define MAP_FIXED      0x0200  /* require mapping to happen at hint */
#define MAP_THIRDPARTY	0x0400		/* perform on behalf of any process */
#define MAP_UNINITIALIZED 0x0800	/* do not clear memory */

/*
 * Error indicator returned by mmap(2)
 */
#define	MAP_FAILED	((void *) -1)	/* mmap() failed */

#include <sys/cdefs.h>

__BEGIN_DECLS
#ifndef __minix
void *	mmap(void *, size_t, int, int, int, off_t);
int	munmap(void *, size_t);
#else
void *	minix_mmap(void *, size_t, int, int, int, off_t);
void *	minix_mmap_for(endpoint_t, void *, size_t, int, int, int, off_t);
int	minix_munmap(void *, size_t);
void *		vm_remap(int d, int s, void *da, void *sa, size_t si);
void *		vm_remap_ro(int d, int s, void *da, void *sa, size_t si);
int 		vm_unmap(int endpt, void *addr);
unsigned long 	vm_getphys(int endpt, void *addr);
u8_t 		vm_getrefcount(int endpt, void *addr);
#endif /* __minix */
__END_DECLS

#endif /* !_SYS_MMAN_H_ */

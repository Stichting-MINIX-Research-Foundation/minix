/* $NetBSD: nlist_private.h,v 1.23 2015/06/13 17:28:42 dennis Exp $ */

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

#if defined(__alpha__)
#  define	NLIST_ECOFF
#  define	NLIST_ELF64
#elif defined(__x86_64__)
#  define	NLIST_ELF64
#  define	NLIST_ELF32
#elif defined(__mips__)
#  define	NLIST_ECOFF
#  define	NLIST_ELF32
#  ifndef __mips_o32
#    define	NLIST_ELF64
#  endif
#elif defined(__arm__) || defined(__i386__) || defined (__m68k__) || \
    defined(__powerpc__) || defined(__vax__)
#  define	NLIST_AOUT
#  define	NLIST_ELF32
#elif defined(__sparc__)
#  define	NLIST_AOUT
#  define	NLIST_ELF32
#  define	NLIST_ELF64
#elif defined(__SH5__)
#  define	NLIST_ELF32
#  define	NLIST_ELF64
#elif defined(__sh__)
#  define	NLIST_COFF
#  define	NLIST_ELF32
#elif defined(__hppa__)
#  define	NLIST_ELF32
#elif defined(__riscv__)
#  define	NLIST_ELF32
#  ifdef __riscv64
#    define	NLIST_ELF64
#  endif
#elif defined(__aarch64__)
#  define	NLIST_ELF64
#else
#  define	NLIST_AOUT
/* #define	NLIST_ECOFF */
/* #define	NLIST_ELF32 */
/* #define	NLIST_ELF64 */
#endif

#define	ISLAST(p)	(N_NAME(p) == 0 || N_NAME(p)[0] == 0)

struct nlist;

#ifdef NLIST_AOUT
int	__fdnlist_aout(int, struct nlist *);
#endif
#ifdef NLIST_COFF
int	__fdnlist_coff(int, struct nlist *);
#endif
#ifdef NLIST_ECOFF
int	__fdnlist_ecoff(int, struct nlist *);
#endif
#ifdef NLIST_ELF32
int	__fdnlist_elf32(int, struct nlist *);
#endif
#ifdef NLIST_ELF64
int	__fdnlist_elf64(int, struct nlist *);
#endif

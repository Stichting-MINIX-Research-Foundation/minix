/*	$NetBSD: memset.c,v 1.10 2013/12/02 21:21:33 joerg Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Hibler and Chris Torek.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)memset.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: memset.c,v 1.10 2013/12/02 21:21:33 joerg Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <assert.h>
#include <limits.h>
#include <string.h>
#else
#include <lib/libkern/libkern.h>
#if defined(BZERO) && defined(_STANDALONE)
#include <lib/libsa/stand.h>
#endif
#include <machine/limits.h>
#endif 

#define	wsize	sizeof(u_int)
#define	wmask	(wsize - 1)

#ifdef _FORTIFY_SOURCE
#undef bzero
#endif
#undef memset

#ifndef __OPTIMIZE_SIZE__
#ifdef BZERO
#define	RETURN	return
#define	VAL	0
#define	WIDEVAL	0

void
bzero(void *dst0, size_t length)
#else
#define	RETURN	return (dst0)
#define	VAL	c0
#define	WIDEVAL	c

#if defined(__ARM_EABI__)
void __aeabi_memset(void *, size_t, int);
void __aeabi_memclr(void *, size_t);

__strong_alias(__aeabi_memset4, __aebi_memset)
__strong_alias(__aeabi_memset8, __aebi_memset)

void
__aeabi_memset(void *dst0, size_t length, int c)
{
	memset(dst0, c, length);
}

void
__aeabi_memclr(void *dst0, size_t length)
{
	memset(dst0, 0, length);
}
#endif

void *
memset(void *dst0, int c0, size_t length)
#endif
{
	size_t t;
#ifndef BZERO
	u_int c;
#endif
	u_char *dst;

	_DIAGASSERT(dst0 != 0);

	dst = dst0;
	/*
	 * If not enough words, just fill bytes.  A length >= 2 words
	 * guarantees that at least one of them is `complete' after
	 * any necessary alignment.  For instance:
	 *
	 *	|-----------|-----------|-----------|
	 *	|00|01|02|03|04|05|06|07|08|09|0A|00|
	 *	          ^---------------------^
	 *		 dst		 dst+length-1
	 *
	 * but we use a minimum of 3 here since the overhead of the code
	 * to do word writes is substantial.
	 */ 
	if (length < 3 * wsize) {
		while (length != 0) {
			*dst++ = VAL;
			--length;
		}
		RETURN;
	}

#ifndef BZERO
	if ((c = (u_char)c0) != 0) {	/* Fill the word. */
		c = (c << 8) | c;	/* u_int is 16 bits. */
#if UINT_MAX > 0xffff
		c = (c << 16) | c;	/* u_int is 32 bits. */
#endif
#if UINT_MAX > 0xffffffff
		c = (c << 32) | c;	/* u_int is 64 bits. */
#endif
	}
#endif
	/* Align destination by filling in bytes. */
	if ((t = (size_t)((u_long)dst & wmask)) != 0) {
		t = wsize - t;
		length -= t;
		do {
			*dst++ = VAL;
		} while (--t != 0);
	}

	/* Fill words.  Length was >= 2*words so we know t >= 1 here. */
	t = length / wsize;
	do {
		*(u_int *)(void *)dst = WIDEVAL;
		dst += wsize;
	} while (--t != 0);

	/* Mop up trailing bytes, if any. */
	t = length & wmask;
	if (t != 0)
		do {
			*dst++ = VAL;
		} while (--t != 0);
	RETURN;
}
#else /* __OPTIMIZE_SIZE__ */
#ifdef BZERO
void
bzero(void *dstv, size_t length)
{
	u_char *dst = dstv;
	while (length-- > 0)
		*dst++ = 0;
}
#else
void *
memset(void *dstv, int c, size_t length)
{
	u_char *dst = dstv;
	while (length-- > 0)
		*dst++ = c;
	return dstv;
}
#endif /* BZERO */
#endif /* __OPTIMIZE_SIZE__ */
